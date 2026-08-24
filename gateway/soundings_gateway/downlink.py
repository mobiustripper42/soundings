"""Downlink v1 — the encode half of `contracts/downlink-v1.md`.

Six bytes the daemon may send into the receive window a node holds after each
transmit (DEC-010). The node decodes them; `firmware/src/core/downlink.{h,cpp}`
is that side, written independently, the same arrangement packet-v1 uses.

The daemon decides *what* to send; the gateway board only relays. So a downlink
travels daemon -> USB serial (framed per `contracts/serial-framing-v1.md`, in the
reverse direction) -> board -> LoRa.
"""
from __future__ import annotations

import struct

from .packet import crc16_ccitt_false

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
