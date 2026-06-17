# soundings — Phase Retrospectives

Written at each phase boundary by `/retro`. Velocity is **throughput (points per
calendar week)** computed from GitHub issue `closedAt` dates + `points:` labels
(DEC-S026) — not hours/point.

Format per entry: throughput, estimate calibration, scope changes, what worked,
what didn't, forecast update.

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
