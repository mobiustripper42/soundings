#!/usr/bin/env python3
"""Golden-vector generator for Soundings packet v1 (contract D2).

FIXTURE AUTHORING ONLY — this is NOT the gateway parser (that lands independently
in Phase 1.4, written against the spec + the JSON this emits). Keeping the
generator standalone preserves the two-independent-implementations property: the
C++ serializer (1.3) and the Python parser (1.4) are each checked against the
emitted vectors, not against this code.

Run from the repo root:  python3 contracts/tools/gen_vectors.py
It rewrites contracts/vectors/packet-v1.json in place. Re-run whenever the
channel registry grows (adding a sensor = a new bit + a registry row here),
which is exactly the "survives adding sensor fields later" requirement.

The wire format is pinned in contracts/packet-v1.md; this file is the executable
mirror of that spec.
"""
import json
import struct
from pathlib import Path

PROTO_V1 = 0x01

# --- Channel registry v1 -----------------------------------------------------
# (bit, name, struct_fmt). bit = position in channel_mask / fault_mask.
# All v1 channels are 16-bit; the registry carries per-channel width so a future
# channel can differ. Order here defines on-wire order (ascending bit).
CHANNELS = [
    (0,  "SOIL_TENSION_0", "<H"),  # Watermark, raw resistance, 0.1 kΩ/LSB
    (1,  "SOIL_TENSION_1", "<H"),
    (2,  "SOIL_TENSION_2", "<H"),
    (3,  "SOIL_TENSION_3", "<H"),
    (4,  "SOIL_TEMP_0",    "<h"),  # DS18B20, raw, 1/16 °C/LSB, signed
    (5,  "SOIL_TEMP_1",    "<h"),
    (6,  "AIR_TEMP",       "<H"),  # SHT45 raw ticks: T = -45 + 175*ticks/65535
    (7,  "AIR_RH",         "<H"),  # SHT45 raw ticks: RH = -6 + 125*ticks/65535
    (8,  "TANK_DISTANCE",  "<H"),  # A02YYUW raw distance, 1 mm/LSB
    (9,  "SOIL_TEMP_2",    "<h"),  # surface DS18B20 (stretch)
    (10, "LEAF_WETNESS",   "<H"),  # raw grid reading (stretch)
    (11, "AIR_TEMP_1",     "<H"),  # 2nd SHT45 (stratification rig, stretch)
    (12, "AIR_RH_1",       "<H"),
    # bits 13-15 reserved for future sensors
]
FMT_BY_NAME = {name: fmt for _, name, fmt in CHANNELS}
BIT_BY_NAME = {name: bit for bit, name, _ in CHANNELS}


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def encode(node_id, fw_version, seq, battery_mv, channels, faults=()):
    """channels: list of (name, raw_value) in any order; sorted to wire order.
    faults: iterable of channel names whose reading failed this cycle."""
    chan_mask = 0
    fault_mask = 0
    by_name = {}
    for name, val in channels:
        chan_mask |= 1 << BIT_BY_NAME[name]
        by_name[name] = val
    for name in faults:
        fault_mask |= 1 << BIT_BY_NAME[name]

    body = struct.pack("<BBHHHHH", PROTO_V1, node_id, fw_version, seq,
                       battery_mv, chan_mask, fault_mask)
    for bit, name, fmt in CHANNELS:          # ascending-bit wire order
        if chan_mask & (1 << bit):
            body += struct.pack(fmt, by_name[name])
    crc = crc16_ccitt_false(body)
    packet = body + struct.pack("<H", crc)
    return packet, chan_mask, fault_mask, crc


def vec(name, description, node_id, fw_version, seq, battery_mv,
        channels, faults=()):
    packet, chan_mask, fault_mask, crc = encode(
        node_id, fw_version, seq, battery_mv, channels, faults)
    return {
        "name": name,
        "description": description,
        "fields": {
            "proto_ver": PROTO_V1,
            "node_id": node_id,
            "fw_version": fw_version,
            "seq": seq,
            "battery_mv": battery_mv,
            "channel_mask": chan_mask,
            "fault_mask": fault_mask,
            "channels": [
                {"name": n, "raw": v, "fault": n in faults} for n, v in
                sorted(channels, key=lambda c: BIT_BY_NAME[c[0]])
            ],
        },
        "expected": {
            "hex": packet.hex(),
            "len": len(packet),
            "crc16": f"0x{crc:04X}",
        },
    }


VECTORS = [
    vec("tank_node_nominal",
        "Tank-level node: one ultrasonic channel. The minimal real packet.",
        node_id=10, fw_version=100, seq=1, battery_mv=3700,
        channels=[("TANK_DISTANCE", 1234)]),

    vec("bed_node_nominal",
        "Bed node: 2 Watermarks (6\"/12\") + 2 DS18B20 + SHT45 (T,RH). Typical.",
        node_id=1, fw_version=100, seq=42, battery_mv=3850,
        channels=[("SOIL_TENSION_0", 152), ("SOIL_TENSION_1", 318),
                  ("SOIL_TEMP_0", 296), ("SOIL_TEMP_1", 272),
                  ("AIR_TEMP", 26869), ("AIR_RH", 39000)]),

    vec("bed_node_one_fault",
        "Bed node where the 12\" soil-temp probe failed to read: declared (in "
        "channel_mask) AND flagged in fault_mask, value bytes still present. "
        "A fault, not a silent gap (DEC-002).",
        node_id=1, fw_version=100, seq=43, battery_mv=3840,
        channels=[("SOIL_TENSION_0", 155), ("SOIL_TENSION_1", 320),
                  ("SOIL_TEMP_0", 298), ("SOIL_TEMP_1", 0),
                  ("AIR_TEMP", 26900), ("AIR_RH", 38800)],
        faults=["SOIL_TEMP_1"]),

    vec("bed_node_max_watermarks",
        "4-Watermark bed node (commercial+homemade pairs) + 2 DS18B20 + SHT45 "
        "= 8 channels. The top of the ~30-byte size budget.",
        node_id=2, fw_version=101, seq=7, battery_mv=4050,
        channels=[("SOIL_TENSION_0", 140), ("SOIL_TENSION_1", 305),
                  ("SOIL_TENSION_2", 148), ("SOIL_TENSION_3", 311),
                  ("SOIL_TEMP_0", 290), ("SOIL_TEMP_1", 268),
                  ("AIR_TEMP", 27000), ("AIR_RH", 40000)]),

    vec("negative_soil_temp",
        "Sub-zero soil temp exercises the signed i16 encoding: -80 = -5.0 °C.",
        node_id=3, fw_version=100, seq=900, battery_mv=3600,
        channels=[("SOIL_TENSION_0", 600), ("SOIL_TEMP_0", -80)]),

    vec("seq_wrap_battery_unknown",
        "Edge sentinels: seq at u16 max (about to wrap) and battery 0xFFFF "
        "(unknown/unread). Tank node.",
        node_id=10, fw_version=100, seq=0xFFFF, battery_mv=0xFFFF,
        channels=[("TANK_DISTANCE", 305)]),

    vec("all_channels_registry_coverage",
        "Every v1 channel declared at once — registry parse-order coverage. "
        "Exceeds the typical LoRa size budget; a parser test, not a real node.",
        node_id=200, fw_version=255, seq=12345, battery_mv=3777,
        channels=[(name, 1000 + bit) for bit, name, _ in CHANNELS]),
]


def main():
    out = {
        "format": "soundings-packet",
        "proto_ver": PROTO_V1,
        "encoding": {
            "byte_order": "little-endian",
            "crc": "CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF, refin=false, "
                   "refout=false, xorout=0x0000), over all bytes preceding the "
                   "CRC field, appended little-endian.",
            "header_fields": ["proto_ver:u8", "node_id:u8", "fw_version:u16",
                              "seq:u16", "battery_mv:u16", "channel_mask:u16",
                              "fault_mask:u16"],
        },
        "channel_registry": [
            {"bit": bit, "name": name,
             "type": "i16" if fmt == "<h" else "u16"}
            for bit, name, fmt in CHANNELS
        ],
        "vectors": VECTORS,
    }
    dest = Path(__file__).resolve().parent.parent / "vectors" / "packet-v1.json"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(out, indent=2) + "\n")
    print(f"wrote {len(VECTORS)} vectors -> {dest}")
    for v in VECTORS:
        print(f"  {v['name']:32s} {v['expected']['len']:2d} B  "
              f"{v['expected']['hex']}")


if __name__ == "__main__":
    main()
