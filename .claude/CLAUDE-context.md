# soundings — Project Context

Everything specific to **this** project. The jig-managed `CLAUDE.md` shell reads this file at session start and treats it as authoritative for project-specific facts (DEC-S019). Nothing here syncs from jig.

## What We're Building

**Soundings** is a LoRa wireless sensor mesh for Bay Branch Farm. Battery field nodes measure soil-moisture tension (the anchor measurement), soil temperature, canopy air temp/humidity (for VPD), and catchment tank level, and report over raw point-to-point radio to a gateway on the farm LAN. The gateway decodes the packets and stores them on an existing headless server for viewing in a dashboard. **V1 is read-only telemetry** — sensors observe and report, nothing actuates. A failed node means missing data, nothing worse.

Soundings is **one component of a larger farm recording/analysis tool** (daily log, harvest records, crop diagnosis, labor stats) under separate development. Cross-over gets raised as it arises — notably the time-series DB choice (see SPEC §12 D6).

**Sibling project — tinkle.** tinkle is the farm's irrigation controller (separate repo, firmware built, hardware build next). Soundings never controls anything; tinkle may eventually *consume* Soundings data (tank level for a pump-lockout; a future VPD advisory). The boundary is one-way and tinkle stays autonomous if Soundings is dark. Be aware tinkle exists; don't couple to it.

## Build Philosophy

- **Read-only telemetry (V1).** Measure and report; never actuate. Low-stakes by design (DEC-001-era scope in SPEC §2).
- **Nothing gets locked until it has to be (DEC-001).** Defer every decision to the moment simulation or the bench forces it. Deferred decisions live in the SPEC §12 register so they don't rot into surprises.
- **Software first, hardware second.** Build and prove the whole stack against simulation now, so the winter bench build is tuning, not authoring. Sensors and radios sit behind adapters; fakes drive them in simulation and get swapped for real drivers at the bench. This mirrors how tinkle's firmware was built before any hardware.
- **One firmware, declared sensors (DEC-002).** Every node runs the same binary; a per-node declared manifest says which sensors it has. Identity is data, not code.
- **Build crappy, learn, rebuild.** The first node will be ugly. Deploy it; 30 days of real operation teaches more than a month of planning.

## Stack

- **Field node:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 radio, one board), 2× 18650 (no solar), deep sleep. **[settled]**
- **WiFi is on the board and now used.** This said "WiFi disabled" until the OTA work shipped — see `firmware/src/esp32/ota_client.cpp` and `docs/OTA.md`. The radio is still LoRa; WiFi wakes only to pull firmware.
- **Radio:** raw LoRa point-to-point (not LoRaWAN), US 902–928 MHz, on a duty cycle with wake jitter. **[settled]** — the interval and jitter are firmware constants, not facts this file can pin; read them from the source.
- **Sensors:** Watermark soil tension (DIY AC excitation), DS18B20 soil temp (1-Wire), SHT45 air T/RH → VPD (I2C), A02YYUW ultrasonic tank level (UART).
- **Gateway:** a small always-on box near the farm center, on the LAN, holding the LoRa antenna; runs a Python decoder daemon (box choice deferred — D4).
- **Server stack:** not soundings'. **D6 was resolved by DEC-004** — persistence and dashboards belong to Poop Deck. This entry said "[proposed], unvalidated, D6 unresolved" while the Migration Protocol section below said it was settled, in the same always-loaded file. `mill-dev` (Tailscale VPS) is where we develop and simulate.
- **Toolchain:** **PlatformIO** (D5 resolved, Phase 1.1) — `node` (Heltec V3) + `native` (Unity host tests) envs in `firmware/platformio.ini`, mirroring tinkle.

See `docs/SPEC.md` for the full picture and the §12 deferred-decision register.

## Architecture

Two code domains plus a wire contract between them:

- **Node firmware** — wake → sample sensors → assemble packet → transmit → sleep. Fully non-blocking; long actions time against `millis()`. Sensor drivers and the radio live behind adapter interfaces so fakes drive them in host tests / Wokwi and real drivers swap in at the bench — `ls firmware/src/core/i*.h` is the current set. Platform-independent logic lives in the core so it compiles for both the ESP32 and the native test runner.
- **Gateway/ingestion** — the packet source is an adapter too, faked with synthetic or replayed packets so the whole pipeline runs on a laptop with zero hardware.
- **The packet schema is the contract.** A C++ serializer and a Python parser that must never drift — pinned by shared golden-vector round-trip tests. Versioned (every packet carries a firmware-version field).

## Commands

```bash
# Firmware (run from firmware/)
pio test -e native           # host unit tests (core sensor/packet logic) — the load-bearing tier
pio run  -e node             # build node firmware (Heltec WiFi LoRa 32 V3)

# Gateway (Python, run from gateway/)
#   venv:  python3 -m venv .venv && .venv/bin/pip install pytest
.venv/bin/python -m pytest   # gateway parser + contract round-trip tests

# Doc gates (run from the repo root)
npm run verify               # decisions, dictionary, denied commands, context, docs

# Dev-sim stack only — NOT production (DEC-004: the store and broker are Poop Deck's)
docker compose -f deploy/dev-sim/docker-compose.yml up   # broker + DB + Grafana
```

## Additional Docs

| File | Purpose |
|------|---------|
| `docs/tank-level-sensor.md` | Detail spec for the tank-level sensor (folded into SPEC §5.4) |
| `docs/HARDWARE_BUILD_PLAN.md` | The Phase 3 tank-node hardware build — BOM, gateway architecture, mounting, power budget, calibration, build sequence. Carries a `[verify]` tag for anything needing a datasheet or measurement before we buy or cut. |
| `docs/CHAT_HANDOFF.md` | The chat ↔ Claude Code interchange. Hand-synced question ledger (`HW-nn`) — CC owns the repo, Claude chat owns live web research (prices, datasheets, availability). **Anything that matters and lives only in a chat window is lost.** §3.3 carries CC's review of each promoted round. |
| `docs/FUTURE_IDEAS.md` | Parking lot for ideas worth keeping but not building yet. Curated by `@ideas`; nothing in it is committed scope. |

Notes on the baseline docs: `docs/SPEC.md` carries the **§12 deferred-decision register** (D1–D6) — the home for every not-yet-locked choice (DEC-001). `docs/RETROSPECTIVES.md` uses **throughput velocity (DEC-S026)**. `docs/USER_STORIES.md`, `docs/CHEATSHEET.md` and `docs/DEV_REFERENCE.md` are installed as of the v5 pass — USER_STORIES still holds template placeholders and wants a real pass. The one baseline doc that genuinely does not apply is BRAND.md: it is webapp-shaped (voice, visual direction, component styling) and this is an embedded tool with no UI. Written without backticks on purpose — the context checker reads a backticked path as a claim that it resolves, and would flag this sentence for being correct.

## Workflow Mechanisms

The shell's `## Micro Workflow` states what three steps must achieve and names a slot for how (DEC-S042). Filled below. **Slots, not overrides** — the shell states no webapp default to correct, and nothing here cites a step *number*, because numbers move and a stale cross-reference in an always-loaded file fails silently.

Soundings is firmware + a Python gateway + a wire contract. The mechanism is a **tiered test pyramid** (mirroring tinkle).

| Slot | This project |
|---|---|
| **Proof** | Native host tests for sensor math (tension/VPD), packet (de)serialization, and the run cycle, with **fake sensors and a fake clock**. Plus the **contract round-trip**: the C++ serializer and the Python parser checked against shared golden vectors, so the two ends of DEC-003's wire format cannot drift apart. No Playwright, no pgTAP — neither exists here. |
| **Proof command** | `pio test -e native` — the load-bearing tier. Escalate to the Wokwi + synthetic-node sim for the full fake-node → gateway → bus → DB → dashboard pipeline. |
| **Surface check** | **Bench first** — breadboard node, real radio, square-wave sensor stand-ins — then wet/field confirm with real parts in a tunnel. That is the final gate, and it is the step a green test suite cannot stand in for: the packet is the truth about what is on the device, not the repo. The only screen is a Grafana dashboard; there is no 375px viewport to check. |

**No proof, no push.** Run targeted tests freely during development; don't start a full or long run without saying so first.

## Migration Protocol (project)

**N/A — no Supabase, and no database at all.** D6 was **resolved by DEC-004**: soundings does not run a store. Persistence and dashboards belong to Poop Deck (`/home/eric/poop-deck`, TimescaleDB + Grafana, fed over MQTT); soundings publishes `contracts/publish-v1.md` documents and stops. `deploy/dev-sim/` is a local simulation stack, not production. The shell's Supabase toolchain, `safe-supabase.sh` guard (DEC-S009), and Vercel env-sync don't apply, and neither does any migration protocol — the schema soundings' data lands in is Poop Deck's to migrate.

## Conventions

- **Adapters everywhere a sensor or radio touches the world.** Real hardware behind an interface; a fake behind the same interface for simulation. Swapping one for the other is the entire point of the software-first approach.
- **Platform-independent logic lives in the core** so it compiles for both the ESP32 and the native test runner. Board-specific code stays out of it.
- **Raw readings are the durable record.** Default to putting raw values on the wire and deriving (kPa, VPD, gallons) downstream, so the math stays re-revisable against stored raw data without reflashing (D1).
- **Declared, not auto-detected (DEC-002).** A node knows its sensor set from a manifest; a missing expected reading is a fault, not a silent gap.
- **Constants in one place; bench-confirm the physical ones** (excitation timing, calibration coefficients, flow/volume curves) — spec defaults are seeds, not gospel.
- **C++ style:** prefer `constexpr` over `#define` for typed constants; comments explain *why*, not *what*. **Python style:** type hints, stdlib-first, handle malformed packets gracefully and log them — never crash the daemon on bad input.
- **Non-blocking firmware:** no `delay()` in the run path; time against `millis()`.

## Testing

Tiered, mirroring tinkle:
- **Native (host):** the load-bearing tier. Sensor math (tension/VPD), packet (de)serialization, run cycle — fake sensors + fake clock.
- **Contract round-trip:** C++ serializer and Python parser checked against shared golden vectors so they can't drift.
- **Sim (Wokwi + a synthetic-node emitter):** full pipeline — fake node → gateway → bus → DB → dashboard — no hardware.
- **Bench:** breadboard node, real radio, square-wave stand-ins for sensors.
- **Wet/field confirm:** real parts in a tunnel — the final gate.

### A refusal test needs a positive control in the same test

**If every assertion in a test is satisfied by the null value — `0`, `false`, `None`,
`[]`, "raises" — the test passes against an implementation that does nothing, and it is
green while pinning nothing.** Pair it with the adjacent legal case in the same test: the
length that *is* accepted, the config that *does* derive, the moment the timer *does*
expire. What you are pinning is the boundary, not a general willingness to say no.

This is written down because it kept happening — three occurrences in two consecutive PRs
before anyone looked, and nineteen more found in the existing suites when someone did.

**Upper bounds count too, and this is the part that surprises.** `assert ms <= limit` is
satisfied by `0`. The worst instance found: the clamp that stops a misconfigured node
deep-sleeping for 49 days was pinned by three tests that all asserted a sleep of **0** —
arithmetically correct at the low end of the jitter window, and exactly what a
do-nothing implementation returns. A sleep of 0 also means the node never sleeps, which
is the wake-forever failure DEC-006 names as the real battery killer. The one value those
tests agreed on was the value that would have been a disaster.

**Mutation is how you check, and grep is not.** Stub the function under test to return
its null value, run the suite, and see what still passes. Pattern-matching for
negative-shaped assertions misses the upper-bound case entirely. It costs one edit and
one test run, and it is worth doing on anything whose failure mode is a node that does
not come back.

## Versioning (project)

**There IS a `package.json`** — it builds nothing and carries no dependencies, but it holds the doc-integrity gates (DEC-S036/S037) and the version the workflow bumps. So the shell's version-bump steps in `/retro` and `/bump-major` **do** run here; they no longer no-op. `<VersionTag />` is still N/A — there is no UI to render one in. Packet payloads also carry their own firmware-version field (architecture, above), which is the version that matters operationally; the repo version is the workflow's, not the fleet's, and the two are unrelated.

## PR Workflow (project)

Follows the shell. **No `production` branch** unless a deployable surface appears — PRs ship to `main`; only `/promote-production` cares and it gates on `origin/production` (DEC-S022). Stacking PRs is preferred for dependent tasks.

## Median gaps

Where a competent default does the wrong thing in this repo.

| Gap | Why the default is wrong here |
|---|---|
| A green test suite is not evidence about the device | Everything above the bench runs against fakes — fake sensors, a fake clock, a synthetic packet source. That tier is load-bearing for logic and says nothing about whether a real node wakes, reads and transmits. The packet is the truth about the device; the repo is not |
| A test that only asserts a refusal passes against an implementation that does nothing | Covered at length under `## Testing` below, because it kept happening — including on the clamp that stops a node deep-sleeping for 49 days, pinned by three tests that all asserted a sleep of 0 |
| The node-secret file is untracked on purpose | firmware/node\_secret.ini carries WiFi credentials and the firmware-server address. Its absence from a fresh clone is the design, not a broken checkout, and the build compiles without it by design (empty strings). Written without backticks deliberately — `check:context` reads a backticked path as a claim that it resolves |
| There is no store here to migrate | Persistence, dashboards and their schema belong to Poop Deck. A schema instinct that fires in this repo is aimed at the wrong one |

## Workflow Notes (project)

- **Two toolchains, neither of them Node.** Firmware is PlatformIO (`pio test -e native`), the gateway is Python + pytest in `gateway/.venv`. The `package.json` at the root builds nothing — it hangs the doc gates and holds the version the workflow bumps, and `js-yaml` is its only dependency.
- **The wire contract has two ends and they fail apart silently.** A change to the C++ serializer or the Python parser that isn't matched in `contracts/` shows up as decoded garbage, not as a build error. The shared golden vectors are what catch it; run the round-trip whenever either side moves.
- **Poop Deck is a separate repo on disk** at `/home/eric/poop-deck`. Claims about what it stores or graphs come from opening its files, not from what soundings publishes.
