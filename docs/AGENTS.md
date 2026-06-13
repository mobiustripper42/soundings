# soundings — Claude Code Agents & Skills

Agents and session skills support the development workflow. They run as Claude
Code sessions, subagents, or slash commands. None are blocking — if one creates
friction, drop it and revisit.

This is a `tool`-type project, so `@ui-reviewer` (webapp-only) is not used.

---

## Agents

| Agent | Model | When | Purpose |
|-------|-------|------|---------|
| `@architect` | Fable 5 | Before design decisions, new dependencies, scope creep, any DEC-TBD | Keep architecture coherent against SPEC + DECISIONS. Output: proceed/modify/reject + reasoning, draft DEC entry. |
| `@code-review` | Sonnet | After commits (wired into `/kill-this`) | Lightweight post-commit review — bugs, inconsistencies, convention drift. Output: findings ranked by severity, or clean bill. |
| `@pm` | Sonnet | Session start/end | Tracks state — done, next, blocked. Flags timeline risk, recommends order, suggests scope cuts. |
| `@sync-config` | Sonnet | `/push-seeds`, `/pull-seeds` | Classifies template-vs-project diffs, proposes backports, schema-version gated. |

`@tape-reader` and `@doc-consistency` (Sonnet) can be copied from seeds if/when
`/read-the-tape` or `/doc-consistency-check` is used.

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
| `/push-seeds` · `/pull-seeds` | Workflow sync | Backport to / pull from the seeds templates via `@sync-config`, gated on `.claude/seeds-version`. |
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
