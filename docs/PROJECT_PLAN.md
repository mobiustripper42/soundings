# soundings — Project Plan

**Critical path:** operational across Red Tunnel beds by the **2027 tomato
transplant (~March 2027)**. Paper planning summer 2026 → software build
(simulation-first) summer/fall 2026 → winter bench bring-up + Green Tunnel
shakedown → Red Tunnel deploy at transplant.

> **Tasks broken down (2026-06-14 planning session).** Phases 1–3 are
> fine-pokered; Phases 4–7 carry a coarse, provisional skeleton (re-poker at each
> `/start-phase` — their task definitions depend on decisions the earlier phases
> resolve, DEC-001). Current-phase tasks live as GitHub Issues (DEC-S013). This
> plan is read at planning and written at retro — not edited mid-phase.
>
> **Direction change (2026-08-07, DEC-005).** The tank-level node was promoted
> ahead of the soil/VPD sensor work and inserted as a new **Phase 3 — a
> hardware-first vertical slice for one node type**. **Phase 2 is parked, not
> cancelled** — its issues ([#19–#25](https://github.com/mobiustripper42/soundings/issues))
> stay open with their `phase:2` labels and nothing in it was started, so the
> re-sequence cost no work. Former Phases 3–6 renumbered to 4–7; none had
> materialized issues, so no labels moved. Phases 4 and 6 shrank — the tank
> slice absorbs parts of both.
>
> **Scope to a trusted core: ~140 fine+coarse points** (22 + 29 + 47 + 8 + 13 + 21),
> plus a 13–21 stretch placeholder for Phase 7. Up from ~115 — the re-sequence
> didn't add work, it *exposed* it: the old Phase 5 range was carrying a whole
> hardware bring-up at ~30 coarse points that Phase 3 now itemizes honestly.

---

## Estimation Method

Fibonacci points (2, 3, 5, 8, 13). No 1s (just do it), avoid 13s (break them
down). Tests are baked into every estimate — no separate testing tasks. Velocity
is tracked as **throughput (points per calendar week)** at phase boundaries
(DEC-S026), not hours/point. See `VELOCITY_AND_POKER_GUIDE.md`.

**Velocity (throughput, DEC-S026):**

| Phase | Points | Span (d) | Throughput | Re-est'd | Net drift | Sessions |
|-------|--------|----------|------------|----------|-----------|----------|
| 1     | 22     | 3.5      | burst (<7d) — 22 pts in 3.5d | 0 | 0 | 1 |

Phase 1 was a single-session burst, so no per-week rate is quoted (a sub-week
denominator is noise). Estimate unit held: every task shipped at its planned
points (0 re-estimates, 0 net drift).

---

## Build Order — software-first, then the easiest hardware first

Phases 0–2 front-load the riskiest *integration* and keep every sensor and radio
behind an adapter driven by fakes.

**Phase 3 changes the shape of the rest (DEC-005).** Rather than holding all
hardware to the end, it takes the one node type with no analog front end — the
tank node — all the way to real silicon. That retires the unknowns every node
shares (link budget, sleep current, enclosure, battery model) behind a sensor
that can't be the culprit when something doesn't work. Fakes still drive the
parked soil/air sensors, whose real drivers swap in at Phase 6.

### Phase 0 — Re-baseline & scaffold

Get the project onto correct footing: `tool` type, seeds-version, a SPEC and
DECISIONS that describe the real project, a tool-type CLAUDE.md, and a documented
repo layout. No application code.

**Done when:** the repo describes the real project and has a documented place for
every kind of code to live. *(This phase — in progress.)*

### Phase 1 — The contract & the simulation spine

The thinnest possible end-to-end slice, fully faked. Choose the firmware
toolchain (resolves D5). Define packet v1; build the node-side serializer (C++)
and gateway-side parser (Python) pinned by shared round-trip test vectors; push
one synthetic reading all the way through — fake node → gateway → message bus →
time-series DB → one chart.

**Done when:** you can watch a simulated drydown curve move on a dashboard with
zero hardware. The whole pipeline is de-risked before any layer is deepened.

| Task | Description | Points | Issue |
|------|-------------|--------|-------|
| 1.1 | Firmware toolchain skeleton + native test harness — build env, `firmware/core` layout, fake-clock/fake-sensor harness stub, one green host test. **Resolves D5.** | 3 | [x] [#11](https://github.com/mobiustripper42/soundings/pull/11) |
| 1.2 | Packet v1 schema + shared golden test vectors — the contract artifact in `contracts/`. Raw values on the wire (D1 default), CRC choice, firmware-version field, **manifest-aware field presence** (DEC-002, not just a fixed superset). **Designs D2.** | 5 | [x] [#12](https://github.com/mobiustripper42/soundings/pull/12) |
| 1.3 | C++ node-side serializer + native round-trip against the vectors. Compiles native *and* esp32-clean. | 3 | [x] [#14](https://github.com/mobiustripper42/soundings/pull/14) |
| 1.4 | Python gateway-side parser + pytest round-trip against the same vectors. Graceful on malformed (truncation, bad CRC, unknown fw-version, unexpected field set) — log + drop, never crash. | 3 | [x] [#15](https://github.com/mobiustripper42/soundings/pull/15) |
| 1.5 | Minimal sim server-stack compose — Mosquitto + a **provisional** DB + Grafana. Pick-but-switchable: fastest DB to stand up, **non-binding on D6** (labeled in compose + Issue). | 3 | [x] [#17](https://github.com/mobiustripper42/soundings/pull/17) |
| 1.6 | End-to-end spine wiring — fake-node emitter (slow drydown curve) → gateway → bus → DB → one moving chart. The fleet sim should eyeball the ±30 s jitter window. | 5 | [x] [#18](https://github.com/mobiustripper42/soundings/pull/18) |

**Phase 1 total: 22 points.**

### Phase 2 — Node firmware core (simulated) — ⏸ **PARKED**

> **Parked 2026-08-07 (DEC-005), not cancelled.** Zero tasks were started, so
> nothing is lost. Issues #19–#25 stay open with `phase:2` labels. Phase 3 (the
> tank slice) builds the node firmware spine — adapter seam, run cycle, a minimal
> manifest — against one simple sensor; this phase resumes afterward to add the
> hard sensor math (Watermark, VPD) and the full node-type preset system on top
> of a spine that has by then been proven on real hardware. Expect 2.1, 2.5 and
> 2.6 to shrink at resume, since Phase 3 does a narrowed version of each.

Deepen the node: every sensor behind an adapter with a fake driver; the real
sensor math (Watermark → tension with temp compensation, VPD); the
wake → sample → assemble → transmit → sleep cycle with ±30 s jitter and battery
read; the DEC-002 declared-manifest config and node-type presets. Host unit tests
+ Wokwi.

**Done when:** a simulated node of any type runs a full realistic cycle and its
packets pass the Phase-1 contract tests.

| Task | Description | Points | Issue |
|------|-------------|--------|-------|
| 2.1 | Adapter seam — interface set (`ISoilMoisture`, `ITemp`, `IHumidity`, `IRadio`, `IClock`, battery) + non-sensor fakes (radio queues a packet, clock fakes `millis()`/sleep, settable battery voltage). Sensor-specific fakes ride with their math tasks. | 3 | [#19](https://github.com/mobiustripper42/soundings/issues/19) |
| 2.2 | Watermark tension math — AC-excitation sampling (both half-cycles, DC cancel), resistance→kPa with temp compensation, valid-tension band, golden vectors for the noisy wet end. Seed coefficients; bench-calibrate in Phase 5. The anchor measurement. | 5 | [#20](https://github.com/mobiustripper42/soundings/issues/20) |
| 2.3 | VPD + soil-temp math — SHT45 T/RH → SVP/AVP/VPD (test matrix across the tomato band + extremes), DS18B20 read (feeds 2.2's temp comp). | 3 | [#21](https://github.com/mobiustripper42/soundings/issues/21) |
| 2.4 | Tank-level math — two-segment distance→gallons curve (IBC kink ~46"), dead-zone clamp, raw distance always emitted. **Seed coefficients; empirical fit is a Phase 5 bench task.** | 2 | [#22](https://github.com/mobiustripper42/soundings/issues/22) |
| 2.5 | Run cycle — wake → sample (walk declared set) → assemble → transmit → sleep, non-blocking against `millis()`, ±30 s wake jitter (injectable RNG), battery read. | 5 | [#23](https://github.com/mobiustripper42/soundings/issues/23) |
| 2.6 | Declared-manifest config + node-type presets (DEC-002) — manifest format (identity-as-data), bed/tunnel-air/tank/rig presets, declared-but-missing → fault not silent gap. One coherent unit, no split. | 8 | [#24](https://github.com/mobiustripper42/soundings/issues/24) |
| 2.7 | Wokwi node integration — `diagram.json`, sim build flag (sim-shortened cycle constants), confirm a declared node wakes → samples → assembles → "transmits" in sim. | 3 | [#25](https://github.com/mobiustripper42/soundings/issues/25) |

**Phase 2 total: 29 points (parked).**

### Phase 3 — Tank node vertical slice (the first hardware)

One node type, all the way through: hardware plan → parts → real radio → real
sensor → real packets → gallons on a chart. Everything the tank node needs and
nothing it doesn't.

The tank node goes first because it is the only node type with no analog front
end — no AC excitation circuit (D11), no Watermark calibration, no temperature
compensation. One sensor, one UART. Every remaining unknown in the build (link
budget, deep-sleep current, enclosure survival) is shared by *every* node type,
so isolating them behind a sensor that can't be the culprit is the cheapest way
to meet hardware. **This resolves D3, D4, D7, D9** and validates the `SPEC.md` §4
power claim with a measurement.

**Done when:** the real catchment-cluster level is on a chart, fed by a real node
over a real radio link. Full detail: `docs/HARDWARE_BUILD_PLAN.md`.

Sequenced hardware-plan-first: tasks 3.1–3.2 gate the spend, 3.3–3.7 are software
that runs in parallel against fakes, 3.8–3.12 are bring-up, 3.13 trails.

| Task | Description | Points | Issue |
|------|-------------|--------|-------|
| 3.1 | Hardware build plan + BOM + the chat↔CC research loop — resolve the `[verify]` set (HW-01…HW-08) before anything is ordered. | 3 | — |
| 3.2 | **Range test** — 2× Heltec V3, RSSI + loss from the tank cluster to the intended gateway location. **Resolves D3 + D4.** Gates every downstream hardware choice; do it before any other spend. | 3 | — |
| 3.3 | Adapter seam, tank subset — `IDistance`, `IRadio`, `IClock`, battery + fakes. A narrowed 2.1; the sensor-specific fakes for soil/air stay parked. | 3 | — |
| 3.4 | Run cycle — wake → sample → assemble → transmit → sleep, non-blocking against `millis()`, ±30 s jitter (injectable RNG), battery read. Node-type-agnostic, so 2.5 is largely satisfied by this. | 5 | — |
| 3.5 | Minimal declared manifest — tank preset only, identity-as-data shaped (DEC-002) but without the full node-type preset system. A narrowed 2.6 (8 → 3). | 3 | — |
| 3.6 | Gateway: tank derivation (two-segment curve + dead-zone clamp + percent) in **Python, not firmware** (DEC-004), `farm/soundings/…` topic hierarchy, node→location map. **Resolves D7 + D9.** | 5 | — |
| 3.7 | Poop Deck publish path — retire the provisional VictoriaMetrics + Grafana from `deploy/`, swap `ingest.py`'s writer. The DEC-004 breakout, previously untracked. | 3 | — |
| 3.8 | Real A02YYUW UART driver behind `IDistance` + bench read. Switched power rail (see build plan §6). | 3 | — |
| 3.9 | Gateway radio firmware (LoRa → USB serial) + `SerialPacketSource` behind the existing `IPacketSource` seam. | 5 | — |
| 3.10 | Power — battery ADC, deep sleep, LVC, and a **measured** µA draw replacing `SPEC.md` §4's estimate. Also calibrates the fleet battery model ([#36](https://github.com/mobiustripper42/soundings/issues/36)). | 5 | — |
| 3.11 | Enclosure + tank-lid mount + PVC standoff build. Gated on HW-03 (beam angle) and HW-04 (cable run). | 3 | — |
| 3.12 | **First light** — real tank distance on a chart, analytic seed curve. **M1.** | 3 | — |
| 3.13 | Calibration — analytic curve from measured tank geometry, then empirical correction from observed fills. **M2, calendar-gated** — may trail the phase close. | 3 | — |

**Phase 3 total: 47 points.**

> **The largest phase in the plan — ~2× Phase 1.** It carries a full hardware
> bring-up, which the original plan spread across Phases 3 and 5. If it wants
> splitting at `/start-phase`, the natural seam is after **3.12 (first light)**;
> 3.13 is calendar-gated on rainfall and should be expected to trail regardless.

### Phase 4 — Gateway & ingestion (simulated)

Generalize the gateway from *one real node* (Phase 3) to a **fleet**: graceful
lost/malformed-packet handling, structured logging, restart safety, the rest of
the derived math, and a simulated Davis WeatherLink poller.

**Done when:** a simulated fleet streams into Poop Deck, bad packets are logged
not crashed, and weather data lands beside it.

> **Shrunk by the Phase 3 re-sequence (DEC-005).** Four of the five original
> tasks are wholly or partly absorbed: D7 + D9 are resolved by 3.6, D6 and D1 by
> DEC-004, and the tank slice builds a real receive loop and the first
> gateway-side derivation. What's left is fleet hardening and the work that
> genuinely needs the parked Phase 2 sensor math.
>
> **Provisional — re-poker at `/start-phase`.**

| Task | Description | Points |
|------|-------------|--------|
| 4.1 | Gateway daemon hardening for a *fleet* — the 3.9 receive loop generalized: multi-node, lost/malformed resilience, structured logging, restart safety. | ~3 |
| 4.2 | Extend gateway-side derivation to kPa + VPD — the tank curve (3.6) is the first instance; this adds the rest. **Gated on the parked Phase 2 math.** | ~3 |
| 4.3 | Simulated Davis WeatherLink poller — fake the local HTTP JSON API, poll ~60 s, land weather beside sensor data. *(Confirm this is still soundings' job and not Poop Deck's under DEC-004.)* | ~3 |
| ~~Node→location + topic hierarchy~~ | Absorbed by 3.6 — D7 + D9 resolved there. | — |
| ~~Time-series DB schema~~ | D6 resolved by DEC-004; the store is Poop Deck's. Publish shape lands in 3.7. | — |

**Phase 4 coarse total: ~8 points (provisional, was ~21).**

### Phase 5 — Dashboards & alerting (simulated)

The per-tunnel and overview dashboards (phone-usable) plus the alerts (tunnel
over-temp, soil tension > ~80 cb, node silent > 45 min, low battery) — all driven
by the simulator so they're real before hardware exists.

**Done when:** the dashboards and every alert fire correctly against simulated
conditions.

> **Provisional — re-poker at `/start-phase`.** Layout depends on D10. Under
> DEC-004 these are dashboard/alert *definitions* versioned here and provisioned
> into Poop Deck's shared Grafana — the tank chart from 3.12 is the first one.

| Task | Description | Points |
|------|-------------|--------|
| 5.1 | Per-tunnel + overview dashboards (phone-usable) from the existing series (**resolves D10**). | ~8 |
| 5.2 | Alerting — tunnel over-temp, soil tension > ~80 cb, node silent > 45 min, low battery — driven by the simulator. | ~5 |

**Phase 5 coarse total: ~13 points (provisional).**

### Phase 6 — Bench bring-up, remaining sensors (winter — the swap)

Swap the rest of the fakes for real drivers behind the same adapters. Phase 3
already did this for the tank node — the radio, the battery read, one real
sensor, one real node. What's left is the analog front end and the sensors the
parked Phase 2 math serves.

**Done when:** a real reading off a real *bed* node lands on the dashboard — the
spec's bar: *"I can watch soil moisture change as I add water."*

> **Shrunk by the Phase 3 re-sequence (DEC-005).** D3, D4, the A02YYUW driver,
> the battery read, and the tank curve all move to Phase 3. The widest remaining
> uncertainty is **D11** — the AC excitation circuit, the hardest hardware in the
> project. **Provisional — re-poker at `/start-phase`.**

| Task | Description | Points |
|------|-------------|--------|
| 6.1 | Remaining real drivers behind the same adapters — DS18B20, SHT45 on real silicon. *(A02YYUW + battery read done in 3.8/3.10.)* | ~5 |
| 6.2 | AC excitation circuit + real Watermark driver (**D11**) — the anchor sensor on the bench. | ~8 |
| 6.3 | Watermark calibration against the commercial reader. *(The tank curve is calibrated in 3.13.)* | ~3 |
| 6.4 | Green Tunnel shakedown — one real bed node end-to-end, the "watch moisture change as I add water" gate. | ~5 |
| ~~Real gateway radio~~ | Absorbed by 3.2 + 3.9 — D3 + D4 resolved there. | — |

**Phase 6 coarse total: ~21 points (provisional, ±, was ~30).**

### Phase 7 — Stretch sensors (after core is trusted)

Leaf-wetness node and the portable stratification rig.

**Done when:** scoped separately once core is deployed and trusted.

> **Not yet poked — scoped at the boundary** once core is deployed and trusted,
> per SPEC §5.5/§5.6. Nominal **13–21** as a forecasting placeholder only.

| Task | Description | Points |
|------|-------------|--------|
| 7.1 | Leaf-wetness node (SPEC §5.5 — highest-priority stretch; may promote to core). | TBD |
| 7.2 | Portable stratification rig (SPEC §5.6 — one relocatable node, vertical temp profile). | TBD |

**Phase 7: TBD (nominal 13–21 placeholder).**

---

## Velocity Table

Updated at each phase boundary (throughput, DEC-S026).

| Phase | Date Closed | Points | Span (days) | Throughput (pts/wk) | Re-estimated | Net Drift | PRs |
|-------|-------------|--------|-------------|---------------------|--------------|-----------|-----|
| 0 | — | — | — | — | — | — | — |

---

## Phase Boundary Checklist

At the end of every phase:
1. All targeted tests green (native + contract round-trip; sim where relevant).
2. `/doc-consistency-check` if docs were touched heavily.
3. `/retro` — throughput velocity, mark `[x]`, write RETROSPECTIVES.md entry.
4. `/start-phase` for the next phase (materialize tasks as Issues).
