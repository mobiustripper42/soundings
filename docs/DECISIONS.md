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

## DEC-005: The tank node goes first — a hardware-first vertical slice

**Decision:** The tank-level node is promoted ahead of the soil/VPD sensor work and built as a complete vertical slice — hardware plan → parts → real radio → real sensor → real packets → gallons on a chart — as a new **Phase 3** (`docs/PROJECT_PLAN.md`, detail in `docs/HARDWARE_BUILD_PLAN.md`). **Phase 2 is parked, not cancelled**; former Phases 3–6 renumber to 4–7. Three sub-decisions ride along:

- **Topic namespace: `farm/soundings/…`.** Resolves a conflict between `docs/tank-level-sensor.md:93-99` (`farm/water/cluster/*`, written before DEC-004) and DEC-004 (`farm/soundings/…`). Everything soundings publishes lives under its own namespace. If tinkle wants a `farm/water/…` view for its future pump lockout, that is a broker-side republish on the consumer's side of the boundary — not soundings' concern. Constrains D9; the full hierarchy is still designed in 3.6.
- **Tank derivation runs gateway-side, in Python.** Issue [#22](https://github.com/mobiustripper42/soundings/issues/22) put the two-segment curve in firmware; it was written 2026-06-17, three weeks before DEC-004 moved all derivation to the gateway. #22 is stale and gets rewritten as a Python task (3.6).
- **Calibration seeds analytically, corrects empirically.** `docs/tank-level-sensor.md:79-87` specifies a purely empirical fit, whose stated virtue is needing no tank dimensions. Its unstated cost is a calendar dependency: the tanks are rain-fed, so fill points on both sides of the IBC breakpoint could take months, and it needs an independent volume reference that may not exist. Instead: measure the three tanks once with a tape measure, compute the two-segment curve from geometry, and correct it from real fills as they happen. Raw distance is published either way (`:100-102`), so any curve re-fits from stored raw with no reflash.

**Why:**
- **It is the only node type with no analog front end.** No AC excitation circuit (D11 — the hardest hardware in the project), no Watermark calibration against a commercial reader, no temperature compensation. One sensor, one UART, one 4-byte frame. Roughly 21 points of the hardest near-path work defers with it.
- **It isolates the unknowns that are common to every node.** Link budget, deep-sleep current, enclosure survival, and the battery model are shared by all node types and none of them are validated. Meeting them behind a sensor that *cannot be the culprit* is the cheapest possible first contact with hardware. When the link doesn't close, the sensor is not a suspect.
- **The re-sequence cost nothing.** Zero Phase 2 issues were started; #19–#25 keep their `phase:2` labels untouched. Former Phases 3–6 had no materialized issues, so renumbering moved no labels.
- **It validates the numbers the whole fleet is planned against.** `SPEC.md` §4's 20–30 µA sleep and ~2-year battery life are estimates carried from the source spec and never measured. A tank node — one cheap sensor on a switched rail — sits near the floor of what any node draws, so measuring it calibrates the fleet battery model that open issue [#36](https://github.com/mobiustripper42/soundings/issues/36) asks for.
- **Phase 2 resumes onto a proven spine.** Phase 3 builds the adapter seam, run cycle, and a minimal manifest against one simple sensor. The parked sensor-math tasks land later on firmware that has by then survived real hardware, rather than on fakes.

**Tradeoff:** The anchor measurement — soil-moisture tension, the thing that can't be gotten any other way (`SPEC.md` §1) — is deliberately delayed in favor of a secondary one. Accepted for two reasons: the critical path is the **2027 tomato transplant (~March 2027)**, which the delay does not threaten, and the tank node retires the hardware risk that would otherwise surface for the first time *during* the anchor sensor's bench bring-up, when the excitation circuit would be an equally plausible suspect. A second, real cost: Phase 3 is ~47 points, roughly twice Phase 1, because it carries a full hardware bring-up the old plan had spread thin across two coarse phases.

**Revisit:** If the range test (3.2) shows the link can't close from the tank cluster, the gateway architecture branches (`HARDWARE_BUILD_PLAN.md` §3) but the sequencing still holds — that test has to happen for any node. If tank hardware stalls on procurement or weather, Phase 2 is parked, not dead: its issues are intact and can resume in place.

---

*Settled choices recorded as `[settled]` in SPEC (read-only V1, LoRa
point-to-point not LoRaWAN, no solar, USB-flash not OTA, software-first build)
may graduate to their own DEC entries here if their reasoning needs preserving.
For now they live in SPEC §2–§3.*
