"""Serial framing v1 — the decode half of `contracts/serial-framing-v1.md`.

A serial port hands us an undelimited byte stream that contains, in no
particular order: whole packets, partial packets, and ESP32 boot chatter printed
at every board reset. This turns that into whole payloads.

It yields *candidates*, not validated packets. Whether a candidate is a real
packet is settled by the packet CRC in `packet.decode()`, which already logs and
drops the failures. Splitting it that way is what keeps this class free of an
error state it could get stuck in: every failure here is either "discard a byte"
or "wait for more bytes".
"""
from __future__ import annotations

import logging
from collections.abc import Iterator

log = logging.getLogger(__name__)

SYNC = b"\xa5\x5a"
OVERHEAD = 3  # sync (2) + length (1)

# Range-check bounds for the length byte, mirroring firmware/src/core/serial_framing.h.
#
# The minimum was 14 — a packet-v1 header and CRC with no channels — while the link ran
# one way. 3.9b added the reverse leg, where a 6-byte downlink-v1 message travels
# daemon -> board, so the smaller carried type sets the floor. That weakens the check,
# which is accepted: it was never what establishes validity, and both carried types
# carry CRC-16/CCITT-FALSE over their own bytes.
MIN_PAYLOAD = 6
MAX_PAYLOAD = 46


def encode(payload: bytes) -> bytes:
    """Wrap one payload in the envelope. The mirror of `frameForSerial` in
    `firmware/src/core/serial_framing.cpp`, and the daemon's half of the reverse leg.

    Raises ValueError outside [MIN_PAYLOAD, MAX_PAYLOAD] rather than truncating or
    emitting a partial frame — the same call the C++ encoder makes, for the same
    reason. A partial frame is worse than no frame: it desynchronises the reader for
    one frame instead of simply not existing, and the caller holding a real message
    has no way to notice. Raising rather than returning b"" because a silently empty
    write is indistinguishable, from the board's side, from an unplugged cable.
    """
    if not MIN_PAYLOAD <= len(payload) <= MAX_PAYLOAD:
        raise ValueError(
            f"payload of {len(payload)} bytes is outside the framed range "
            f"[{MIN_PAYLOAD}, {MAX_PAYLOAD}]"
        )
    return SYNC + bytes([len(payload)]) + payload


class SerialFramer:
    """Feed it bytes, get whole payloads out."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.discarded = 0   # bytes thrown away hunting for a frame; a resync counter

    def feed(self, chunk: bytes) -> Iterator[bytes]:
        """Append `chunk` and yield every whole payload it completed.

        Follows the five steps in the contract literally. Every path either consumes
        bytes or returns to wait for more, so there is no state this can wedge in.
        """
        self._buf += chunk

        while True:
            i = self._buf.find(SYNC)
            if i < 0:
                # No sync in hand. Discard everything EXCEPT a possible trailing 0xA5 —
                # it may be the first half of a sync whose second byte hasn't arrived,
                # and dropping it would lose the frame behind it.
                keep = 1 if self._buf.endswith(SYNC[:1]) else 0
                self.discarded += len(self._buf) - keep
                del self._buf[: len(self._buf) - keep]
                return

            if i:
                self.discarded += i
                del self._buf[:i]

            if len(self._buf) < OVERHEAD:
                return  # sync found, length byte not here yet

            length = self._buf[2]
            if not (MIN_PAYLOAD <= length <= MAX_PAYLOAD):
                # Not a frame header: boot text, or payload bytes that happened to look
                # like one. Discard exactly ONE byte, not the whole sync pair — dropping
                # both would skip the real sync in `A5 A5 5A ...`.
                log.debug("serial framing: length %d out of range, resyncing", length)
                self.discarded += 1
                del self._buf[:1]
                continue

            if len(self._buf) < OVERHEAD + length:
                return  # payload still arriving; hold it rather than drop it

            payload = bytes(self._buf[OVERHEAD : OVERHEAD + length])
            del self._buf[: OVERHEAD + length]
            # A candidate, not a verified packet. packet.decode() owns the CRC, and
            # already logs and drops what fails it.
            yield payload
