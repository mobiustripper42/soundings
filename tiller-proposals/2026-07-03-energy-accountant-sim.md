# Tiller: Give the run cycle a power budget it can fail a build over

*Overnight idea from Tiller for **soundings**. Draft only — you're the gate. Full write-up also committed at `tiller-proposals/2026-07-03-energy-accountant-sim.md`.*

---

## The pitch

### The idea

The whole project rests on one number nothing in the repo can see: **~2 years of
battery life, no solar, one service window a year.** That number is a spec guess
(SPEC §4 — "~0.15–0.25 mA average"), and it is *entirely a function of code you
have not written yet* — the Phase-2 run cycle (#23): how long the radio stays on,
how many times the AC-excitation loop samples, whether anything busy-waits instead
of sleeping. Today that budget is invisible to the sim. The bench multimeter in
winter is the first — and only — time anything measures it.

The idea is an **energy accountant** riding the seam that already exists. The
`FakeClock` already advances time deterministically through the run cycle; give
each fake adapter (the radio, each sensor, the MCU active/sleep states) a
**labeled current draw**, and integrate current × time-in-state as the cycle runs.
Out comes a per-cycle **energy and state-occupancy profile**: radio-on
milliseconds, sample count, active-state duration, blocking-spin count, µAh drawn.

The load-bearing half — the part that survives every objection — is a **host-test
assertion on that profile.** A native-tier test that says *"one wake cycle must
not exceed N µAh / keep the radio on longer than M ms / spin-wait at all."* If a
Phase-2 change leaves the radio powered 200 ms too long, over-samples the
Watermark, or slips a `delay()` into the run path, `pio test -e native` **goes
red at commit** — not eleven months later when a node dies in the field. The
absolute "2.3 yr projected" figure comes out too, but it stays a **labeled,
bench-pending reporting artifact — never a gate** (see *Gotchas*: its dominant
term is hardware quiescent draw the sim can only guess). The test guards the
primitives the *firmware* controls; those are coefficient-invariant.

### Why it's worth it

This is a **no-solar, annual-swap, two-year-claim** device. If the power budget is
wrong, the entire value proposition — set it and forget it, one visit a year —
collapses, and there is currently **no tripwire** between "a commit quietly doubled
the active-cycle cost" and "the back-forty node went dark in February." A once-a-
year multimeter is not a substitute for a commit-time gate.

Why *now*: the run cycle is **unstarted** (#23). Build the accountant alongside it
and every timing decision in that code is born with a visible cost and a golden
baseline — a PR that costs 15% more energy shows up as a *number in review*. This
is the project's own stated thesis — *"the bench is tuning, not authoring"* —
extended from sensor data to power: the bench then *tunes the coefficients*
against a model that already exists, instead of discovering the power story from
scratch with a meter.

### Why he hasn't already

Because power *feels* like a bench measurement — you reach for a multimeter, and
DEC-001 ("nothing locks until it has to") reads like a direct order to defer every
current figure to the bench. But that conflates two things the proposal separates:
the *coefficients* are genuinely bench-pending (seed them, same as calibration
constants), while the *thing worth testing* — time-in-state per cycle — is a pure
function of the run-cycle code and is knowable the moment that code exists. The
sim so far has been asked "does the data flow?", and on that question it looks
finished. Nobody framed battery life as a property of the firmware's control flow,
testable at the native tier off the fake clock that's already sitting there. It's
one reframe past where "the spine works" naturally stops — the same shape as the
project's other good calls, hiding because the obvious mental model files power
under "hardware."

---

## Build handoff

**Scale:** a contained firmware-core feature, best built *with* the Phase-2 run
cycle (#23) rather than bolted on after. No new dependency, no wire-contract
change, no gateway change. First slice is one coherent subsystem; the deferred
facets are sketched so they don't get lost.

### Approach — the shape and the key decisions

- **The accountant is an energy-aware clock, not a new seam.** The cleanest fit is
  an `EnergyClock` that *wraps* `IClock`: the run cycle already advances time
  through the clock, so every `advance()`/state transition is exactly where charge
  should accrue. It records time-in-state and, given a current for the active
  state, integrates µAh. Host tests inject it in place of (or wrapping) `FakeClock`;
  the ESP32 build never references it — **zero on-target cost, zero hot-path cost.**
- **States, not guesses.** Model the cycle as a small state enum — `SLEEP`,
  `MCU_ACTIVE`, `RADIO_TX`, `SENSOR_READ` (extend as the run cycle defines them).
  The run cycle *declares* which state it's in as it runs (a one-line
  `energy.enter(RADIO_TX)` at the transition it already makes); the accountant owns
  the arithmetic. This is what makes the guard coefficient-invariant: it asserts on
  **occupancy** (ms in `RADIO_TX`, count of `SENSOR_READ`, any `SLEEP`-that-should-
  have-happened-but-didn't), which a wrong current can't move.
- **Two outputs, two very different trust levels.**
  1. **State-occupancy profile → hard CI assertion.** `radio_on_ms`,
     `sensor_read_count`, `active_ms`, `blocking_spins`. These get real
     `TEST_ASSERT` ceilings. Coefficient-free.
  2. **µAh / projected-life → reporting artifact, provenance-stamped
     `seed, bench-pending`, never asserted.** Printed in the test log and dumped to
     a golden baseline for diffing; it exists to tie code to the 2 yr claim, not to
     gate a build.
- **Currents live in one labeled table, seeded from datasheets, marked for bench
  confirmation** — exactly the "spec defaults are seeds, not gospel" pattern
  (CLAUDE.md Conventions). One `constexpr` table in core with a comment per row
  citing its datasheet source and a `// BENCH-CONFIRM (D-power)` tag.

### File-by-file (firmware core; first slice)

- **`firmware/src/core/energy.h`** (new) — `enum class PowerState`; a
  `PowerModel` (the labeled current-per-state table, `constexpr`); an `EnergyClock`
  that implements `IClock`, wraps an inner clock, and on each state entry/clock
  advance accrues `state_ms[state]` and `micro_amp_hours`. Query methods:
  `radioOnMs()`, `readCount()`, `activeMs()`, `blockingSpins()`, `microAmpHours()`,
  `projectedLifeDays(cyclesPerDay, sleepFloorUa)`.
- **`firmware/src/core/`** run-cycle file (the #23 work) — at each existing
  transition, a single `energy.enter(PowerState::…)` call. If a spin-wait is ever
  hit in the run path, `energy.noteBlockingSpin()`. These are the *only* touches to
  production logic, and they read as documentation of what state the node is in.
- **`firmware/test/test_native/fakes/`** — no new fake needed; `EnergyClock`
  composes over the existing `FakeClock`. Optionally an `EnergyRadio`/sensor spy
  only if a state can't be inferred from the clock alone (prefer inference).
- **`firmware/test/test_native/test_energy.cpp`** (new) — the crown jewel:
  - a nominal cycle stays under the µAh ceiling **and** `radio_on_ms ≤ M`,
    `sensor_read_count == expected`, `blocking_spins == 0`;
  - a deliberately sabotaged cycle (radio left on / extra sample / a `delay`) trips
    the occupancy ceiling **with the current table both halved and doubled** — the
    test that *proves* the guard is coefficient-invariant;
  - the projected-life number is computed and **printed, not asserted** (guard
    against someone later turning it into a gate).
- **`docs/SPEC.md` §12** — add a deferred-decision row **D-power**: the per-state
  current coefficients + sleep-floor quiescent draw are seeded from datasheets,
  bench-confirmed at Phase 5; note that the *occupancy ceilings* are already live.

### Gotchas / risks

- **The GIGO trap is real — and it lives entirely in the projected-life number.**
  Two-year life is dominated by the always-on **sleep-floor quiescent draw**
  (regulator + deep-sleep leakage + sensor standby over ~17,500 h), a *hardware*
  term datasheets routinely under-state 2–3×. Any absolute projection inherits that
  error. This is *the* reason projected-life must never gate a build; the
  occupancy ceilings, which don't depend on it, are what you actually assert.
- **False confidence is the failure mode to design against.** A "2.3 yr projected"
  line someone *believes* is worse than no number. Stamp it `seed, bench-pending`
  everywhere it appears; keep it out of any assertion.
- **Why the accountant earns its place over a thin radio spy.** A one-line
  `IRadio` spy ("radio on ≤ N ms") reaches *some* of the regression value. The
  accountant justifies itself as the **aggregator** — one scalar guarding the whole
  cycle plus the single human-facing number that ties the run-cycle code to the
  2 yr claim — not as gold-plating over a spy. If the run cycle turns out trivial
  enough that a spy suffices, take the spy; the reframe (power = testable property
  of the run cycle) is the real deliverable, the accountant is its cleanest vehicle.
- **Keep it inference-first.** Derive state time from the clock the run cycle
  already drives; add spy hooks only where a state genuinely can't be inferred.
  Every production-code touch should be a state *declaration*, never accounting
  logic leaking into the run path.
- **Don't model what you can't yet see.** Radio TX energy depends on SF/BW/power
  (D3, deferred). Seed a single labeled TX current now; the SF/BW-swept version is
  a later refinement, not this slice — same discipline as everywhere else here.

### Done when

- `pio test -e native` includes an energy test that **fails** when a cycle exceeds
  its state-occupancy ceilings (radio-on ms, sample count, active ms, blocking
  spins), and **passes** those same assertions with the current coefficients both
  halved and doubled — coefficient-invariance demonstrated, not asserted.
- The nominal Phase-2 run cycle reports a per-cycle µAh + projected-life figure to
  the test log and a golden baseline, every appearance stamped `seed, bench-pending`.
- No production code carries accounting logic — only single-line state
  declarations; the ESP32 build never links `EnergyClock`.
- SPEC §12 carries **D-power** (coefficients bench-pending; occupancy ceilings
  live now).

### Kickoff

> We're giving the soundings run cycle a power budget it can fail a build over.
> Read `tiller-proposals/2026-07-03-energy-accountant-sim.md`. Build it *with* the
> Phase-2 run cycle (#23), not after. First slice: an `EnergyClock` wrapping the
> existing `IClock`/`FakeClock` that accrues time-in-state (SLEEP / MCU_ACTIVE /
> RADIO_TX / SENSOR_READ) and integrates µAh from a labeled, datasheet-seeded
> current table in core. The deliverable is a native-tier test that asserts on
> **state occupancy** (radio-on ms, sample count, blocking spins) — coefficient-
> invariant — and merely *reports* the projected-life number, provenance-stamped
> and never asserted. Prove invariance by passing the occupancy assertions with the
> current table both halved and doubled. Add a D-power row to SPEC §12. Plan it and
> wait for my go before writing code.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01S5NtsWuXkzjPcsBjzJZHXw
