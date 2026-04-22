# soundings — Project Plan

**Start date:** [YYYY-MM-DD]
**V1 target:** TBD
**Critical path:** [What must be true for V1 to ship]

---

## Estimation Method

Fibonacci scale (2, 3, 5, 8, 13). See `VELOCITY_AND_POKER_GUIDE.md` for definitions.
All estimates from planning poker between [your name] and Claude.
Disagreements logged in the Standing Disagreements table at the bottom.
Tests are baked into every task estimate — no separate testing tasks.

**Velocity baseline:** Not yet established. Will update after first 5 sessions.

---

## Phase 0: Infrastructure

Everything needed to develop safely. No user-facing changes. Do this phase first — no feature work until green.

| # | Task | Effort | Notes |
|---|------|--------|-------|
| 0.1 | Install Docker Desktop on WSL2, verify running | 2 | |
| 0.2 | Initialize local Supabase (`supabase init`, `supabase start`) | 2 | |
| 0.3 | Baseline migration — dump prod schema as `supabase/migrations/000_baseline.sql` | 3 | If greenfield, create initial schema migration instead |
| 0.4 | Seed data — `supabase/seed.sql` with Playwright test users (all roles) | 2 | |
| 0.5 | Verify: `supabase db reset` → app runs against local Supabase | 2 | |
| 0.6 | pgTAP setup — install extension, create `supabase/tests/` structure, verify pipeline | 3 | |
| 0.7 | pgTAP test suite — RLS tests for `profiles` table (all roles × CRUD) | 3 | |
| 0.8 | pgTAP test suite — RLS tests for [core tables] | 5 | Adjust effort based on table count |
| 0.9 | pgTAP test suite — RLS tests for [remaining tables] | 5 | |
| 0.10 | RLS audit — fix gaps found by pgTAP tests | 3 | |
| 0.11 | Install Playwright, configure viewports (375/768/1440) | 3 | |
| 0.12 | Playwright test suite — auth flows (login, register, role routing) | 3 | |
| 0.13 | Playwright test suite — [core flow 1] | 5 | Adjust per feature complexity |
| 0.14 | Playwright test suite — [core flow 2] | 5 | |
| 0.15 | Save @ui-reviewer agent spec to `.claude/agents/ui-reviewer.md` | 2 | Fill in project theme details |
| 0.16 | Verify session skills installed (`/its-alive`, `/pause-this`, etc.) | 1 | Should already be in `~/.claude/skills/` from domain-seeds |
| 0.17 | Fill in CLAUDE.md, SPEC.md, DECISIONS.md, AGENTS.md, BRAND.md | 3 | Done before first session if possible |

**Phase 0 total: [sum] pts**

**Ejection point:** Dev environment is professional-grade. Every future session is faster and safer. No user-facing value yet.

**Demo:** `supabase db reset` → `npm run dev` → `supabase test db` (all green) → `npx playwright test` (all green)

---

## Phase 1: [Name]

[Description of what this phase delivers and why it comes first]

| # | Task | Effort | Notes |
|---|------|--------|-------|
| 1.1 | [Task name] | [N] | |
| 1.2 | [Task name] | [N] | |

**Phase 1 total: [sum] pts**

**Ejection point:** [What's working / what can be demoed at end of this phase]

---

## Phase 2: [Name]

| # | Task | Effort | Notes |
|---|------|--------|-------|
| 2.1 | [Task name] | [N] | |

**Phase 2 total: [sum] pts**

---

## Velocity Table

Updated at end of each phase. Used by @pm to project remaining time.

| Phase | Actual Hours | Effort Points | Hrs/Pt | Notes |
|-------|-------------|---------------|--------|-------|
| 0 | — | — | — | |
| 1 | — | — | — | |

**Lifetime velocity:** — hrs/pt

---

## Estimation Poker — Standing Disagreements

Unresolved estimate disagreements. Revisit when the task starts.

| Task | Claude says | You say | Question |
|------|------------|---------|----------|
| [task] | [N] | [N] | [what's in dispute] |

---

## Phase Boundary Checklist

At the end of every phase:
1. All pgTAP tests green (`supabase test db`)
2. All Playwright tests green (`npx playwright test`)
3. @pm phase retrospective — velocity check, timeline update
4. Write retrospective entry in `docs/RETROSPECTIVES.md` (velocity, scope changes, process notes, forecast update)
5. Return to primary planning chat — review docs against intent

---

## Cuttable Tasks (if behind)

Tasks that can be deferred to V2 without breaking core functionality. Reference before any scope cut conversation.

| Task | Why it's cuttable | Defer to |
|------|------------------|---------|
| [task ID] | [reason] | V2 |
