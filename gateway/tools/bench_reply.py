"""Bench instrument: answer every packet immediately and time the round trip.

Measures the one thing DEC-010 says has never been measured — *"the gateway->node
direction has never been measured ... the first thing bench step 6 should establish
after a packet decodes is that a reply reaches the node."*

    daemon reads packet -> decides -> writes downlink -> board -> LoRa -> node's window

This end can only time its own half (packet-in to downlink-out). Whether the reply
landed while the window was still open is a question only the NODE can answer, so
watch its serial monitor for the `DOWNLINK heard` line the bench build prints.

Run it against the gateway board while a bench node is transmitting:

    gateway/.venv/bin/python -m tools.bench_reply --port /dev/ttyUSB0

stdlib only — no pyserial. `SerialPacketSource` and `SerialDownlinkSink` take their
streams structurally (a `.read(n)` and a `.write(b)`), which a raw tty file object
satisfies directly. That was the point of declaring them as protocols.
"""
from __future__ import annotations

import argparse
import fcntl
import logging
import os
import struct
import sys
import termios
import time
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from soundings_gateway.downlink import FLAGS_NONE, SerialDownlinkSink
from soundings_gateway.ota import UpdatePolicy
from soundings_gateway.gateway import Gateway
from soundings_gateway.source import SerialPacketSource

log = logging.getLogger("bench_reply")

BAUD = termios.B115200

# Modem-control ioctl and bits (linux/termios.h). Not exposed by the termios module, and
# small enough not to be worth a dependency — see the comment in open_port for why they
# are load-bearing rather than hygiene.
TIOCMBIC = 0x5417       # clear the given modem bits
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004


def open_port(path: str, read_timeout_ds: int = 1):
    """Open a tty raw at 115200, returning a buffered binary file object.

    `read_timeout_ds` is VTIME, in TENTHS of a second, with VMIN=0 — so read() returns
    whatever has arrived within that window, or b"" if nothing did. b"" is exactly what
    SerialPacketSource treats as "quiet, keep going" rather than end-of-stream.

    ⚠ Kept SHORT (0.1 s) on purpose. This timeout is dead latency on the reply path: a
    packet that lands just after a read returns waits the whole timeout before the next
    one sees it, and that delay is spent inside the node's receive window.
    """
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)

    # ⚠ DEASSERT DTR AND RTS BEFORE ANYTHING ELSE, and this is not boilerplate.
    #
    # On these boards the CP2102's DTR and RTS drive the ESP32's EN and GPIO0 through the
    # usual auto-reset transistor pair. Opening the port asserts both by default, which is
    # the "hold GPIO0 low across a reset" combination — i.e. it parks the chip in the ROM
    # download bootloader, where it runs no firmware and says nothing.
    #
    # The symptom is brutal to read: the port opens, termios reports settings byte-for-byte
    # identical to a working `stty raw` setup, reads return cleanly on schedule, and every
    # one of them is empty. Not even the boot banner arrives, because there is no boot.
    # It looks exactly like a radio that failed, a node that stopped transmitting, or a
    # framing bug — and it cost a bench cycle here before the missing banner gave it away.
    #
    # `cat` does not hit this because the shell leaves the modem lines where it found them.
    # pyserial handles it for you; this is the price of not taking the dependency, and it
    # is worth paying once, in a comment, rather than rediscovering.
    fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_DTR | TIOCM_RTS))

    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs

    # Raw: no canonical mode, no echo, no signal chars, no CR/LF translation, no flow
    # control. Any of those would mangle binary packet bytes — 0x11/0x13 are XON/XOFF.
    iflag &= ~(termios.IXON | termios.IXOFF | termios.IXANY | termios.ICRNL
               | termios.INLCR | termios.IGNCR | termios.ISTRIP | termios.INPCK
               | termios.BRKINT)
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ICANON | termios.ECHO | termios.ECHOE | termios.ISIG)
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL

    cc = list(cc)
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = read_timeout_ds

    termios.tcsetattr(fd, termios.TCSANOW,
                      [iflag, oflag, cflag, lflag, BAUD, BAUD, cc])
    # Discard whatever the board said before we were listening — boot chatter, and the
    # tail of a frame we opened the port in the middle of.
    termios.tcflush(fd, termios.TCIOFLUSH)
    return os.fdopen(fd, "r+b", buffering=0)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", default="/dev/ttyUSB0", help="the GATEWAY board, not the node")
    p.add_argument("--flags", type=lambda s: int(s, 0), default=FLAGS_NONE,
                   help="downlink flags to send (default 0 — 'heard, nothing for you')")
    p.add_argument("--ota-dir", default=None, type=Path,
                   help="serve OTA decisions from this manifest directory instead of "
                        "sending a fixed --flags value")
    p.add_argument("--ota-pubkey", default=None,
                   help="fleet signing public key, 64 hex chars — the same value as "
                        "SOUNDINGS_OTA_PUBKEY in firmware/platformio.ini. Required with "
                        "--ota-dir; there is no unverified mode (DEC-013)")
    p.add_argument("--count", type=int, default=0, help="stop after N packets (0 = forever)")
    p.add_argument("--log-level", default="INFO")
    args = p.parse_args()
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(message)s")

    port = open_port(args.port)
    sink = SerialDownlinkSink(port)
    latencies: list[float] = []
    # With --ota-dir the flags are DERIVED from the manifest and the node's reported
    # fw_version, which is the real path; --flags is the fixed-value bench instrument.
    policy = None
    if args.ota_dir:
        if not args.ota_pubkey:
            p.error("--ota-dir requires --ota-pubkey; the bench must verify what the "
                    "field verifies, or it is not proving the field path")
        policy = UpdatePolicy(args.ota_dir, bytes.fromhex(args.ota_pubkey))

    def respond(msg: dict) -> None:
        """Reply the instant a packet decodes. No policy — the OTA version compare
        lands later; right now the question is purely whether a reply fits the window."""
        flags = policy.flags_for(msg) if policy else args.flags
        t0 = time.perf_counter()
        ok = sink.send(msg["node_id"], flags)
        dt = (time.perf_counter() - t0) * 1000.0
        latencies.append(dt)
        log.info("node %s seq %s fw %s -> downlink flags=0x%04x %s in %.1f ms",
                 msg["node_id"], msg.get("seq"), msg.get("fw_version"), flags,
                 "sent" if ok else "FAILED", dt)
        if args.count and len(latencies) >= args.count:
            raise KeyboardInterrupt

    gw = Gateway(SerialPacketSource(port), lambda _m: None, respond=respond)
    log.info("listening on %s — watch the NODE's monitor for 'DOWNLINK heard'", args.port)
    try:
        gw.run()
    except KeyboardInterrupt:
        pass
    finally:
        port.close()

    if latencies:
        log.info("packets %d | downlinks sent %d failed %d | daemon-side write "
                 "min %.1f / mean %.1f / max %.1f ms",
                 gw.decoded, sink.sent, sink.failed,
                 min(latencies), sum(latencies) / len(latencies), max(latencies))
    else:
        log.warning("no packets decoded — is a bench node transmitting, and is this the "
                    "gateway board rather than the node?")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
