"""Downlink v1 — the encode half of `contracts/downlink-v1.md`.

Six bytes the daemon may send into the receive window a node holds after each
transmit (DEC-010). The node decodes them; `firmware/src/core/downlink.{h,cpp}`
is that side, written independently, the same arrangement packet-v1 uses.

The daemon decides *what* to send; the gateway board only relays. So a downlink
travels daemon -> USB serial (framed per `contracts/serial-framing-v1.md`, in the
reverse direction) -> board -> LoRa.
"""
from __future__ import annotations

import logging
import struct
from typing import Protocol

from . import framing
from .packet import crc16_ccitt_false

log = logging.getLogger(__name__)

PROTO_V1 = 0x01
DOWNLINK_LEN = 6

#: No bits are assigned in v1. A downlink with `flags == 0` means "you are heard,
#: and there is nothing for you" — which is the only thing v1 can say. Issue #76
#: assigns the first bit when it has something to say with it.
FLAGS_NONE = 0x0000

_BODY = struct.Struct("<BBH")   # proto_ver, node_id, flags


def encode(node_id: int, flags: int = FLAGS_NONE) -> bytes:
    """Build one downlink. Raises on anything that would not fit its field.

    Raising rather than truncating: a node_id silently wrapped to 8 bits would
    address the wrong node, and every rejection on the node side is silent, so a
    misaddressed downlink would look exactly like a quiet window. That is
    unfalsifiable from the daemon's end, which is the wrong place to be lenient.
    """
    if not 0 <= node_id <= 0xFF:
        raise ValueError(f"node_id {node_id} does not fit u8")
    if not 0 <= flags <= 0xFFFF:
        raise ValueError(f"flags {flags:#x} does not fit u16")

    body = _BODY.pack(PROTO_V1, node_id, flags)
    return body + struct.pack("<H", crc16_ccitt_false(body))


class WritableStream(Protocol):
    """Anything that takes bytes. `serial.Serial` satisfies it.

    Structural, and for the same reason `source.ByteStream` is: it keeps pyserial
    out of this module, so the framing and the survives-a-dead-port behaviour are
    testable with a scripted writer and the gateway's dependency list is unchanged.
    The real port is constructed at the edge.
    """

    def write(self, data: bytes) -> int: ...


class SerialDownlinkSink:
    """Puts a downlink on the wire: encode -> frame -> write.

    Both legs of the contract in one place, because they are never useful apart —
    a bare downlink on the serial link is unreadable to the board, and the board is
    the only thing this ever writes to.
    """

    def __init__(self, stream: WritableStream):
        self.stream = stream
        self.sent = 0
        self.failed = 0

    def send(self, node_id: int, flags: int = FLAGS_NONE) -> bool:
        """Frame one downlink and write it. Returns whether the port took it.

        **A write failure is caught; a bad argument is not.** They are different
        kinds of wrong. A port that went away is the environment misbehaving and the
        daemon has to keep decoding packets through it, so it is logged and counted.
        An out-of-range node_id is this process being wrong about what it is doing —
        and since every node silently ignores a downlink addressed elsewhere, a
        misaddressed one is indistinguishable from a quiet window. Swallowing that
        would make it unfalsifiable from the only end that can still see it.
        """
        frame = framing.encode(encode(node_id, flags))   # raises on a bad argument
        try:
            self.stream.write(frame)
        except Exception:  # noqa: BLE001 — a dead port must not stop the decode loop
            self.failed += 1
            log.exception("downlink write failed (node %d, flags %#06x)", node_id, flags)
            return False
        self.sent += 1
        return True
