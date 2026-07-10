# Soundings — Architectural Decisions

Decisions are numbered DEC-NNN. "DEC-TBD" means a decision is flagged but
unresolved — consult @architect before building.

> **Reset note (2026-06-13).** The original DEC-001–006 were retired. DEC-001–004
> were leftover `seeds/dev` webapp template (Supabase / Next.js / shadcn) and
> never described this project. DEC-005–006 documented the tank-level sensor as
> architecture; per the project owner the tank sensor is just another sensor, so
> it now lives in `SPEC.md` §5.4 (detail in `tank-level-sensor.md`), not as a
> decision. Real decisions restart at DEC-001 below.
>
> **Most architecture choices live as `[settled]` tags in `SPEC.md`, and every
> deferred choice lives in the `SPEC.md` §12 register.** This file is reserved
> for decisions whose *reasoning* is worth preserving on its own — the rule we
> operate by, and the cross-cutting software-architecture calls.

---

## DEC-001: Nothing gets locked until it has to be

**Decision:** Defer every decision to the moment simulation or the bench forces a
real answer. Where a default keeps options open, take that default. The one
exception: a *hardware* choice that would change the *software* we write gets
made early — but only after confirming it can't sit behind an adapter and be
faked in simulation (most can). Deferred decisions are tracked in `SPEC.md` §12
so "deferred" never quietly becomes "forgotten."

**Why:** Soundings is read-only and low-stakes by design, and is being built
software-first against simulation precisely so that choices can be made against
real data instead of guesses. The dominant risk on a project like this isn't
getting a decision wrong — it's committing to infrastructure prematurely and
carrying that weight. Deferral is the strategy, not procrastination.

**Tradeoff:** Requires the discipline to actually maintain the §12 register and
revisit it at each phase, or deferrals rot into surprises.

**Revisit:** This is the governing principle. It isn't revisited — it's applied.

---

## DEC-002: One configurable firmware, declared sensor manifest

**Decision:** Every node runs the **same single firmware binary**. All sensor
drivers are compiled in; a node runs only the drivers listed in its **declared
per-node manifest**. Sensors are **declared, not auto-detected**. A node's
identity — ID, location, sensor set — is **configuration data, not code**. The
"node types" (bed / tunnel-air / tank / stratification rig) are config presets,
not separate firmware.

**Why:**
- **Identical, foolproof flashing.** The annual battery-swap service window
  becomes "plug in, flash, done" — no per-node binary bookkeeping, no flashing
  the wrong build.
- **Declared beats auto-detect for data integrity.** A node that *knows* it
  should have a sensor turns a missing reading into an actionable fault ("sensor
  expected, missing — go check it"), not a silent absence. It also handles the
  Watermark, whose AC-excitation analog circuit isn't reliably auto-detectable.
- **Identity-as-data.** Moving or adding a sensor is a manifest edit, not a
  recompile — the same principle as "raw reading is the durable record."
- **Negligible cost.** Carrying dormant drivers is nothing on the ESP32-S3.
- **Keeps a hardware question deferred.** The firmware is agnostic to how many
  sensors hang off one node, so the "one big node per tunnel vs. several small
  nodes" granularity call (DEC-001) stays deferrable.

**Tradeoff:** A node carries code it never runs. The manifest format and
provisioning flow need designing — the goal is a **dead-simple declaration
setup**, tracked as firmware-skeleton work. The gateway must know which fields a
given node emits, which ties to the packet schema (§12 D2) and the node→location
map (§12 D7).

**Revisit:** If a node ever can't carry all drivers (not foreseeable on the S3),
or if a compelling auto-detect *provisioning convenience* emerges — layered over
the declared manifest as source of truth, never replacing it.

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

## DEC-004: Storage and graphing belong to Poop Deck, not soundings (resolves D1 + D6)

**Decision:** Soundings does not run its own time-series DB or Grafana. Storage and dashboarding move to **Poop Deck**, the farm's shared telemetry backend (TimescaleDB + Grafana, fed over MQTT). Soundings keeps everything upstream of the store: field-node firmware, the gateway/decoder, the packet contract (DEC-003, **unchanged**), the node→location map (D7, gateway-side enrichment before publish), and **its own alert + dashboard definitions as versioned config in this repo**, provisioned into the shared Grafana. The gateway derives kPa/VPD/gallons and publishes **raw + derived** JSON to Poop Deck under `farm/soundings/…`; Poop Deck stores both and never recomputes.

**Why:**
- **One store, farm-wide (resolves D6).** The JOIN-to-farm-records requirement D6 hinged on is real, and Poop Deck is the Postgres counterparty — tinkle already writes there. A single relational store lets soil tension, irrigation, and weather be correlated with a JOIN instead of an ETL, which the provisional VictoriaMetrics (a bare metrics TSDB) couldn't give.
- **Raw preserved, derivation re-revisable (resolves D1).** Raw stays on the wire (DEC-003) and Poop Deck stores raw **and** derived, so a re-fit calibration curve re-derives from stored raw without reflashing. Deriving gateway-side (not on-node) keeps the firmware from baking in coefficients and keeps the math re-revisable. **Do not adopt tinkle's semantic-only schema for soundings** — tinkle is an event producer (a valve-open *is* the fact); soundings is a sensor producer whose raw resistance/T-RH is the ground truth and kPa/VPD a lens.
- **Clear, one-way ownership.** Soundings owns sensor→packet→gateway→publish and its own dashboard/alert *definitions*; Poop Deck owns the store and the shared Grafana instance. Soundings publishes; Poop Deck remembers. A Poop Deck outage is a dropped publish, nothing worse — soundings stays autonomous.

**Tradeoff:** Soundings now depends on an external store for persistence and viewing, and its dashboards live as config provisioned into an instance it doesn't run. Accepted: farm-wide correlation and single-store simplicity outweigh local self-containment, and the gateway can buffer/replay to Poop Deck idempotently (natural key `(node_id, seq)`).

**Revisit:** If the farm-records JOIN requirement evaporates, or Poop Deck's ops burden proves heavier than a local store — neither expected. The DEC-003 wire contract is untouched by this decision.

---

*Settled choices recorded as `[settled]` in SPEC (read-only V1, LoRa
point-to-point not LoRaWAN, no solar, USB-flash not OTA, software-first build)
may graduate to their own DEC entries here if their reasoning needs preserving.
For now they live in SPEC §2–§3.*
