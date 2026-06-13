# Velocity Tracking & Scrum Poker — How-To Guide

A lightweight solo-dev process for estimating work and knowing where you stand
against a deadline. Built for Claude-Code-assisted development with fragmented,
bursty time.

---

## Part 1: Velocity Tracking — throughput (DEC-S026)

### What it measures

**Velocity = throughput: effort points closed per calendar week (pts/wk).**

This deliberately replaced the old hours-per-point model. We do **not** measure
at-keyboard time, read session transcripts, or do wall-clock math. Throughput is
a **capacity signal that already includes your availability** — a week with two
hours of farm-free evenings and a week of rain both just produce "points closed
that week." That's the number that actually forecasts a calendar date.

Because real work comes in bursts (a long Saturday, then nothing for ten days),
throughput is **burst-aware**: a short, intense phase is reported as a burst
rather than a misleadingly high weekly rate.

### Where the number comes from

`/retro` computes it at each phase boundary, entirely from **GitHub data**:

- Each task is a GitHub Issue with a `points:N` label (set by `/start-phase`).
- A task's points count when its Issue `closedAt` lands inside the phase window.
- `phase points ÷ phase span in weeks = throughput`.

No manual logging, no timers, no "log session" step. If the issues and their
`points:` labels are honest, the velocity is honest.

### Reading it

- **Throughput is your forecasting input.** `remaining points ÷ throughput ≈
  calendar weeks left`. Multiply by your real availability/seasonality to land a
  date.
- A phase that came in as a "burst" tells you little about sustainable pace —
  weight multi-phase trends over a single burst.
- If the forecast date slips past the **2027 transplant critical path**, that's
  the cue to cut scope (the plan keeps a cuttable-tasks list), not to pad
  estimates.

### Rules of thumb

- **Keep `points:` labels honest.** Re-label an Issue if it turns out bigger —
  that's the calibration signal, not a failure.
- Don't chase a single phase's number; throughput stabilizes over a few phases.
- The estimate-calibration tally (`/retro`) — how many tasks were re-estimated and
  the net drift — is as useful as the rate itself: it tells you whether your
  poker is systematically low.

---

## Part 2: Scrum Poker (Estimation)

### The scale

**Fibonacci: 2, 3, 5, 8, 13**

| Points | Meaning | Examples (this project) |
|--------|---------|-------------------------|
| **2** | Small, well-understood, minimal unknowns | Add a sensor field to the packet struct, wire an existing adapter into the run cycle, a single constexpr config table |
| **3** | Moderate, clear approach, maybe one unknown | A fake sensor driver behind an adapter, the VPD calc + its host tests, one Grafana panel from existing series |
| **5** | Significant work, some complexity or unknowns | Packet serializer + parser pinned by golden round-trip vectors, Watermark tension math with temp compensation, node→location mapping |
| **8** | Large, multiple moving parts, real unknowns | The full simulation spine (fake node → gateway → bus → DB → chart), the declared-manifest config + node-type presets, the gateway decode daemon with malformed-packet handling |
| **13** | Epic — break it up | "Build the gateway," "build the node firmware." If you see a 13, it's two or three things you haven't separated yet. |

**No 1s** — just do it. **No 13s if avoidable** — a 13 means the task isn't
understood well enough yet.

> **Splitting is a reviewability/risk call, not a model-capability one.** Don't
> chop a coherent 8 just to honor a ceiling — a well-specified 8 with crisp
> acceptance criteria and the `@architect` gate beats two stitched-together 5s.
> Do split when the diff is too big to review well, the blast radius worries you,
> or an "8" is secretly two unrelated things. Still break genuine 13s.

### How to poker (solo dev + Claude)

**Setup:** Claude proposes effort for each task in a phase. You review.

```
Claude: "Task 2.3 — Watermark tension math + temp compensation. I say 3."
You:    "That's a 5. The AC-excitation sampling has to cancel DC offset, and
         the temp-comp equation is touchy near saturation."
Claude: "Fair — what makes it a 5 vs a 3?"
You:    "Averaging both half-cycles, a valid-tension band, and golden vectors
         for the noisy wet end. It's not just plugging into the formula."
Claude: "Agreed — 5. I'll note the sampling + valid-band work in the Issue."
```

**Rules:**
1. **Disagree openly** — catching misestimates before the build is the point.
2. **Justify the gap.** If you say 8 and Claude says 3, someone's missing something.
3. **The person doing the work wins ties.**
4. **Record unresolved disagreements** and revisit when the task starts.
5. **Re-estimate when surprised** — re-label the Issue. That's data.

### Anti-patterns

- **Anchoring:** don't let the first number stick unchallenged.
- **Sandbagging:** don't pad for buffer — throughput already reflects real pace.
- **Precision theater:** 4-or-5 debates are wasted breath; pick the nearer Fibonacci.
- **Estimating mid-build:** if you're in it and it's bigger, finish it, then
  re-estimate the *next* similar task.

---

## Part 3: Putting It Together

### Rhythm

- **Phase start (`/start-phase`):** poker the phase's tasks, create the Issues
  with `points:` labels.
- **During:** work tasks, close Issues via PRs (`/kill-this`). No logging ritual.
- **Phase end (`/retro`):** throughput + calibration computed from the closed
  Issues; RETROSPECTIVES.md entry; forecast vs. the transplant deadline.

### Cross-project note

soundings and its sibling **tinkle** each have their own repo, plan, and
throughput — don't mix them. An embedded firmware phase and a Python-ingestion
phase have different point profiles even within one project, which is why
throughput is read per phase, not as one lifetime constant.

### The one thing that matters

**Keep the Issues and their `points:` labels honest.** Throughput, calibration,
and the deadline forecast are all downstream of that — and it's the only manual
input the system needs.
