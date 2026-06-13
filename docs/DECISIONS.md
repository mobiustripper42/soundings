# Soundings — Architectural Decisions

Decisions are numbered DEC-NNN. "DEC-TBD" means a decision is flagged but
unresolved — consult @architect before building.

> **Reset note (2026-06-13).** The original DEC-001–006 were retired. DEC-001–004
> were leftover `seeds/dev` webapp template (Supabase / Next.js / shadcn) and
> never described this project. DEC-005–006 documented the tank-level sensor as
> architecture; per the project owner the tank sensor is just another sensor, so
> it now lives in `SPEC.md` §5.4 (detail in `tank-level-sensor.md`), not as a
> decision. Real decisions restart at DEC-001 below.
>
> **Most architecture choices live as `[settled]` tags in `SPEC.md`, and every
> deferred choice lives in the `SPEC.md` §12 register.** This file is reserved
> for decisions whose *reasoning* is worth preserving on its own — the rule we
> operate by, and the cross-cutting software-architecture calls.

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

## DEC-002: One configurable firmware, declared sensor manifest

**Decision:** Every node runs the **same single firmware binary**. All sensor
drivers are compiled in; a node runs only the drivers listed in its **declared
per-node manifest**. Sensors are **declared, not auto-detected**. A node's
identity — ID, location, sensor set — is **configuration data, not code**. The
"node types" (bed / tunnel-air / tank / stratification rig) are config presets,
not separate firmware.

**Why:**
- **Identical, foolproof flashing.** The annual battery-swap service window
  becomes "plug in, flash, done" — no per-node binary bookkeeping, no flashing
  the wrong build.
- **Declared beats auto-detect for data integrity.** A node that *knows* it
  should have a sensor turns a missing reading into an actionable fault ("sensor
  expected, missing — go check it"), not a silent absence. It also handles the
  Watermark, whose AC-excitation analog circuit isn't reliably auto-detectable.
- **Identity-as-data.** Moving or adding a sensor is a manifest edit, not a
  recompile — the same principle as "raw reading is the durable record."
- **Negligible cost.** Carrying dormant drivers is nothing on the ESP32-S3.
- **Keeps a hardware question deferred.** The firmware is agnostic to how many
  sensors hang off one node, so the "one big node per tunnel vs. several small
  nodes" granularity call (DEC-001) stays deferrable.

**Tradeoff:** A node carries code it never runs. The manifest format and
provisioning flow need designing — the goal is a **dead-simple declaration
setup**, tracked as firmware-skeleton work. The gateway must know which fields a
given node emits, which ties to the packet schema (§12 D2) and the node→location
map (§12 D7).

**Revisit:** If a node ever can't carry all drivers (not foreseeable on the S3),
or if a compelling auto-detect *provisioning convenience* emerges — layered over
the declared manifest as source of truth, never replacing it.

---

*Settled choices recorded as `[settled]` in SPEC (read-only V1, LoRa
point-to-point not LoRaWAN, no solar, USB-flash not OTA, software-first build)
may graduate to their own DEC entries here if their reasoning needs preserving.
For now they live in SPEC §2–§3.*
