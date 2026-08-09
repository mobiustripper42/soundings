# Soundings — Architectural Decisions

Decisions are numbered DEC-NNN, one per file, indexed by topic in the generated
`docs/DECISIONS.md` (DEC-S036). An unresolved choice gets a real id under the
**Open questions** topic rather than a placeholder id — ids are unique under v5,
so several placeholders would collide. Consult @architect before building on one.

> **Reset note (2026-06-13).** The original DEC-001–006 were retired. DEC-001–004
> were leftover `seeds/dev` webapp template (Supabase / Next.js / shadcn) and
> never described this project. The two after them documented the tank-level sensor as
> architecture; per the project owner the tank sensor is just another sensor, so
> it now lives in `SPEC.md` §5.4 (detail in `tank-level-sensor.md`), not as a
> decision. Real decisions restart at DEC-001 below.
>
> **Most architecture choices live as `[settled]` tags in `SPEC.md`, and every
> deferred choice lives in the `SPEC.md` §12 register.** This file is reserved
> for decisions whose *reasoning* is worth preserving on its own — the rule we
> operate by, and the cross-cutting software-architecture calls.

---
