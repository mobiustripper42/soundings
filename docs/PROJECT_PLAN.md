# soundings — Project Plan

**Critical path:** operational across Red Tunnel beds by the **2027 tomato
transplant (~March 2027)**. Paper planning summer 2026 → software build
(simulation-first) summer/fall 2026 → winter bench bring-up + Green Tunnel
shakedown → Red Tunnel deploy at transplant.

> **Tasks broken down (2026-06-14 planning session).** Phases 1–2 are
> fine-pokered; Phases 3–6 carry a coarse, provisional skeleton (re-poker at each
> `/start-phase` — their task definitions depend on decisions Phases 1–2 resolve,
> DEC-001). Current-phase tasks live as GitHub Issues (DEC-S013). This plan is
> read at planning and written at retro — not edited mid-phase.
>
> **Scope to a trusted core: ~115 fine+coarse points** (22 + 29 + 21 + 13 + 30),
> plus a 13–21 stretch placeholder for Phase 6.

---

## Estimation Method

Fibonacci points (2, 3, 5, 8, 13). No 1s (just do it), avoid 13s (break them
down). Tests are baked into every estimate — no separate testing tasks. Velocity
is tracked as **throughput (points per calendar week)** at phase boundaries
(DEC-S026), not hours/point. See `VELOCITY_AND_POKER_GUIDE.md`.

**Velocity baseline:** not yet established.

---

## Build Order — software-first, simulation-first

The build is sequenced to front-load the riskiest integration and push all
hardware to the end. Sensors and radios sit behind adapters; fakes drive them
through Phases 1–4, real drivers swap in at Phase 5.

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
| 1.1 | Firmware toolchain skeleton + native test harness — build env, `firmware/core` layout, fake-clock/fake-sensor harness stub, one green host test. **Resolves D5.** | 3 | — |
| 1.2 | Packet v1 schema + shared golden test vectors — the contract artifact in `contracts/`. Raw values on the wire (D1 default), CRC choice, firmware-version field, **manifest-aware field presence** (DEC-002, not just a fixed superset). **Designs D2.** | 5 | — |
| 1.3 | C++ node-side serializer + native round-trip against the vectors. Compiles native *and* esp32-clean. | 3 | — |
| 1.4 | Python gateway-side parser + pytest round-trip against the same vectors. Graceful on malformed (truncation, bad CRC, unknown fw-version, unexpected field set) — log + drop, never crash. | 3 | — |
| 1.5 | Minimal sim server-stack compose — Mosquitto + a **provisional** DB + Grafana. Pick-but-switchable: fastest DB to stand up, **non-binding on D6** (labeled in compose + Issue). | 3 | — |
| 1.6 | End-to-end spine wiring — fake-node emitter (slow drydown curve) → gateway → bus → DB → one moving chart. The fleet sim should eyeball the ±30 s jitter window. | 5 | — |

**Phase 1 total: 22 points.**

### Phase 2 — Node firmware core (simulated)

Deepen the node: every sensor behind an adapter with a fake driver; the real
sensor math (Watermark → tension with temp compensation, VPD); the
wake → sample → assemble → transmit → sleep cycle with ±30 s jitter and battery
read; the DEC-002 declared-manifest config and node-type presets. Host unit tests
+ Wokwi.

**Done when:** a simulated node of any type runs a full realistic cycle and its
packets pass the Phase-1 contract tests.

| Task | Description | Points | Issue |
|------|-------------|--------|-------|
| 2.1 | Adapter seam — interface set (`ISoilMoisture`, `ITemp`, `IHumidity`, `IRadio`, `IClock`, battery) + non-sensor fakes (radio queues a packet, clock fakes `millis()`/sleep, settable battery voltage). Sensor-specific fakes ride with their math tasks. | 3 | — |
| 2.2 | Watermark tension math — AC-excitation sampling (both half-cycles, DC cancel), resistance→kPa with temp compensation, valid-tension band, golden vectors for the noisy wet end. Seed coefficients; bench-calibrate in Phase 5. The anchor measurement. | 5 | — |
| 2.3 | VPD + soil-temp math — SHT45 T/RH → SVP/AVP/VPD (test matrix across the tomato band + extremes), DS18B20 read (feeds 2.2's temp comp). | 3 | — |
| 2.4 | Tank-level math — two-segment distance→gallons curve (IBC kink ~46"), dead-zone clamp, raw distance always emitted. **Seed coefficients; empirical fit is a Phase 5 bench task.** | 2 | — |
| 2.5 | Run cycle — wake → sample (walk declared set) → assemble → transmit → sleep, non-blocking against `millis()`, ±30 s wake jitter (injectable RNG), battery read. | 5 | — |
| 2.6 | Declared-manifest config + node-type presets (DEC-002) — manifest format (identity-as-data), bed/tunnel-air/tank/rig presets, declared-but-missing → fault not silent gap. One coherent unit, no split. | 8 | — |
| 2.7 | Wokwi node integration — `diagram.json`, sim build flag (sim-shortened cycle constants), confirm a declared node wakes → samples → assembles → "transmits" in sim. | 3 | — |

**Phase 2 total: 29 points.**

### Phase 3 — Gateway & ingestion (simulated)

Deepen the gateway: full decode, node → location mapping, server-side derived
math (if we land there — D1), graceful lost/malformed-packet handling and
logging, the time-series DB schema with retention/downsampling, the finalized
MQTT topic hierarchy, and a simulated Davis WeatherLink poller. Resolve the DB
choice (D6) here if the farm-records cross-over has firmed up.

**Done when:** a simulated fleet streams into a properly-shaped store, bad
packets are logged not crashed, and weather data lands beside it.

> **Provisional — re-poker at `/start-phase`.** These tasks' *definitions* depend
> on decisions Phases 1–2 resolve (D1, D6, D7, D9); estimating them finely now
> would assign points to work simulation is about to reshape (DEC-001). Coarse
> skeleton + range for forecasting only.

| Task | Description | Points |
|------|-------------|--------|
| 3.1 | Gateway decode daemon — wrap the 1.4 parser in a long-running service: `IPacketSource` seam, receive loop, malformed/lost-packet resilience + structured logging. | ~5 |
| 3.2 | Node→location mapping (**resolves D7**: config vs DB table) + MQTT topic hierarchy (**resolves D9**). | ~5 |
| 3.3 | Time-series DB schema + retention/downsampling — **resolves D6** (Timescale vs VictoriaMetrics), gated on the farm-records JOIN cross-over firming up. | ~5 |
| 3.4 | Server-side derived math — **resolves D1**: where kPa/VPD/gallons execute; wire the (portable, Phase-2) core math into its runtime home. | ~3 |
| 3.5 | Simulated Davis WeatherLink poller — fake the local HTTP JSON API, poll ~60 s, land weather beside sensor data. | ~3 |

**Phase 3 coarse total: ~21 points (provisional).**

### Phase 4 — Dashboards & alerting (simulated)

The per-tunnel and overview dashboards (phone-usable) plus the alerts (tunnel
over-temp, soil tension > ~80 cb, node silent > 45 min, low battery) — all driven
by the simulator so they're real before hardware exists.

**Done when:** the dashboards and every alert fire correctly against simulated
conditions.

> **Provisional — re-poker at `/start-phase`.** Layout depends on D10 and the DB
> chosen in Phase 3.

| Task | Description | Points |
|------|-------------|--------|
| 4.1 | Per-tunnel + overview dashboards (phone-usable) from the existing series (**resolves D10**). | ~8 |
| 4.2 | Alerting — tunnel over-temp, soil tension > ~80 cb, node silent > 45 min, low battery — driven by the simulator. | ~5 |

**Phase 4 coarse total: ~13 points (provisional).**

### Phase 5 — Bench bring-up (winter — the swap)

Swap fakes for real drivers behind the same adapters: one real node, the real
gateway radio, the AC excitation circuit, real sensors. Resolve the remaining
deferred hardware decisions (radio pairing D3, gateway box D4). Green Tunnel
shakedown.

**Done when:** a real reading off real hardware lands on the dashboard — the
spec's bar: *"I can watch soil moisture change as I add water."*

> **Provisional — re-poker at `/start-phase`.** Widest uncertainty in the plan:
> first contact with hardware, and it resolves the remaining hardware deferrals
> (D3, D4, D11). Range, not a point estimate.

| Task | Description | Points |
|------|-------------|--------|
| 5.1 | Swap fakes for real drivers behind the same adapters — DS18B20, SHT45, A02YYUW, battery read on real silicon. | ~8 |
| 5.2 | AC excitation circuit + real Watermark driver (**D11**) — the anchor sensor on the bench. | ~8 |
| 5.3 | Real gateway radio — **resolves D3** (SX1262↔SX127x/RFM95 PHY pairing) and **D4** (gateway box). | ~5 |
| 5.4 | Calibration — Watermark against the commercial reader, tank curve against known fills (the empirical fits deferred from 2.2/2.4). | ~5 |
| 5.5 | Green Tunnel shakedown — one real node end-to-end, the "watch moisture change as I add water" gate. | ~5 |

**Phase 5 coarse total: ~30 points (provisional, ±).**

### Phase 6 — Stretch sensors (after core is trusted)

Leaf-wetness node and the portable stratification rig.

**Done when:** scoped separately once core is deployed and trusted.

> **Not yet poked — scoped at the boundary** once core is deployed and trusted,
> per SPEC §5.5/§5.6. Nominal **13–21** as a forecasting placeholder only.

| Task | Description | Points |
|------|-------------|--------|
| 6.1 | Leaf-wetness node (SPEC §5.5 — highest-priority stretch; may promote to core). | TBD |
| 6.2 | Portable stratification rig (SPEC §5.6 — one relocatable node, vertical temp profile). | TBD |

**Phase 6: TBD (nominal 13–21 placeholder).**

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
