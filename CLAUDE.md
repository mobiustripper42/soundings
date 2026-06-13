# soundings — Claude Code Project Context

## What We're Building

**Soundings** is a LoRa wireless sensor mesh for Bay Branch Farm. Battery field
nodes measure soil-moisture tension (the anchor measurement), soil temperature,
canopy air temp/humidity (for VPD), and catchment tank level, and report over
raw point-to-point radio to a gateway on the farm LAN. The gateway decodes the
packets and stores them on an existing headless server for viewing in a
dashboard. **V1 is read-only telemetry** — sensors observe and report, nothing
actuates. A failed node means missing data, nothing worse.

Soundings is **one component of a larger farm recording/analysis tool** (daily
log, harvest records, crop diagnosis, labor stats) under separate development.
Cross-over gets raised as it arises — notably the time-series DB choice (see
SPEC §12 D6).

**Sibling project — tinkle.** tinkle is the farm's irrigation controller
(separate repo, firmware built, hardware build next). Soundings never controls
anything; tinkle may eventually *consume* Soundings data (tank level for a
pump-lockout; a future VPD advisory). The boundary is one-way and tinkle stays
autonomous if Soundings is dark. Be aware tinkle exists; don't couple to it.

## Project Type

`tool` — embedded firmware (ESP32 field nodes) + a Python gateway/ingestion
service + dashboards. **Not a webapp.** No Supabase, Next.js, React, RLS, or
Playwright. `@ui-reviewer` and `VersionTag.tsx` are intentionally absent
(gated out for `tool` type, DEC-S011 in seeds).

## Build Philosophy

- **Read-only telemetry (V1).** Measure and report; never actuate. Low-stakes by
  design (DEC-001-era scope in SPEC §2).
- **Nothing gets locked until it has to be (DEC-001).** Defer every decision to
  the moment simulation or the bench forces it. Deferred decisions live in the
  SPEC §12 register so they don't rot into surprises.
- **Software first, hardware second.** Build and prove the whole stack against
  simulation now, so the winter bench build is tuning, not authoring. Sensors and
  radios sit behind adapters; fakes drive them in simulation and get swapped for
  real drivers at the bench. This mirrors how tinkle's firmware was built before
  any hardware.
- **One firmware, declared sensors (DEC-002).** Every node runs the same binary;
  a per-node declared manifest says which sensors it has. Identity is data, not
  code.
- **Build crappy, learn, rebuild.** The first node will be ugly. Deploy it; 30
  days of real operation teaches more than a month of planning.

## Stack

- **Field node:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 radio, one board),
  2× 18650 (no solar), WiFi disabled, deep sleep. **[settled]**
- **Radio:** raw LoRa point-to-point (not LoRaWAN), US 902–928 MHz, tx every
  10–15 min with ±30 s wake jitter. **[settled]**
- **Sensors:** Watermark soil tension (DIY AC excitation), DS18B20 soil temp
  (1-Wire), SHT45 air T/RH → VPD (I2C), A02YYUW ultrasonic tank level (UART).
- **Gateway:** a small always-on box near the farm center, on the LAN, holding
  the LoRa antenna; runs a Python decoder daemon (box choice deferred — D4).
- **Server stack:** existing headless Linux box. Message bus + time-series DB +
  Grafana — **all [proposed], unvalidated** (D6: TimescaleDB vs VictoriaMetrics
  unresolved). `mill-dev` (Tailscale VPS) is where we develop/simulate.
- **Toolchain:** firmware build environment (PlatformIO vs Arduino IDE) is
  **deferred (D5)** until the firmware skeleton goes up in Phase 1.

See `docs/SPEC.md` for the full picture and the §12 deferred-decision register.

## Key Docs

| File | Purpose |
|------|---------|
| `docs/SPEC.md` | What we're building — scope, sensors, architecture, **§12 deferred-decision register** |
| `docs/DECISIONS.md` | Architectural decisions (DEC-NNN) worth preserving the reasoning for |
| `docs/PROJECT_PLAN.md` | Phases + descriptions; current-phase tasks live as GitHub Issues |
| `docs/RETROSPECTIVES.md` | Phase-end retros (throughput velocity, DEC-S026) |
| `docs/AGENTS.md` | Agent + skill specs |
| `docs/tank-level-sensor.md` | Detail spec for the tank-level sensor (folded into SPEC §5.4) |
| `docs/VELOCITY_AND_POKER_GUIDE.md` | Estimation methodology |
| `sessions/*.md` (orphan `sessions` branch via `.sessions-worktree/`) | Per-session files (DEC-S013/S014) |
| `.claude/seeds-version` | Schema version for `/pull-seeds` (currently `4`) |
| `.claude/project-type` | `tool` |

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

- **Node firmware** — wake → sample sensors → assemble packet → transmit → sleep.
  Fully non-blocking; long actions time against `millis()`. Sensor drivers and
  the radio live behind adapter interfaces (`ISoilMoisture`, `ITemp`,
  `IHumidity`, `IRadio`, `IClock`) so fakes drive them in host tests / Wokwi and
  real drivers swap in at the bench. Platform-independent logic lives in the core
  so it compiles for both the ESP32 and the native test runner.
- **Gateway/ingestion** — the LoRa source is an adapter too (`IPacketSource`),
  faked with synthetic/replayed packets so the whole pipeline runs on a laptop
  with zero hardware.
- **The packet schema is the contract.** A C++ serializer and a Python parser
  that must never drift — pinned by shared golden-vector round-trip tests.
  Versioned (every packet carries a firmware-version field).

## Commands

Provisional — the firmware toolchain is deferred (D5), so build commands firm up
in Phase 1. Intended shape:

```bash
# Firmware (toolchain TBD — PlatformIO shown as the likely shape)
# pio run -e node            # build node firmware
# pio test -e native         # host unit tests (core sensor/packet logic)

# Gateway (Python)
# python -m pytest           # gateway + contract round-trip tests

# Server stack (simulation)
# docker compose -f deploy/docker-compose.yml up   # broker + DB + Grafana
```

## Conventions

- **Adapters everywhere a sensor or radio touches the world.** Real hardware
  behind an interface; a fake behind the same interface for simulation. Swapping
  one for the other is the entire point of the software-first approach.
- **Platform-independent logic lives in the core** so it compiles for both the
  ESP32 and the native test runner. Board-specific code stays out of it.
- **Raw readings are the durable record.** Default to putting raw values on the
  wire and deriving (kPa, VPD, gallons) downstream, so the math stays re-revisable
  against stored raw data without reflashing (D1).
- **Declared, not auto-detected (DEC-002).** A node knows its sensor set from a
  manifest; a missing expected reading is a fault, not a silent gap.
- **Constants in one place; bench-confirm the physical ones** (excitation timing,
  calibration coefficients, flow/volume curves) — spec defaults are seeds, not
  gospel.
- **C++ style:** prefer `constexpr` over `#define` for typed constants; comments
  explain *why*, not *what*. **Python style:** type hints, stdlib-first, handle
  malformed packets gracefully and log them — never crash the daemon on bad input.
- **Non-blocking firmware:** no `delay()` in the run path; time against `millis()`.

## Testing

Tiered, mirroring tinkle:

- **Native (host):** the load-bearing tier. Exercise sensor math (tension/VPD),
  packet (de)serialization, and the run cycle with fake sensors and a fake clock.
- **Contract round-trip:** the C++ serializer and Python parser checked against
  shared golden vectors so they can't drift.
- **Sim (Wokwi + a synthetic-node emitter):** full pipeline — fake node →
  gateway → bus → DB → dashboard — with no hardware.
- **Bench:** breadboard node, real radio, square-wave stand-ins for sensors.
- **Wet/field confirm:** real parts in a tunnel — the final gate.

**No test, no push.** Run targeted tests freely during development; don't run a
full/long suite without saying so first.

## Session Skills

| Skill | When | What |
|-------|------|------|
| `/its-alive` | Session start | Ensure `.sessions-worktree/`, open session file on `sessions` branch, capture transcript, recommend task |
| `/pause-this` · `/restart-this` | Mid-session break / resume | Build check + WIP commit; reload context |
| `/kill-this` | Per task (DEC-S013) | Build check, commit, @code-review, open PR, append `## Task <N>` to the session file |
| `/its-dead` | Session end (once) | Stamp `ended:`, tally points, close session file, push |
| `/start-phase` · `/retro` | Phase boundaries | Materialize a phase as Issues / compute throughput velocity (DEC-S026), write retro |
| `/push-seeds` · `/pull-seeds` | Workflow sync | Backport to / pull from seeds templates via @sync-config (schema-version gated) |
| `/read-the-tape` | After a notable session | Audit JSONL for anti-patterns via @tape-reader |
| `/doc-consistency-check` | Before phase boundaries | Cross-reference doc claims (report-only) |

**Task model:** PROJECT_PLAN.md is read at planning, written at retro;
current-phase tasks are GitHub Issues; a phase ends when its issues close.

## Agents

| Agent | Model | When | Purpose |
|-------|-------|------|---------|
| @architect | Opus 4.8 | Before design decisions, new deps, scope creep | Coherence vs SPEC + DECISIONS |
| @code-review | Sonnet | After commits (wired into `/kill-this`) | Catch issues early |
| @pm | Sonnet | Session start/end | Track progress, flag risks |
| @sync-config | Sonnet | `/push-seeds`, `/pull-seeds` | Classify template-vs-project diffs |

(`@ui-reviewer` omitted — `tool` type. `@tape-reader` / `@doc-consistency` can be
copied from seeds if/when their skills are used.)

## Model Selection (DEC-S027)

Default to the cheapest model that does the job. **Opus 4.8 is the standing
model** for real development and architecture; Sonnet handles cheap/scoped work;
Fable is a deliberate, on-demand escalation for *bundled* long-horizon work —
never the default ($10/$50 per MTok, 2× Opus, drains usage fast).

| Tier | Model | Use for |
|------|-------|---------|
| Cheap | `claude-sonnet-4-6` | Trivial/scoped agents and reviews — fast, low-cost. |
| Default | `claude-opus-4-8` | The standing model for development and architecture. Most work runs here. |
| Frontier (on demand) | `claude-fable-5` | A *bundled* long-horizon unit — several related tasks combined into one coherent multi-file run (e.g. the whole simulation spine, or the node firmware core end to end). Spawned deliberately and scope-confirmed. One-off task → stay on Opus. |

**The Fable trigger — bundle, then escalate.** Fable's lead is largest on long,
coherent, multi-file work, which is also where its cost amortizes across the most
output — so don't route individual tasks to it.
- When several related tasks form one coherent unit, **Claude proposes** bundling
  them into one Fable run *before* starting, with the scope.
- The **operator can request the same**: say `bundle for fable`. Either party raises it.
- A Fable run is opt-in and announced — confirm scope before spawning, give it the
  full combined spec up front, run it at high effort.

- **Reach for `effort` before reaching for a tier.** `xhigh` is the floor for
  coding/agentic work; `high` for intelligence-sensitive work; `max` only when
  correctness must beat cost.
- **File memory is a force multiplier** (~3× more on Fable). Keep SPEC, DECISIONS,
  the §12 register, and acceptance criteria current; reference them explicitly —
  matters most on a bundled Fable run.
- **Vision.** Fable is state-of-the-art at vision — a legitimate reason to escalate
  a unit heavy on datasheet figures, wiring diagrams, or bench photos.
- **Agents:** `@architect` runs Opus 4.8, escalating to a Fable run only for
  genuinely hard or bundled design work. Reviewers (`@code-review`, `@pm`,
  `@sync-config`) stay Sonnet. New agents default to Sonnet; pin `model: opus`
  only when the standing job needs it — don't pin Fable.

## PR Workflow

- Each task gets a branch: `git checkout -b task/X.Y-short-description`.
- `/kill-this` opens the PR (`closes #N`), runs @code-review. Keep ≤3 open PRs.
- **Stacking PRs is preferred** when tasks depend on each other.
- PRs ship to `main`. No `production` branch unless a deployable surface appears.
- **Never rebase a task branch that already has commits on origin** — use
  GitHub's "Update branch" at merge time.

## Workflow Notes

- **Diagnostic commands** (build, lint, test, sim runs): run directly.
- **Environment-changing commands** (installs, deploys, flashing hardware,
  pushes): say what you're doing; confirm before the genuinely consequential.
- **Bug reports:** create a GitHub issue, label `bug`, add to current/next phase.
- **JSON in Bash:** prefer `gh ... --jq` or `jq` over python one-liners.

## Approach to Action

Default to action. For non-trivial or destructive work, say what you're about to
do and why in a sentence, then proceed — don't stall for approval on local,
reversible, diagnostic steps. Reserve explicit confirmation for the genuinely
consequential: flashing hardware, force-pushes, anything touching shared/remote
state, anything hard to reverse.

Check `docs/SPEC.md` "Not V1" before adding scope. If a task feels bigger than
its estimate: stop, re-estimate; if it's scope creep, flag and move on. Still
break genuine 13s.

## Tone

Occasional dry humor welcome. One good line beats three forced ones. Skip
disclaimers; be meticulous.

## Response Length

Default to the shortest response that fully answers — usually 2–5 sentences. No
preamble, no restating the question, no reflexive offers to help further. Offer
concrete follow-ups when they'd save a round-trip. Length is requested explicitly
("expand," "the long version"), never the default.

## Verbosity

End-of-turn summaries: one or two sentences — what changed, what's next. Don't
recap work just watched. Mid-session updates: one sentence per state change.

## Narration

`terse` (default): silence between tool calls; one sentence when you find
something, change direction, or hit a blocker. `normal`: brief progress notes at
meaningful steps. `narrate`: explain reasoning as you go. Switch any time:
`narration: terse|normal|narrate`.

## Cost and Waste

Never minimize cost. Banned phrasings (and any synonym whose function is to
minimize): "essentially zero", "negligible", "only a few cents", "just X
dollars", "a rounding error", "not a big deal", "don't worry about it". It's real
money and real resources. Waste of any kind is a fact, not a problem to console
about: acknowledge it and move on. No reassurance.
