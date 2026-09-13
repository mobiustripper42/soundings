# soundings — Phase Retrospectives

Written at each phase boundary by `/retro`. Velocity is **throughput (points per
calendar week)** computed from GitHub issue `closedAt` dates + `points:` labels
(DEC-S026) — not hours/point.

Format per entry: throughput, estimate calibration, scope changes, what worked,
what didn't, forecast update.

---

## Phase 2 — Node firmware core (simulated)

**Closed:** 2026-09-11

**Points:** 29 / 29 (100%)
**Span:** 86 days (2026-06-17 → 2026-09-11)
**Throughput:** **not quoted — and the reason is the finding.** Phase 2 was parked
on 2026-08-07 (DEC-005), overtaken by Phase 3, and finished as leftovers. 22 of its
29 points closed in a single sitting on 2026-09-11, months after the code shipped.
The 86-day span is filing lag, not work. A per-week rate computed over it deflates
into nonsense for the same reason the sub-week rule exists in the other direction.
**Estimate calibration:** 1 task re-estimated, net drift −1 pt
**Sessions:** 16 in the window · **PRs merged:** 58 in the window — ⚠ **both
contaminated.** The window contains all of Phase 3's work: the tank node, the radio
link, OTA, the DS18B20 fixes. Attributing 58 PRs to Phase 2 would credit it with
almost none of its own work. Recorded as raw facts, not as Phase 2's cost.
**Issues:** 7 created, 7 closed, 0 moved to Phase 3

### Phase throughput line
| Phase | Date | Points | Span(d) | Throughput | Re-est'd | Net drift | Sessions | PRs |
|-------|------|--------|---------|------------|----------|-----------|----------|-----|
| 2 | 2026-09-11 | 29 | 86 | not quoted (interleaved) | 1 | −1 | 16* | 58* |

\* contaminated by Phase 3 — see above.

### What worked
- "I was able to move the project forward without waiting for hardware, farming
  season ends I'm ready to build"

### What didn't
- "Priorities changed and I had to stop work."

### Changes for next phase
- "nothing"

### Scope changes
- **Issue #21 re-pokered 3 → 2.** Reads as a firmware task and almost none of it
  was: `packet-v1.md:82-83` already defined bits 6/7 as SHT45 ticks with the exact
  conversions, `packet.h:31` sized them, both parsers named them. Only the curve was
  missing, and DEC-004 puts that gateway-side in Python. The only calibration data
  point the phase produced.
- **Issue #19 — `IHumidity` deferred to Phase 6.1.** No SHT45 driver and no
  tunnel-air preset, so it would be an interface with a fake behind it and nothing
  on either end. Sixteen other interfaces shipped with matching fakes.
- **Issue #24 — bed/tunnel-air/rig presets deferred to Phase 6.1.** Three of four
  criteria shipped; only `tankPreset` exists. The issue said "one coherent unit, no
  split" and was split anyway by issue #44 (Phase 3.5), deliberately. A bed preset
  written today declares four Watermark channels with no drivers and faults all four.
- **Issue #25 — Wokwi killed.** Its value was highest before there were boards on
  the bench. There are. It would cover the ESP32 binding layer, the one layer host
  tests cannot reach — but the defect that layer actually produced (issue #94:
  `pinMode()` costing ~14 µs inside a 15 µs 1-Wire write slot) is a real-silicon
  timing artifact a simulator would not reproduce faithfully.
- **Issue #98 closed as speculative** during the phase — the condition it existed to
  surface has never been observed.

All four deferrals are the same move: **don't build a seam with nothing on either
end.** That discipline is why Phase 2 could be parked in August at zero cost.

### PM read

Pace. 29 points, 86 calendar days, and no honest rate to quote — that call is
correct. The more useful number is the shape of the gap: Phase 2's issues were
created 2026-06-17, and the session log shows session 1 closing 2026-06-20, then
session 2 (2026-07-10, never closed out, zero points logged), then nothing until
session 3 on 2026-08-10. Seven weeks, one abandoned session. That isn't slow work,
it's no work, and the distinction matters because it means Phase 2's estimates were
never actually tested against effort. The one data point we have — #21 delivered at
2 instead of 3 — came from a task inspected in September, not built in June. The
point unit that went six-for-six in Phase 1 is still, functionally, untested at
Phase 2 scale.

Scope. The deferrals are all defensible and all the same move: don't build a seam
with nothing on either end. `IHumidity` waiting for the SHT45 driver, the
bed/tunnel-air/rig presets waiting for the drivers that fill them, #98 closed
because the condition never appeared — that's three instances of refusing to build
speculatively, which is a genuinely hard discipline and the reason Phase 2 could be
parked in August at zero cost. Killing Wokwi is the sharpest of them. The stated
reason is exactly right and worth preserving: the one defect the simulated layer
produced was a real-silicon GPIO timing artifact, and a simulator wouldn't have
reproduced it. Session 17 documents the same lesson from the other direction — 22
green host tests over a DS18B20 path that had never once worked on a board, and a
fake "more honest than the hardware." Simulation bought Phase 1 and Phase 2 and then
stopped paying.

The pattern nobody has written down yet. Phase 3 was planned at 47 points across 13
tasks. The issues actually total 85 points across 19, with 71 closed. Task 3.8 was
estimated at 3 and shipped as #47 + #71 + #94 = 14. Task 3.9 was estimated at 5 and
shipped as #48 + #73 + #76 + #79 + #85 = 32. Some of that is real scope addition
(OTA, manifest signing), but the direction is unambiguous: every hardware task
fissions on contact with silicon. Phase 2's estimates held to −1 point because Phase
2 never touched hardware. That is the single most important fact going into what's
left, and it has nothing to do with the retro's headline calibration figure.

On the three answers. "Nothing changes" is legitimate here and I'd have been more
worried by an elaborate answer. The failure mode was external — farming season — and
inventing a process fix for a calendar problem is how retros become theater. But be
honest about what the evidence does and doesn't support. "I was able to move the
project forward without waiting for hardware" is true and was the entire design
thesis, and it has now expired: the software-first tier is done, and the next four
issues are a crimping tool, a drill, and a tape measure. "Priorities changed and I
had to stop work" is stated as past tense; the plan's critical path (Red Tunnel,
~March 2027) assumes it stays past tense. That assumption, not the estimates, is the
live risk.

Forward. #49 Power, #50 Enclosure, #51 First light, #52 Calibration — 14 points
nominal, all four physical, and #49 already inherits work: issue #94 closed with
bench check 4 (sleep current) unmeasured, and sleep current is #49's job now. Given
the 3.8/3.9 expansion, treat 14 as a floor, not an estimate. The sequence is forced
anyway — #49 and #50 gate #51, and #52 is calendar-gated on actual tank fills, so it
will trail regardless and should not be allowed to hold the phase open. One concrete
ask for the next retro: log sessions properly through the hardware run. Session 2's
empty `points:` and the unclosed sessions 4, 15, 16 are why Phase 2's velocity is
unrecoverable, and hardware work is exactly where per-task actuals are worth the
most.

> Correction applied to the PM read as delivered: bench check 3 **was** measured —
> the probe read −1.00 °C in the freezer. Only check 4, sleep current, is
> outstanding. Session 17 was closed properly at the 2026-09-13 retro resume.

---

## Phase 1 — The contract & the simulation spine

**Closed:** 2026-06-17
**Span:** 3.5 days (2026-06-14 → 2026-06-17) · **Points:** 22/22 (100%) · **PRs merged:** 7
**Issues:** 6 created, 6 closed, 0 moved

### Throughput

| Metric | Value |
|--------|-------|
| Points closed | 22 |
| Span | 3.5 days |
| **Throughput** | **burst (<7d) — 22 pts in 3.5d** |

**Estimate calibration:** 0 tasks re-estimated mid-phase. Net drift: 0 pts — every
task shipped at its planned points.

**Why:** A single-session burst; no per-week rate is quoted (a sub-week
denominator is noise). The headline is the calibration: six-for-six on estimates
means the point unit is honest going into Phase 2.

### Scope Changes

- None. All six planned tasks (1.1–1.6) shipped at estimate. Three deferred
  decisions retired: **D5 → PlatformIO**, **D2 → DEC-003** (packet v1 contract);
  **D6 deliberately held open** (provisional VictoriaMetrics, non-binding).
- Non-task PRs in the window: **#13** (doc recovery — see below) and **#16**
  (harness/shell migration); neither is Phase 1 task work.

Original estimate: 22 pts. Final: 22 pts (0 pts drift).

### What Worked

- The shared-golden-vectors contract paid off exactly as intended — C++ and Python
  were written independently and met with zero drift.
- Software-first + adapters let the whole pipeline (incl. a live drydown curve)
  run with no hardware.
- Architect review before building the packet contract (D2) caught the right
  design questions early.

### What Didn't Work

- PR #13 — doc additions missed the #12 merge (branch merged one commit early),
  needing a recovery PR.
- Two issues (#4/#5) didn't auto-close because early PR bodies lacked the
  "closes #N" keyword.
- Host `curl` was sandbox-blocked, so live VM validation had to route through
  Python.

### Changes For Next Phase

- Always put "Closes #N" in the PR body.
- Confirm the branch head is current before merging.
- Carry the contract-vectors discipline into Phase 2's sensor math.

### PM Read

**Pace.** 22 points across six tasks, all closed in one ~3.5-day session, zero
re-estimation, zero net drift. Every task shipped at its planned points — not luck
on a first phase, an honestly-sized plan. "Burst" is the right label, but the
useful signal is the calibration: six-for-six means the poker discipline is real,
and we carry a trustworthy estimator into Phase 2.

**Scope.** Double duty — built the spine *and* retired three deferred decisions
(D5→PlatformIO, D2→DEC-003, D6 held open). Resolving D2 through @architect *before*
writing the contract is the DEC-001 play, and it's why the two halves met with
zero drift. "Done when" met and validated live with a 432-reading run. Clean exit.

**Patterns.** The shared-golden-vectors contract did exactly its job; review caught
the one place prose lied (14 vs 12 bytes) before it poisoned anyone's model.
Friction was never in the code, always in process plumbing: #13's early-merge
recovery, #4/#5 missing "Closes #N". Papercuts, not debt — and the dev already
named the fixes.

**A genuine reaction.** Most reassuring isn't the 100% — it's that the dev refused
to vibe-code the contract and reviewed it by hand. On a project whose architecture
rests on two codebases never drifting, that instinct beats the velocity number. A
retro that surfaces its own plumbing mistakes on a flawless-looking phase is one I
trust. Push point: "confirm branch head before merging" gets *harder* under Phase
2's stacked-PR workflow — make it a standing checklist item.

**Forward into Phase 2.** The math gets opinionated (temp-compensated kPa, VPD, the
wake→sample→sleep cycle). Transfer the discipline: pin the sensor math to golden
input→output vectors so a calibration tweak is a vector diff, not a guess.
Run-cycle work is the first real test of `millis()` non-blocking timing beyond a
heartbeat stub. Estimates are trustworthy, the spine is live — Phase 2 is tuning
the brain, not authoring the skeleton. No timeline risk yet; critical path (Red
Tunnel ~March 2027) has runway, but I'll want a real pts/wk number once there's a
multi-week denominator.

---

## Phase [N] — [Name]

**Closed:** [date]
**Span:** [N] days · **Points:** [N] · **PRs:** [N]

### Throughput

| Metric | Value |
|--------|-------|
| Points closed | [N] |
| Span | [N] days |
| **Throughput** | **[N] pts/wk** (or "burst") |

**Estimate calibration:** [tasks re-estimated mid-phase: N. Net drift: ±N pts.]

**Why:** [One sentence on what drove the throughput — infra investment paying
off, heavier tasks than estimated, rework, availability, etc.]

### Scope Changes

- **[task ID]** — [description] ([N] pts) — [why added/cut]

Original estimate: [N] pts. Final: [N] pts ([±N] pts).

### What Worked

- [Process or tooling win to repeat]

### What Didn't Work

- [Friction point to change next phase]

### Forecast Update (as of [date])

**Critical path:** Red Tunnel by ~March 2027.
**Remaining work:** ~[N] pts across Phases [N]–[N].

At [N] pts/wk throughput → ~[N] weeks of work remaining. Combined with real
availability, that lands ~[date] — [on track / at risk / behind] vs. the
transplant.
