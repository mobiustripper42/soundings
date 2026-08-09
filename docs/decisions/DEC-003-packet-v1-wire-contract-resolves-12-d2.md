---
id: DEC-003
title: "Packet v1 wire contract (resolves §12 D2)"
topic: "Radio, wire contract & gateway"
---

## DEC-003: Packet v1 wire contract (resolves §12 D2)

**Decision:** The node→gateway binary packet is a **12-byte fixed little-endian
header** — `proto_ver:u8`, `node_id:u8`, `fw_version:u16`, `seq:u16`,
`battery_mv:u16`, `channel_mask:u16`, `fault_mask:u16` — followed by the
**declared channel values in ascending channel-bit order**, then a trailing
**CRC-16/CCITT-FALSE** (little-endian). A 16-slot **channel registry** maps each
bit to a raw sensor channel (raw values per D1). The full contract, registry, and
versioning policy live in `contracts/packet-v1.md`, pinned by shared golden
vectors in `contracts/vectors/packet-v1.json` — the single source both the C++
serializer (Phase 1.3) and the Python parser (Phase 1.4) build against, so they
can't drift.

**Why:**
- **Manifest on the wire (DEC-002).** `channel_mask` *is* the node's declared
  sensor set, so the layout is a node's manifest, not a fixed superset — a tank
  node is 16 B, a typical bed node 26 B, a 4-Watermark bed node 30 B (the
  ~20–30 B §8 target).
- **Three unambiguous states per channel.** absent (`channel_mask` bit 0),
  read-OK (mask 1 / `fault_mask` 0), or **declared-but-failed** (mask 1 /
  `fault_mask` 1, value bytes present but don't-care). A declared sensor that
  fails is an explicit fault the gateway can alert on, never a silent gap
  (DEC-002).
- **Layout is a pure function of `channel_mask`.** Faulted channels keep their
  bytes, so a parser walks the packet from mask + registry alone; `fault_mask` is
  pure metadata. A separate mask (not per-type sentinels) means the full value
  range stays usable on channel types with no spare sentinel (SHT45 ticks).
- **Raw resistance, not ADC counts, for the Watermark.** Resistance (kΩ) is the
  lowest *circuit-independent* value; ADC counts would bake in the deferred
  excitation-circuit design (D11). The temp-compensated kPa conversion — the part
  that benefits from re-revisability (D1) — stays downstream.
- **CRC fully parameter-pinned** (poly 0x1021, init 0xFFFF, no reflection, xorout
  0) in both the spec and the JSON, and hard-pinned by the golden vectors — CRC
  ambiguity is the classic C++/Python interop bug.

**Tradeoff / assumption:** Adding a sensor is a new bit + registry row with **no
`proto_ver` bump**; this relies on the **gateway registry always being a superset
of every deployed node's** (update the gateway before flashing a node with a new
channel — trivial given central gateway + USB-flash service window). A parser that
meets a set bit outside its registry can't compute trailing offsets and therefore
**MUST drop the packet**, never best-effort parse. `proto_ver` bumps only on a
layout-incompatible change (header/CRC/endianness/channel-width).

**Channel-ceiling upgrade path (16 → 32+ types).** The 16-bit masks cap the
registry at 16 channel *types* (a sensor can be several channels — SHT45 = T+RH);
the 17th type is the trigger. The path: **bump `proto_ver` to `0x02`, widen both
masks to `u32`** (32 types, +4 header bytes). Parser branches on `proto_ver`;
gateway updated first parses both v1 and v2; v1 field nodes need no change; only
v2 nodes pay the extra bytes. A variable-length mask (continuation bit) is the
"never revisit" option, taken only if a real node design needs >16 channels
(don't gold-plate, DEC-001). Documented in `contracts/packet-v1.md` § Versioning.

**Alternative considered — per-node contracts (deferred).** Instead of one global
channel registry + a `channel_mask` in every packet, make `node_id` the key: the
gateway looks up each node's manifest (DEC-002) and that *is* the layout, so the
packet carries no presence mask. **Pros:** the 16-type ceiling disappears
entirely (no global mask to overflow); slightly smaller packets; heterogeneous
one-off nodes cost nothing. **Cons (why deferred):** parsing becomes hostage to a
correct, in-sync `node_id`→manifest map (D7) — a packet from an unprovisioned or
skewed node is opaque bytes, where today a self-describing packet is parseable in
isolation; one contract becomes N, multiplying the firmware-vs-gateway drift
surface the golden vectors exist to eliminate; you still need a per-node fault map
(only the *presence* mask is shed); and a node loses the ability to drop a flaky
channel mid-cycle without a gateway change. For a small fleet with a central
gateway and a USB-flash service window — exactly where provisioning *will* skew —
the self-describing packet is the more forgiving trade, and the ceiling fix
(v2/u32) is small, late, and reversible. **The good part of the idea is kept:**
the per-node manifest stays a gateway-side *validation* layer (flag a node that
emits a channel its manifest doesn't list), not a *parsing dependency*. Flip to
per-node contracts only if the fleet becomes genuinely large and heterogeneous.

**Revisit:** If the bench excitation circuit (D11) makes raw ADC counts worth
carrying for the Watermark; at the 17th channel type (apply the v2/u32 path
above); if `node_id` (u8) nears its ceiling; or if the fleet grows large and
heterogeneous enough that per-node contracts (or a self-describing TLV layout)
beat the global registry.

---
