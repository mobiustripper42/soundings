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

## Index

### Process & decision discipline
- DEC-001 — Nothing gets locked until it has to be
- DEC-005 — The tank node goes first — a hardware-first vertical slice

### Hardware — board, power & enclosure
- DEC-006 — Protected cells + a firmware cutoff, not a hardware LVC board

### Sensors & calibration
- DEC-007 — Ultrasonic temperature compensation, derived gateway-side

### Firmware architecture
- DEC-002 — One configurable firmware, declared sensor manifest
- DEC-012 — OTA is a pull, and the manifest is the trigger

### Radio, wire contract & gateway
- DEC-003 — Packet v1 wire contract (resolves §12 D2)
- DEC-008 — MQTT topic hierarchy, and a file-backed node→location map
- DEC-009 — LoRa stands, and the gateway radio lives on the server
- DEC-010 — Modem parameters, and the downlink that carries them back
- DEC-011 — The gateway listens continuously, and downlinks describe state rather than command it

### Server, storage & display
- DEC-004 — Storage and graphing belong to Poop Deck, not soundings (resolves D1 + D6)

_**This file is GENERATED** by `npm run gen:decisions` —
edit `docs/decisions/DEC-*.md`, not this file. `npm run check:decisions` fails on a stale index, a
duplicate id, an unknown topic, an unlanded SPEC amendment, or a reference to a decision
that does not exist._
