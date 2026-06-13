# soundings — Project Plan

**Critical path:** operational across Red Tunnel beds by the **2027 tomato
transplant (~March 2027)**. Paper planning summer 2026 → software build
(simulation-first) summer/fall 2026 → winter bench bring-up + Green Tunnel
shakedown → Red Tunnel deploy at transplant.

> **Phase structure agreed; tasks not yet broken down.** This file holds the
> phase skeleton. Per-phase tasks + poker estimates are materialized in a
> dedicated planning session and then live as GitHub Issues (DEC-S013). This plan
> is read at planning and written at retro — not edited mid-phase.

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

### Phase 2 — Node firmware core (simulated)

Deepen the node: every sensor behind an adapter with a fake driver; the real
sensor math (Watermark → tension with temp compensation, VPD); the
wake → sample → assemble → transmit → sleep cycle with ±30 s jitter and battery
read; the DEC-002 declared-manifest config and node-type presets. Host unit tests
+ Wokwi.

**Done when:** a simulated node of any type runs a full realistic cycle and its
packets pass the Phase-1 contract tests.

### Phase 3 — Gateway & ingestion (simulated)

Deepen the gateway: full decode, node → location mapping, server-side derived
math (if we land there — D1), graceful lost/malformed-packet handling and
logging, the time-series DB schema with retention/downsampling, the finalized
MQTT topic hierarchy, and a simulated Davis WeatherLink poller. Resolve the DB
choice (D6) here if the farm-records cross-over has firmed up.

**Done when:** a simulated fleet streams into a properly-shaped store, bad
packets are logged not crashed, and weather data lands beside it.

### Phase 4 — Dashboards & alerting (simulated)

The per-tunnel and overview dashboards (phone-usable) plus the alerts (tunnel
over-temp, soil tension > ~80 cb, node silent > 45 min, low battery) — all driven
by the simulator so they're real before hardware exists.

**Done when:** the dashboards and every alert fire correctly against simulated
conditions.

### Phase 5 — Bench bring-up (winter — the swap)

Swap fakes for real drivers behind the same adapters: one real node, the real
gateway radio, the AC excitation circuit, real sensors. Resolve the remaining
deferred hardware decisions (radio pairing D3, gateway box D4). Green Tunnel
shakedown.

**Done when:** a real reading off real hardware lands on the dashboard — the
spec's bar: *"I can watch soil moisture change as I add water."*

### Phase 6 — Stretch sensors (after core is trusted)

Leaf-wetness node and the portable stratification rig.

**Done when:** scoped separately once core is deployed and trusted.

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
