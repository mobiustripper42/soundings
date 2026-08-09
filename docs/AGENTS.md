# soundings — Claude Code Agents & Skills

Agents and session skills support the development workflow. They run as Claude
Code sessions, subagents, or slash commands. None are blocking — if one creates
friction, drop it and revisit.

This is a `tool`-type project, so `@ui-reviewer` (webapp-only) is not used.

---

## Agents

| Agent | Model | When | Purpose |
|-------|-------|------|---------|
| `@architect` | Opus 4.8 | Before design decisions, new dependencies, scope creep, any unresolved decision | Keep architecture coherent against SPEC + DECISIONS. Runs Opus; escalate to a Fable run for genuinely hard or bundled design work (DEC-S027). Output: proceed/modify/reject + reasoning, draft DEC entry. |
| `@code-review` | Sonnet | After commits (wired into `/kill-this`) | Lightweight post-commit review — bugs, inconsistencies, convention drift. Output: findings ranked by severity, or clean bill. |
| `@pm` | Sonnet | Session start/end | Tracks state — done, next, blocked. Flags timeline risk, recommends order, suggests scope cuts. |
| `@tape-reader` | Sonnet | Via `/read-the-tape` | Reads a session transcript and writes one cited observation to seeds' `observations` branch. Modifies nothing in this repo (DEC-S040). |
| `@doc-consistency` | Sonnet | Via `/doc-consistency-check`, ad-hoc | Cross-references factual claims across the doc set; flags mismatches and unfilled placeholders. Report-only. |
| `@ideas` | Sonnet | Park an idea, re-rank, audit the parking lot | Curates docs/FUTURE_IDEAS.md — capture, dedupe, cross-ref. Edits only that file, and creates it on first use, which is why that path is unbackticked: it does not exist yet and the checker reads markup as a claim that it does. |

**There is no sync agent.** The sync-config agent and the push-seeds and pull-seeds skills were retired (DEC-S040) — named without slashes here because either would read as a claim they still exist. Moving a file between seeds and this repo is a manual `cp`; check seeds' `.claude/routine-config.yaml` § `file-classes` first.

Agent specs live in `.claude/agents/`.

---

## Session Skills

| Skill | When | What it does |
|-------|------|--------------|
| `/its-alive` | Session start | Ensures `.sessions-worktree/`, opens a per-session file on the orphan `sessions` branch, captures the active JSONL transcript path, reads last session + the plan, recommends a task, waits for confirmation. |
| `/pause-this` | Mid-session break | Build check, commits WIP on the task branch, notes the pause in the session file. |
| `/restart-this` | Resume from pause | Reloads context from the open session file — no new session. |
| `/kill-this` | Per task (DEC-S013) | Build check, commits code on the task branch, runs `@code-review`, opens a PR (`closes #N`), appends a `## Task <N>` block to the session file. May run multiple times per window. |
| `/its-dead` | Session end (once per window) | Stamps `ended:`, tallies points from the per-task blocks, displays wall-clock, commits + pushes the sessions branch. No time math or version bump — those moved to `/retro`. |
| `/start-phase` | Phase boundary (start) | Reads the next phase from PROJECT_PLAN.md, creates one Issue per task with `phase:N` + `points:X` labels, writes issue numbers back. |
| `/retro` | Phase boundary (end) | Computes phase throughput (points per calendar week from issue `closedAt` + `points:` labels, DEC-S026) + an estimate-calibration tally. Marks tasks `[x]`, writes RETROSPECTIVES.md, runs version bumps. Optionally chains into `/start-phase`. |
| `/bump-major` | Breaking change | Manual major bump — CHANGELOG entry + tag on `main`. |
| `/promote-production` | Ship trunk to prod | ff-merges `main` → `production`. Requires `origin/production`; not set up here yet. |
| `/read-the-tape` | After a notable session | Audits the JSONL transcript for anti-patterns via `@tape-reader`. |
| `/doc-consistency-check` | Before phase boundaries | Cross-references doc claims via `@doc-consistency` (report-only). |

**Dev identity:** skills resolve `DEV` from `~/.claude/devname` (with `$USER`
fallback). Used in session filenames.

---

## Task Model (post phase-rituals)

- `PROJECT_PLAN.md` is **read at planning, written at retro** — untouched
  mid-phase.
- The **current phase's tasks live as GitHub Issues** (created by `/start-phase`,
  closed by PRs).
- Phase boundaries are work-defined, not time-boxed: a phase ends when its issues
  close.

---

## Session Workflow

**Start:** `/its-alive` → briefing + task recommendation → confirm.
**During:** Spec → Build → Test (native + contract round-trip; sim where
relevant). Hit an architectural question → `@architect`. Long session →
`/pause-this` … `/restart-this`.
**Per task:** `/kill-this` → commit, review, PR.
**End of window:** `/its-dead` once.
**Phase boundary:** `/retro` (→ optionally `/start-phase`).
