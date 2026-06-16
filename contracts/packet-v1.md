# Soundings packet v1 — wire contract (D2)

The binary packet is **the contract** between the node firmware (C++ serializer,
Phase 1.3) and the gateway (Python parser, Phase 1.4). Neither side may drift:
both are pinned to the shared golden vectors in
[`vectors/packet-v1.json`](vectors/packet-v1.json), authored by
[`tools/gen_vectors.py`](tools/gen_vectors.py).

This document is the human-readable spec; the JSON is the machine-checkable
mirror. If they ever disagree, that is a bug — fix it before shipping either
implementation. **This resolves §12 D2.**

## Design goals

- **~20–30 bytes** for a real node (tank node 16 B, typical bed node 26 B,
  4-Watermark bed node 30 B). LoRa airtime is the budget.
- **Raw values on the wire (D1).** Resistance, ticks, raw distance, raw temp
  counts — never computed kPa/VPD/gallons. The math stays re-revisable against
  stored raw data without reflashing.
- **Manifest-aware (DEC-002).** The packet self-describes which sensor channels a
  node declares, so the layout is a node's sensor set, not a fixed superset. A
  declared sensor that fails to read is an explicit **fault**, never a silent gap.
- **Future-proof.** Adding a sensor is a new channel bit + registry row; existing
  vectors stay valid (see *Versioning*).

## Byte order

**Little-endian** for every multi-byte field. The ESP32-S3 is little-endian, so
the serializer does no byte-swapping on the hot path. The gateway parser must
read little-endian explicitly.

## Layout

A packet is a fixed **header**, then the **declared channel values** in ascending
bit order, then a **CRC**.

### Header (14 bytes, always present)

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| 0 | `proto_ver` | u8 | Packet-format version. **`0x01`** for v1. Distinct from `fw_version`. A parser rejects/branches on this before reading anything else. |
| 1 | `node_id` | u8 | 1–254. `0` reserved, `255` reserved (broadcast/unset). |
| 2 | `fw_version` | u16 | Firmware build version. **Every packet carries it** — lets the gateway attribute decode quirks to a firmware. |
| 4 | `seq` | u16 | Per-node sequence counter, wraps at 0xFFFF. Gateway uses it for dedup + loss detection. |
| 6 | `battery_mv` | u16 | Battery voltage in millivolts. `0xFFFF` = unknown/unread (battery never legitimately reaches 65.535 V). |
| 8 | `channel_mask` | u16 | Bit *i* set ⇒ channel *i* (per the registry) is **declared** by this node and its value bytes are present below. This is the node's manifest on the wire. |
| 10 | `fault_mask` | u16 | Subset of `channel_mask`. Bit *i* set ⇒ channel *i* is declared **but failed to read this cycle**. Its value bytes are still present (so layout depends only on `channel_mask`) but must be treated as invalid. |
| 12 | *(channel values)* | — | See below. |

### Channel values (variable)

For each bit set in `channel_mask`, **in ascending bit order**, the channel's
value is appended at its registry width. Layout is a pure function of
`channel_mask` — a faulted channel still occupies its bytes, so a parser can walk
the packet knowing only the mask and the registry. `fault_mask` is pure metadata
layered on top.

### CRC (2 bytes, trailing)

| Field | Type | Notes |
|-------|------|-------|
| `crc16` | u16 | **CRC-16/CCITT-FALSE** over **all preceding bytes** (header + channel values), appended little-endian. |

**CRC parameters (pin these exactly — CRC ambiguity is the classic interop bug):**
`width=16, poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0x0000`.
A parser that fails the CRC **logs and drops** the packet — never crashes, never
ingests partial data.

## Channel registry v1

All v1 channels are 16-bit; the registry carries a per-channel width so a future
channel can differ. Bit position is fixed forever once assigned.

| Bit | Channel | Type | Raw encoding |
|-----|---------|------|--------------|
| 0 | `SOIL_TENSION_0` | u16 | Watermark raw resistance, **0.1 kΩ/LSB** (0–6553.4 kΩ). Conversion to kPa (temp-compensated, SPEC §5.1) happens downstream (D1). |
| 1 | `SOIL_TENSION_1` | u16 | second Watermark (e.g. 12") |
| 2 | `SOIL_TENSION_2` | u16 | third (homemade pair) |
| 3 | `SOIL_TENSION_3` | u16 | fourth |
| 4 | `SOIL_TEMP_0` | i16 | DS18B20 raw, **1/16 °C/LSB**, signed (native DS18B20 format) |
| 5 | `SOIL_TEMP_1` | i16 | second DS18B20 (e.g. 12") |
| 6 | `AIR_TEMP` | u16 | SHT45 raw ticks; `T_°C = -45 + 175·ticks/65535` |
| 7 | `AIR_RH` | u16 | SHT45 raw ticks; `RH_% = -6 + 125·ticks/65535` |
| 8 | `TANK_DISTANCE` | u16 | A02YYUW raw distance, **1 mm/LSB**. Gallons/percent derived downstream (SPEC §5.4). |
| 9 | `SOIL_TEMP_2` | i16 | surface DS18B20 *(stretch)* |
| 10 | `LEAF_WETNESS` | u16 | raw grid reading *(stretch)* |
| 11 | `AIR_TEMP_1` | u16 | second SHT45 (stratification rig) *(stretch)* |
| 12 | `AIR_RH_1` | u16 | second SHT45 RH *(stretch)* |
| 13–15 | *reserved* | — | future sensors |

**Why raw resistance, not ADC counts, for the Watermark.** The truly-raw value is
ADC counts, but the ADC→resistance step depends on the AC-excitation circuit,
whose v2 design is deferred (§12 D11). Resistance (kΩ) is the lowest-level value
that is *circuit-independent* and matches what SPEC §5.1 calls the durable record
("log raw resistance"). The temperature-compensated kPa conversion — the part
that genuinely benefits from re-revisability — stays downstream. Revisit if the
bench circuit makes ADC counts worth carrying.

The 0.1 kΩ/LSB encoding saturates at 6553.4 kΩ (`0xFFFE`). Watermark resistance
stays well under that across the irrigation-decision range (it gets noisy at the
dry end anyway, SPEC §5.1), so the ceiling is comfortable. `0xFFFF` is left free
as a possible future over-range marker — don't emit it as a real reading.

## Fault vs. absent (DEC-002)

Three distinct states for any channel, all unambiguous on the wire:

| State | `channel_mask` bit | `fault_mask` bit | Value bytes |
|-------|:--:|:--:|:--:|
| Not declared (node lacks this sensor) | 0 | 0 | absent |
| Declared, read OK | 1 | 0 | present, valid |
| Declared, read **failed** this cycle | 1 | 1 | present, **invalid** |

A node that *should* have a sensor but got no reading emits state 3 — an
actionable fault the gateway can alert on — rather than silently dropping the
field (which would be indistinguishable from "node doesn't have that sensor").

**A faulted channel's value bytes are don't-care.** The serializer may write
anything (the fixtures write `0`); the parser **must ignore the value whenever
the fault bit is set** and must not treat any particular value as a sentinel.
Faults live in `fault_mask`, never in the value — so the full u16/i16 range stays
available for real readings on channel types (SHT45 ticks, tank mm) that have no
spare sentinel value.

## Versioning

- **`proto_ver` bumps** only on a layout-incompatible change: header field
  changes, CRC change, endianness change, or a channel width change. An old
  parser rejects a packet whose `proto_ver` it doesn't know.
- **Adding a sensor does *not* bump `proto_ver`.** Assign the next free
  `channel_mask` bit + a registry row in `gen_vectors.py`, regenerate the
  vectors, add new fixtures. Existing vectors stay byte-identical. Because the
  gateway is updated centrally (and ships with the registry), node and gateway
  move together; an updated gateway parses both old and new packets.
- Bit positions are **permanent** once assigned — never reuse a retired bit.

**Forward-compat assumption (load-bearing).** Because layout depends on
`channel_mask` + the registry, a parser that meets a set bit it doesn't recognize
knows a value is present but **not its width** — it cannot compute the remaining
offsets. So forward-compat rests on one operational rule: **the gateway's
registry is always a superset of every deployed node's registry** — update the
gateway *before* flashing any node that declares a new channel (trivial given
central gateway + USB-flash service window, SPEC §8). A parser that nonetheless
encounters a set bit outside its registry **MUST drop the packet** (log it as an
unknown-channel/unparseable frame); it must never best-effort parse, which would
silently misread every trailing channel and the CRC. This turns a corruption
mode into a defined, safe drop.

## Golden vectors

[`vectors/packet-v1.json`](vectors/packet-v1.json) holds named cases, each with
the decoded `fields` and the `expected` hex/length/CRC. Both implementations must
round-trip every vector: **fields → serialize → bytes** must equal `hex`, and
**bytes → parse → fields** must equal `fields`. Coverage includes the minimal
tank packet, a typical and a maximal bed node, a fault case, signed/sub-zero soil
temp, edge sentinels (seq wrap, unknown battery), and full-registry parse-order
coverage.

Regenerate after any registry change:

```bash
python3 contracts/tools/gen_vectors.py
```
