# soundings — Product Specification

## Overview
[One paragraph: what it is, who uses it, what it replaces or enables.]

## Philosophy
[The guiding principle. How should users feel? What does the app stand for? What should it NOT be?]

## Target Launch
- **V1 target:** TBD
- **V1 critical path:** [What must be true for V1 to ship — e.g., "payments live"]

## Stack
- **Frontend:** Next.js 14+ (App Router), Tailwind CSS, shadcn/ui, [Font]
- **Backend:** Supabase (PostgreSQL + Auth + Row Level Security) — no separate API server
- **Payments:** [Stripe (Checkout Sessions, webhooks) / none]
- **Notifications:** [Twilio (SMS), Resend (email) / none]
- **Hosting:** Vercel (frontend), Supabase Cloud (database)
- **Dev Environment:** Local Supabase via Docker[, Stripe test mode]
- **Testing:** pgTAP (RLS), Playwright (integration), axe-core (accessibility)

## Roles
- **[Role 1]** — [what they manage or do]
- **[Role 2]** — [what they do]
- **[Role 3]** — [what they do]

## Core Concepts
- **[Concept 1]** — [definition and why it exists as a distinct thing]
- **[Concept 2]** — [definition]

## V1 Scope

### Phase 0 — Infrastructure
Local Supabase, pgTAP RLS tests, Playwright integration tests, session skills, updated docs.

### Phase 1 — [Name]
[High-level description of this phase's goal and the features it includes]

### Phase 2 — [Name]
[etc.]

### Sensors (V1)
The mesh's V1 sensor set. (The rest of this spec is still `seeds/dev` template
pending a docs pass; this section is filled because the sensor is approved.)

- **Tank level (cluster)** — ✅ approved for V1. One A02YYUW ultrasonic sensor on a
  dedicated node, measuring the shared level across the three plumbed-together
  catchment tanks (2530 gal). Publishes raw distance + calibrated gallons + percent.
  Full spec: `docs/tank-level-sensor.md`; architecture: DEC-005 / DEC-006.

## Not V1
[List things explicitly out of scope for V1. This is the scope guardrail — Claude will check here before adding anything.]

- **Closed-loop pump control / low-tank lockout** — Soundings publishes the tank
  level; acting on it (locking out the irrigation pump before it runs dry) lives in
  the **tinkle** controller (tinkle DEC-017), not here.
- [Feature X] — defer to V2
- [Feature Y] — defer to V2
