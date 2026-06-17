"""Packet v1 parser tests — graded against the SHARED golden vectors
(contracts/vectors/packet-v1.json), the same fixtures the C++ node serializer is
checked against. That shared answer key is what guarantees the two sides can't
drift. Plus the malformed-input robustness suite, which doubles as the daemon's
robustness spec (carried to Phase 3.1).
"""
from __future__ import annotations

import json
import logging
import struct
from pathlib import Path

import pytest

from soundings_gateway import packet
from soundings_gateway.packet import (
    BadCRC,
    BadFaultMask,
    Channel,
    LengthMismatch,
    Reading,
    TooShort,
    UnknownChannel,
    UnknownProtocol,
    decode,
    encode,
    parse,
)

VECTORS_PATH = Path(__file__).resolve().parents[2] / "contracts" / "vectors" / "packet-v1.json"


def _load():
    data = json.loads(VECTORS_PATH.read_text())
    bit_by_name = {c["name"]: c["bit"] for c in data["channel_registry"]}
    return data["vectors"], bit_by_name


VECTORS, BIT_BY_NAME = _load()


def _channels_and_faults(fields):
    channels = {BIT_BY_NAME[c["name"]]: c["raw"] for c in fields["channels"]}
    faults = {BIT_BY_NAME[c["name"]] for c in fields["channels"] if c["fault"]}
    return channels, faults


@pytest.mark.parametrize("vec", VECTORS, ids=[v["name"] for v in VECTORS])
def test_parse_matches_fixture(vec):
    """bytes -> parse -> structured Reading equals the fixture's decoded fields."""
    f = vec["fields"]
    raw = bytes.fromhex(vec["expected"]["hex"])
    r = parse(raw)

    assert r.proto_ver == f["proto_ver"]
    assert r.node_id == f["node_id"]
    assert r.fw_version == f["fw_version"]
    assert r.seq == f["seq"]
    assert r.battery_mv == f["battery_mv"]
    assert r.channel_mask == f["channel_mask"]
    assert r.fault_mask == f["fault_mask"]

    expected = [
        Channel(name=c["name"], bit=BIT_BY_NAME[c["name"]], raw=c["raw"], fault=c["fault"])
        for c in f["channels"]
    ]
    assert list(r.channels) == expected


@pytest.mark.parametrize("vec", VECTORS, ids=[v["name"] for v in VECTORS])
def test_encode_matches_fixture(vec):
    """fields -> encode -> bytes equals the exact golden hex (Python is byte-exact too)."""
    f = vec["fields"]
    channels, faults = _channels_and_faults(f)
    raw = encode(
        node_id=f["node_id"],
        fw_version=f["fw_version"],
        seq=f["seq"],
        battery_mv=f["battery_mv"],
        channels=channels,
        fault_bits=faults,
    )
    assert raw.hex() == vec["expected"]["hex"]
    assert len(raw) == vec["expected"]["len"]


@pytest.mark.parametrize("vec", VECTORS, ids=[v["name"] for v in VECTORS])
def test_roundtrip(vec):
    """encode -> parse reproduces the same Reading as parsing the golden bytes."""
    f = vec["fields"]
    channels, faults = _channels_and_faults(f)
    built = encode(
        node_id=f["node_id"],
        fw_version=f["fw_version"],
        seq=f["seq"],
        battery_mv=f["battery_mv"],
        channels=channels,
        fault_bits=faults,
    )
    assert parse(built) == parse(bytes.fromhex(vec["expected"]["hex"]))


def _good_tank() -> bytes:
    """tank_node_nominal, built fresh — the base for mutation tests."""
    return encode(node_id=10, fw_version=100, seq=1, battery_mv=3700, channels={8: 1234})


def test_crc_verified_on_good_packet():
    r = parse(_good_tank())
    assert r.channel("TANK_DISTANCE").raw == 1234
    assert r.fw_version == 100


# --- malformed input: each must raise the right error, and decode() must absorb it ---

def test_truncated_raises():
    with pytest.raises(TooShort):
        parse(_good_tank()[:8])  # header only partial, well under header+CRC


def test_bad_crc_raises():
    buf = bytearray(_good_tank())
    buf[12] ^= 0xFF  # corrupt a payload byte; CRC no longer matches
    with pytest.raises(BadCRC):
        parse(bytes(buf))


def test_unknown_protocol_raises():
    buf = bytearray(_good_tank())
    buf[0] = 0x02  # not v1
    with pytest.raises(UnknownProtocol):
        parse(bytes(buf))


def test_unknown_channel_raises():
    # Hand-craft a packet declaring reserved bit 13 (no registry entry).
    body = bytearray(packet._HEADER.pack(packet.PROTO_V1, 5, 0, 0, 0, 1 << 13, 0))
    body += b"\x00\x00"  # a 2-byte "value" so the length looks plausible
    body += packet.crc16_ccitt_false(bytes(body)).to_bytes(2, "little")
    with pytest.raises(UnknownChannel):
        parse(bytes(body))


def test_bad_fault_mask_raises():
    # Declare only TANK_DISTANCE (bit 8) but flag SOIL_TENSION_0 (bit 0) as faulted.
    body = bytearray(packet._HEADER.pack(packet.PROTO_V1, 5, 0, 0, 0, 1 << 8, 1 << 0))
    body += struct.pack("<H", 1234)
    body += packet.crc16_ccitt_false(bytes(body)).to_bytes(2, "little")
    with pytest.raises(BadFaultMask):
        parse(bytes(body))


def test_length_mismatch_raises():
    buf = _good_tank() + b"\x00"  # trailing junk / radio padding
    with pytest.raises(LengthMismatch):
        parse(buf)


def test_decode_never_raises_and_logs(caplog):
    """The daemon boundary: malformed input returns None and logs, never raises."""
    bad = bytearray(_good_tank())
    bad[12] ^= 0xFF
    with caplog.at_level(logging.WARNING):
        assert decode(bytes(bad)) is None
        assert decode(b"\x01\x02") is None  # far too short
    assert any("dropping malformed packet" in rec.message for rec in caplog.records)


def test_decode_returns_reading_on_good_input():
    r = decode(_good_tank())
    assert isinstance(r, Reading)
    assert r.node_id == 10


def test_empty_channel_mask_packet():
    """A header-only packet (channel_mask=0, 14 bytes) is structurally valid."""
    raw = encode(node_id=7, fw_version=100, seq=5, battery_mv=3700, channels={})
    assert len(raw) == packet.HEADER_LEN + packet.CRC_LEN  # 14
    r = parse(raw)
    assert r.channel_mask == 0
    assert r.channels == ()
    assert r.node_id == 7


def test_decode_absorbs_non_packeterror(caplog, monkeypatch):
    """Even a non-PacketError bug in parse() must not crash the daemon."""
    def boom(_):
        raise ValueError("synthetic parse bug")

    monkeypatch.setattr(packet, "parse", boom)
    with caplog.at_level(logging.ERROR):
        assert decode(_good_tank()) is None
    assert any("unexpected error decoding packet" in rec.message for rec in caplog.records)
