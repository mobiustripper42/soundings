# soundings — Project Context

Everything specific to **this** project. The seeds-managed `CLAUDE.md` shell reads this file at session start and treats it as authoritative for project-specific facts (DEC-S019). This is a **`tool`** project (embedded firmware + a Python gateway + dashboards), so the shell's webapp defaults — Playwright/pgTAP, Supabase migrations, 375px screenshots, `<VersionTag />`, `@ui-reviewer` — are overridden or N/A below. Nothing here syncs from seeds.

## What We're Building

**Soundings** is a LoRa wireless sensor mesh for Bay Branch Farm. Battery field nodes measure soil-moisture tension (the anchor measurement), soil temperature, canopy air temp/humidity (for VPD), and catchment tank level, and report over raw point-to-point radio to a gateway on the farm LAN. The gateway decodes the packets and stores them on an existing headless server for viewing in a dashboard. **V1 is read-only telemetry** — sensors observe and report, nothing actuates. A failed node means missing data, nothing worse.

Soundings is **one component of a larger farm recording/analysis tool** (daily log, harvest records, crop diagnosis, labor stats) under separate development. Cross-over gets raised as it arises — notably the time-series DB choice (see SPEC §12 D6).

**Sibling project — tinkle.** tinkle is the farm's irrigation controller (separate repo, firmware built, hardware build next). Soundings never controls anything; tinkle may eventually *consume* Soundings data (tank level for a pump-lockout; a future VPD advisory). The boundary is one-way and tinkle stays autonomous if Soundings is dark. Be aware tinkle exists; don't couple to it.

## Project Type

`tool` — embedded firmware (ESP32 field nodes) + a Python gateway/ingestion service + dashboards. **Not a webapp.** No Supabase, Next.js, React, RLS, or Playwright. `@ui-reviewer` and `VersionTag.tsx` are intentionally absent (gated out for `tool` type, DEC-S011 in seeds).

## Build Philosophy

- **Read-only telemetry (V1).** Measure and report; never actuate. Low-stakes by design (DEC-001-era scope in SPEC §2).
- **Nothing gets locked until it has to be (DEC-001).** Defer every decision to the moment simulation or the bench forces it. Deferred decisions live in the SPEC §12 register so they don't rot into surprises.
- **Software first, hardware second.** Build and prove the whole stack against simulation now, so the winter bench build is tuning, not authoring. Sensors and radios sit behind adapters; fakes drive them in simulation and get swapped for real drivers at the bench. This mirrors how tinkle's firmware was built before any hardware.
- **One firmware, declared sensors (DEC-002).** Every node runs the same binary; a per-node declared manifest says which sensors it has. Identity is data, not code.
- **Build crappy, learn, rebuild.** The first node will be ugly. Deploy it; 30 days of real operation teaches more than a month of planning.

## Stack

- **Field node:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 radio, one board), 2× 18650 (no solar), WiFi disabled, deep sleep. **[settled]**
- **Radio:** raw LoRa point-to-point (not LoRaWAN), US 902–928 MHz, tx every 10–15 min with ±30 s wake jitter. **[settled]**
- **Sensors:** Watermark soil tension (DIY AC excitation), DS18B20 soil temp (1-Wire), SHT45 air T/RH → VPD (I2C), A02YYUW ultrasonic tank level (UART).
- **Gateway:** a small always-on box near the farm center, on the LAN, holding the LoRa antenna; runs a Python decoder daemon (box choice deferred — D4).
- **Server stack:** existing headless Linux box. Message bus + time-series DB + Grafana — **all [proposed], unvalidated** (D6: TimescaleDB vs VictoriaMetrics unresolved). `mill-dev` (Tailscale VPS) is where we develop/simulate.
- **Toolchain:** **PlatformIO** (D5 resolved, Phase 1.1) — `node` (Heltec V3) + `native` (Unity host tests) envs in `firmware/platformio.ini`, mirroring tinkle.

See `docs/SPEC.md` for the full picture and the §12 deferred-decision register.

## Repo Layout (documented now; directories created in Phase 1 with the toolchain)

```
firmware/    ESP32 node firmware. Platform-independent core (host-testable
             sensor math, packet (de)serialization, run cycle) split from
             board-specific drivers, behind adapters with fakes for simulation.
gateway/     Python decoder daemon + ingestion: LoRa receive → decode → node→
             location map → message bus → time-series DB. Fakeable packet source.
contracts/   The binary packet schema — the contract between firmware (C++
             serializer) and gateway (Python parser), pinned by shared test
             vectors. The single source of truth both sides build against.
deploy/      docker-compose for the server stack (broker / DB / Grafana).
dashboards/  Grafana dashboard definitions.
```

## Architecture

Two code domains plus a wire contract between them:

- **Node firmware** — wake → sample sensors → assemble packet → transmit → sleep. Fully non-blocking; long actions time against `millis()`. Sensor drivers and the radio live behind adapter interfaces (`ISoilMoisture`, `ITemp`, `IHumidity`, `IRadio`, `IClock`) so fakes drive them in host tests / Wokwi and real drivers swap in at the bench. Platform-independent logic lives in the core so it compiles for both the ESP32 and the native test runner.
- **Gateway/ingestion** — the LoRa source is an adapter too (`IPacketSource`), faked with synthetic/replayed packets so the whole pipeline runs on a laptop with zero hardware.
- **The packet schema is the contract.** A C++ serializer and a Python parser that must never drift — pinned by shared golden-vector round-trip tests. Versioned (every packet carries a firmware-version field).

## Commands

Firmware is PlatformIO (D5 resolved). Gateway and server-stack commands firm up as those layers land in Phase 1.

```bash
# Firmware (run from firmware/)
pio test -e native           # host unit tests (core sensor/packet logic) — the load-bearing tier
pio run  -e node             # build node firmware (Heltec WiFi LoRa 32 V3)

# Gateway (Python) — lands in Phase 1.4
# python -m pytest           # gateway + contract round-trip tests

# Server stack (simulation) — lands in Phase 1.5
# docker compose -f deploy/docker-compose.yml up   # broker + DB + Grafana
```

## Additional Docs

| File | Purpose |
|------|---------|
| `docs/tank-level-sensor.md` | Detail spec for the tank-level sensor (folded into SPEC §5.4) |

Notes on the baseline docs: `docs/SPEC.md` carries the **§12 deferred-decision register** (D1–D6) — the home for every not-yet-locked choice (DEC-001). `docs/RETROSPECTIVES.md` uses **throughput velocity (DEC-S026)**. Baseline docs the shell lists that **don't apply here:** no `docs/BRAND.md`, `docs/USER_STORIES.md`, or `docs/CHEATSHEET.md` (embedded tool).

## Workflow Overrides

The shell's `## Micro Workflow` is webapp-shaped (Playwright + pgTAP + 375px screenshot). Soundings is firmware + a Python gateway + a wire contract — those steps are replaced by the **tiered test pyramid** (mirroring tinkle):

- **Step 5 (Write the test):** native host tests for sensor math (tension/VPD), packet (de)serialization, and the run cycle, with **fake sensors and a fake clock**. **Contract round-trip:** the C++ serializer and Python parser checked against shared golden vectors so they can't drift. No Playwright, no pgTAP.
- **Step 6 (Run targeted tests):** `pio test -e native` (the load-bearing tier); escalate to the Wokwi + synthetic-node sim for the full fake-node → gateway → bus → DB → dashboard pipeline. Bench (breadboard node, real radio, square-wave sensor stand-ins) then wet/field confirm (real parts in a tunnel) is the final gate.
- **Step 7 (Mobile screenshot):** N/A — the only UI is Grafana dashboards.
- **`No test, no push.`** Run targeted tests freely during development; don't run a full/long suite without saying so first.

## Migration Protocol (project)

**N/A — no Supabase.** The time-series DB is itself a deferred decision (SPEC §12 D6: TimescaleDB vs VictoriaMetrics, unresolved); the server stack is broker + TSDB + Grafana via `deploy/docker-compose.yml`, not a Supabase/migrations setup. The shell's Supabase toolchain, `safe-supabase.sh` guard (DEC-S009), and Vercel env-sync don't apply.

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

## Versioning (project)

**No `package.json`**, so the shell's version-bump steps in `/retro` / `/bump-major` no-op silently. `<VersionTag />` is N/A. Packet payloads carry their own firmware-version field (architecture, above) — that's the versioning that matters operationally; a repo version surface can be added later if needed.

## PR Workflow (project)

Follows the shell. **No `production` branch** unless a deployable surface appears — PRs ship to `main`; only `/promote-production` cares and it gates on `origin/production` (DEC-S022). Stacking PRs is preferred for dependent tasks.

## Model Selection

Soundings follows the shell's `## Model Selection` (DEC-S027) **as-is** — Opus 4.8 the standing model, Sonnet for cheap/scoped work, Fable an on-demand bundle escalation. `@architect` is pinned to **Opus 4.8** (`.claude/agents/architect.md` frontmatter, matching the shell default — no override). Project-flavored Fable-bundle candidates: the whole simulation spine, or the node firmware core end to end.

## Approach to Action (project override)

**This overrides the shell's `## Approval Before Action` / `## Bug Reports & Questions` gates.** Soundings defaults to action: for non-trivial or destructive work, say what you're about to do and why in a sentence, then proceed — **don't stall for approval on local, reversible, diagnostic steps** (builds, tests, sim runs, file edits). Reserve explicit confirmation for the genuinely consequential: **flashing hardware, force-pushes, anything touching shared/remote state, anything hard to reverse.**

Check `docs/SPEC.md` "Not V1" before adding scope. If a task feels bigger than its estimate: stop, re-estimate; if it's scope creep, flag and move on. Still break genuine 13s.
