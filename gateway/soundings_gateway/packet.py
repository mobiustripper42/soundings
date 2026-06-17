"""Soundings packet v1 parser — the gateway (Python) side of the wire contract.

The decode half of `contracts/packet-v1.md` (DEC-003): the gateway receives raw
LoRa bytes and turns them into structured readings. This is an INDEPENDENT
implementation from the C++ node serializer (`firmware/src/core/packet.{h,cpp}`);
both are graded against the same shared golden vectors
(`contracts/vectors/packet-v1.json`), which is what keeps them from drifting.

Robustness is the point, not an afterthought. `parse()` is strict and raises a
`PacketError` subclass on anything malformed; `decode()` is the daemon-facing
wrapper that NEVER raises — it logs and returns None. A single bad packet (a
truncated frame, a CRC hit by noise, a node from the future) must never take down
the gateway daemon.
"""
from __future__ import annotations

import logging
import struct
from dataclasses import dataclass
from enum import Enum

log = logging.getLogger(__name__)

PROTO_V1 = 0x01
HEADER_LEN = 12
CRC_LEN = 2
CHANNEL_WIDTH = 2  # every v1 channel is 16-bit
_HEADER = struct.Struct("<BBHHHHH")  # proto, node, fw, seq, batt, chan_mask, fault_mask


class ChannelType(Enum):
    U16 = "u16"
    I16 = "i16"


# Channel registry v1 — bit -> (name, type). The gateway's copy of the contract
# (mirrors `contracts/packet-v1.md` § Channel registry); the golden vectors pin
# it. Independent of the C++ registry by design — the shared vectors catch drift.
CHANNEL_REGISTRY: dict[int, tuple[str, ChannelType]] = {
    0: ("SOIL_TENSION_0", ChannelType.U16),
    1: ("SOIL_TENSION_1", ChannelType.U16),
    2: ("SOIL_TENSION_2", ChannelType.U16),
    3: ("SOIL_TENSION_3", ChannelType.U16),
    4: ("SOIL_TEMP_0", ChannelType.I16),
    5: ("SOIL_TEMP_1", ChannelType.I16),
    6: ("AIR_TEMP", ChannelType.U16),
    7: ("AIR_RH", ChannelType.U16),
    8: ("TANK_DISTANCE", ChannelType.U16),
    9: ("SOIL_TEMP_2", ChannelType.I16),
    10: ("LEAF_WETNESS", ChannelType.U16),
    11: ("AIR_TEMP_1", ChannelType.U16),
    12: ("AIR_RH_1", ChannelType.U16),
    # bits 13-15 reserved
}
_KNOWN_MASK = 0
for _bit in CHANNEL_REGISTRY:
    _KNOWN_MASK |= 1 << _bit


class PacketError(Exception):
    """Base for all malformed-packet conditions; caught at the daemon boundary."""


class TooShort(PacketError):
    """Fewer bytes than a header + CRC."""


class UnknownProtocol(PacketError):
    """proto_ver is not one this build knows — layout is unparseable, drop it.

    This is the version byte that gates the layout. (fw_version is informational
    and does NOT gate parsing — see module note in parse().)
    """


class UnknownChannel(PacketError):
    """channel_mask declares a bit with no registry entry — the layout can't be
    sized, so the rest of the packet is unwalkable. Drop, never best-effort."""


class BadFaultMask(PacketError):
    """fault_mask is not a subset of channel_mask (faults a sensor not declared)."""


class LengthMismatch(PacketError):
    """Declared channels imply a different total length than the bytes given."""


class BadCRC(PacketError):
    """CRC-16/CCITT-FALSE check failed — corrupted in flight."""


@dataclass(frozen=True)
class Channel:
    name: str
    bit: int
    raw: int     # raw on-wire value, signed per the registry; convert to kPa/VPD/etc downstream (D1)
    fault: bool  # declared but failed to read this cycle (DEC-002) — raw is don't-care when True


@dataclass(frozen=True)
class Reading:
    proto_ver: int
    node_id: int
    fw_version: int
    seq: int
    battery_mv: int
    channel_mask: int
    fault_mask: int
    channels: tuple[Channel, ...]  # ascending bit order

    def channel(self, name: str) -> Channel | None:
        """Look up a decoded channel by registry name, or None if not present."""
        for c in self.channels:
            if c.name == name:
                return c
        return None

    def to_dict(self) -> dict:
        """JSON-friendly view — the gateway publishes this over MQTT to ingestion."""
        return {
            "proto_ver": self.proto_ver,
            "node_id": self.node_id,
            "fw_version": self.fw_version,
            "seq": self.seq,
            "battery_mv": self.battery_mv,
            "channel_mask": self.channel_mask,
            "fault_mask": self.fault_mask,
            "channels": [
                {"name": c.name, "bit": c.bit, "raw": c.raw, "fault": c.fault}
                for c in self.channels
            ],
        }

    @classmethod
    def from_dict(cls, d: dict) -> "Reading":
        """Rebuild a Reading from to_dict() output (the ingestion side of MQTT)."""
        return cls(
            proto_ver=d["proto_ver"],
            node_id=d["node_id"],
            fw_version=d["fw_version"],
            seq=d["seq"],
            battery_mv=d["battery_mv"],
            channel_mask=d["channel_mask"],
            fault_mask=d["fault_mask"],
            channels=tuple(
                Channel(name=c["name"], bit=c["bit"], raw=c["raw"], fault=c["fault"])
                for c in d["channels"]
            ),
        )


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0x0000."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def parse(data: bytes) -> Reading:
    """Strictly parse one packet. Raises a PacketError subclass on any malformed
    input; returns a fully validated, CRC-checked Reading otherwise.

    Note on versions: `proto_ver` gates the layout and an unknown one is dropped
    (UnknownProtocol). `fw_version` is informational — any value is structurally
    valid and is surfaced on the Reading; we do NOT drop on it, because dropping
    telemetry from a newer-but-layout-compatible firmware would silently blind the
    gateway to that node. Unrecognized firmware versions are a logging concern for
    the ingestion layer, not a parse failure.
    """
    if not isinstance(data, (bytes, bytearray, memoryview)):
        raise PacketError(f"expected bytes-like input, got {type(data).__name__}")
    if len(data) < HEADER_LEN + CRC_LEN:
        raise TooShort(f"{len(data)} bytes < minimum {HEADER_LEN + CRC_LEN}")

    proto, node_id, fw, seq, batt, chan_mask, fault_mask = _HEADER.unpack_from(data, 0)

    if proto != PROTO_V1:
        raise UnknownProtocol(f"proto_ver=0x{proto:02x}")
    if chan_mask & ~_KNOWN_MASK:
        raise UnknownChannel(f"channel_mask=0x{chan_mask:04x} declares bits outside the registry")
    if fault_mask & ~chan_mask:
        raise BadFaultMask(
            f"fault_mask=0x{fault_mask:04x} not a subset of channel_mask=0x{chan_mask:04x}"
        )

    bits = [b for b in range(16) if chan_mask & (1 << b)]
    total = HEADER_LEN + len(bits) * CHANNEL_WIDTH + CRC_LEN
    if len(data) != total:
        raise LengthMismatch(f"{len(data)} bytes, declared channels imply {total}")

    stored = struct.unpack_from("<H", data, total - CRC_LEN)[0]
    calc = crc16_ccitt_false(data[: total - CRC_LEN])
    if calc != stored:
        raise BadCRC(f"calc=0x{calc:04x} stored=0x{stored:04x}")

    channels = []
    off = HEADER_LEN
    for b in bits:
        name, ctype = CHANNEL_REGISTRY[b]
        fmt = "<h" if ctype is ChannelType.I16 else "<H"
        (raw,) = struct.unpack_from(fmt, data, off)
        off += CHANNEL_WIDTH
        channels.append(Channel(name=name, bit=b, raw=raw, fault=bool(fault_mask & (1 << b))))

    return Reading(proto, node_id, fw, seq, batt, chan_mask, fault_mask, tuple(channels))


def decode(data: bytes) -> Reading | None:
    """Daemon-safe decode: never raises. Logs and drops anything malformed."""
    try:
        return parse(data)
    except PacketError as e:
        log.warning("dropping malformed packet (%d bytes): %s: %s", len(data), type(e).__name__, e)
        return None
    except Exception:  # noqa: BLE001 — a bug in parse() must not crash the daemon
        log.exception("unexpected error decoding packet (%d bytes)", len(data))
        return None


def encode(
    *,
    node_id: int,
    fw_version: int,
    seq: int,
    battery_mv: int,
    channels: dict[int, int],
    fault_bits: set[int] = frozenset(),
) -> bytes:
    """Serialize a v1 packet. The gateway side needs this for the synthetic-node
    emitter (Phase 1.6) and to make the round-trip contract test symmetric — the
    Python side reproduces the exact golden bytes too, not just parses them.

    `channels` maps channel bit -> raw value (signed values accepted for i16
    channels). `fault_bits` marks declared channels that failed this cycle.
    """
    chan_mask = 0
    for b in channels:
        chan_mask |= 1 << b
    if chan_mask & ~_KNOWN_MASK:
        raise UnknownChannel(f"channel_mask=0x{chan_mask:04x} declares bits outside the registry")
    fault_mask = 0
    for b in fault_bits:
        fault_mask |= 1 << b
    if fault_mask & ~chan_mask:
        raise BadFaultMask(
            f"fault_mask=0x{fault_mask:04x} not a subset of channel_mask=0x{chan_mask:04x}"
        )

    body = bytearray(_HEADER.pack(PROTO_V1, node_id, fw_version, seq, battery_mv, chan_mask, fault_mask))
    for b in sorted(channels):  # ascending-bit wire order
        _, ctype = CHANNEL_REGISTRY[b]
        fmt = "<h" if ctype is ChannelType.I16 else "<H"
        body += struct.pack(fmt, channels[b])
    body += struct.pack("<H", crc16_ccitt_false(bytes(body)))
    return bytes(body)
