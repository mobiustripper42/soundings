# soundings — Architectural Decisions

Decisions are numbered DEC-NNN. "DEC-TBD" means the decision is flagged but unresolved — consult @architect before building.

---

## DEC-001: Supabase-direct instead of Express API
**Decision:** No separate backend server. Next.js talks directly to Supabase via server actions and server components.
**Why:** The Express layer is ~40% of the build and is entirely CRUD plumbing. Supabase Auth + Row Level Security replaces JWT middleware and role checks. Eliminates backend hosting costs. For single-tenant CRUD apps, the API layer adds cost with no benefit.
**Tradeoff:** Business logic lives in Postgres RLS policies and database functions. Migration away from Supabase is more involved later.
**Note:** If the app has a payment provider (Stripe), one API route is needed for webhooks. A single webhook endpoint does not justify a backend server.
**Revisit if:** Business logic becomes complex enough to warrant a dedicated API layer, or if the number of webhook endpoints exceeds 3–4.

## DEC-002: Next.js 14+ App Router
**Decision:** Next.js with App Router over bare React or Pages Router.
**Why:** File-based routing, Vercel zero-config deployment, SSR available if needed. React's own docs recommend a framework.
**Tradeoff:** App Router is newer — some patterns are less settled than Pages Router.

## DEC-003: Single profiles table (all roles)
**Decision:** One `profiles` table for all user types, extending Supabase Auth. Role flags as boolean columns.
**Why:** All roles share the same auth flow. Boolean flags (is_admin, is_[role2], etc.) support multi-role users. Simplifies queries and RLS.
**Tradeoff:** Some role-specific nullable fields on users that don't need them.

## DEC-004: shadcn/ui component library
**Decision:** shadcn/ui on top of Tailwind CSS.
**Why:** Components are copied into the repo (not a dependency) — fully customizable. Covers forms, tables, dialogs. Claude Code knows it well.

---

> **Note:** DEC-001 through DEC-004 above are leftover `seeds/dev` webapp template
> (Supabase/Next.js/shadcn) and do not describe this LoRa sensor-mesh project.
> They're left untouched here pending a proper docs pass; the real architecture
> record starts at DEC-005.

## DEC-005: A02YYUW ultrasonic level sensor — one node covers the cluster
**Decision:** Measure the tank cluster's water level with a single **A02YYUW
ultrasonic distance sensor** on one dedicated Soundings node, mounted in the lid of
a 1100-gal cylinder (the tallest tank). One sensor serves all three tanks.
**Why:** The three tanks (2× 1100-gal cylinders + 1× 330-gal IBC) are plumbed
together at the bottom — communicating vessels share one water level in height
terms, so a single height reading covers the whole 2530-gal cluster. Non-contact
ultrasonic keeps anything out of the irrigation water (no fouling). The A02YYUW's
clean UART output beats the cheaper JSN-SR04T's noisier interface — worth a few
dollars on a mount-once-and-forget sensor. The tallest tank gives the best vertical
shot and the most dead-zone headroom.
**Tradeoff:** A ~20–25 cm transducer dead zone clamps the very top of the tank to
"full." Accepted — the load-bearing range is the *bottom*, where running the
irrigation pump dry is the real risk. A condensation-dropout mitigation (recess the
transducer in a short PVC standoff through the lid) is required, not optional.
**See:** `docs/tank-level-sensor.md`.

## DEC-006: Two-segment empirical volume curve; publish raw distance always
**Decision:** Convert measured distance → gallons with a **two-segment
piecewise-linear curve** fit **empirically** (log sensor reading vs. known volume
at several fills — a couple below the IBC's ~46" top, a couple above — and fit the
two lines; the breakpoint falls out of the data). The decoder **always publishes
the raw `distance_mm`** alongside the derived gallons and percent. MQTT topic
namespace is `farm/water/cluster/*`: `level_gal`, `percent` (of 2530 gal), and
`distance_mm`.
**Why:** Gallons-per-inch steps where the IBC tops out — below ~46" all three tanks
rise together (steeper), above it only the two cylinders rise (shallower) — so the
volume curve is naturally two linear segments with one breakpoint. Fitting it
empirically calibrates out all tank geometry, fitting elevations, and sensor offset
at once, with no tank measurements. Publishing raw distance means the curve can be
re-fit in software later (more calibration points, a changed cluster) **without
re-calibrating hardware or touching the node** — the raw measurement is the durable
record; the gallons curve is a derived convenience.
**Tradeoff:** Two segments + a fitted breakpoint is marginally more decoder logic
than a single linear map, and the early curve is only as good as its first few
calibration points — which is exactly why raw distance is always on the wire.
**See:** `docs/tank-level-sensor.md`.
