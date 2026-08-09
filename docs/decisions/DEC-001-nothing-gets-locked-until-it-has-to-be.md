---
id: DEC-001
title: "Nothing gets locked until it has to be"
topic: "Process & decision discipline"
---

## DEC-001: Nothing gets locked until it has to be

**Decision:** Defer every decision to the moment simulation or the bench forces a
real answer. Where a default keeps options open, take that default. The one
exception: a *hardware* choice that would change the *software* we write gets
made early — but only after confirming it can't sit behind an adapter and be
faked in simulation (most can). Deferred decisions are tracked in `SPEC.md` §12
so "deferred" never quietly becomes "forgotten."

**Why:** Soundings is read-only and low-stakes by design, and is being built
software-first against simulation precisely so that choices can be made against
real data instead of guesses. The dominant risk on a project like this isn't
getting a decision wrong — it's committing to infrastructure prematurely and
carrying that weight. Deferral is the strategy, not procrastination.

**Tradeoff:** Requires the discipline to actually maintain the §12 register and
revisit it at each phase, or deferrals rot into surprises.

**Revisit:** This is the governing principle. It isn't revisited — it's applied.

---
