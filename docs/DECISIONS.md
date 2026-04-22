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

## DEC-TBD: [Decision placeholder]
**Question:** [What needs to be decided]
**Options:** [Option A vs Option B]
**Consult @architect before building.**
