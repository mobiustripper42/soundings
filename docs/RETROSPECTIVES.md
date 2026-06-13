# soundings — Phase Retrospectives

Written at each phase boundary by `/retro`. Velocity is **throughput (points per
calendar week)** computed from GitHub issue `closedAt` dates + `points:` labels
(DEC-S026) — not hours/point.

Format per entry: throughput, estimate calibration, scope changes, what worked,
what didn't, forecast update.

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
