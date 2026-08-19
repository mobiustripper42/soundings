---
id: DEC-005
title: "The tank node goes first — a hardware-first vertical slice"
topic: "Process & decision discipline"
---

## DEC-005: The tank node goes first — a hardware-first vertical slice

**See also DEC-008**, which designs the topic hierarchy below the `farm/soundings/…` root that this decision constrained but left open. The namespace choice below stands unchanged.

**Decision:** The tank-level node is promoted ahead of the soil/VPD sensor work and built as a complete vertical slice — hardware plan → parts → real radio → real sensor → real packets → gallons on a chart — as a new **Phase 3** (`docs/PROJECT_PLAN.md`, detail in `docs/HARDWARE_BUILD_PLAN.md`). **Phase 2 is parked, not cancelled**; former Phases 3–6 renumber to 4–7. Three sub-decisions ride along:

- **Topic namespace: `farm/soundings/…`.** Resolves a conflict between `docs/tank-level-sensor.md:93-99` (`farm/water/cluster/*`, written before DEC-004) and DEC-004 (`farm/soundings/…`). Everything soundings publishes lives under its own namespace. If tinkle wants a `farm/water/…` view for its future pump lockout, that is a broker-side republish on the consumer's side of the boundary — not soundings' concern. Constrains D9; the full hierarchy is still designed in 3.6.
- **Tank derivation runs gateway-side, in Python.** Issue [#22](https://github.com/mobiustripper42/soundings/issues/22) put the two-segment curve in firmware; it was written 2026-06-17, three weeks before DEC-004 moved all derivation to the gateway. #22 is stale and gets rewritten as a Python task (3.6).
- **Calibration seeds analytically, corrects empirically.** `docs/tank-level-sensor.md:79-87` specifies a purely empirical fit, whose stated virtue is needing no tank dimensions. Its unstated cost is a calendar dependency: the tanks are rain-fed, so fill points on both sides of the IBC breakpoint could take months, and it needs an independent volume reference that may not exist. Instead: measure the three tanks once with a tape measure, compute the two-segment curve from geometry, and correct it from real fills as they happen. Raw distance is published either way (`:100-102`), so any curve re-fits from stored raw with no reflash.

**Why:**
- **It is the only node type with no analog front end.** No AC excitation circuit (D11 — the hardest hardware in the project), no Watermark calibration against a commercial reader, no temperature compensation. One sensor, one UART, one 4-byte frame. Roughly 21 points of the hardest near-path work defers with it.
- **It isolates the unknowns that are common to every node.** Link budget, deep-sleep current, enclosure survival, and the battery model are shared by all node types and none of them are validated. Meeting them behind a sensor that *cannot be the culprit* is the cheapest possible first contact with hardware. When the link doesn't close, the sensor is not a suspect.
- **The re-sequence cost nothing.** Zero Phase 2 issues were started; #19–#25 keep their `phase:2` labels untouched. Former Phases 3–6 had no materialized issues, so renumbering moved no labels.
- **It validates the numbers the whole fleet is planned against.** `SPEC.md` §4's 20–30 µA sleep and ~2-year battery life are estimates carried from the source spec and never measured. A tank node — one cheap sensor on a switched rail — sits near the floor of what any node draws, so measuring it calibrates the fleet battery model that open issue [#36](https://github.com/mobiustripper42/soundings/issues/36) asks for.
- **Phase 2 resumes onto a proven spine.** Phase 3 builds the adapter seam, run cycle, and a minimal manifest against one simple sensor. The parked sensor-math tasks land later on firmware that has by then survived real hardware, rather than on fakes.

**Tradeoff:** The anchor measurement — soil-moisture tension, the thing that can't be gotten any other way (`SPEC.md` §1) — is deliberately delayed in favor of a secondary one. Accepted for two reasons: the critical path is the **2027 tomato transplant (~March 2027)**, which the delay does not threaten, and the tank node retires the hardware risk that would otherwise surface for the first time *during* the anchor sensor's bench bring-up, when the excitation circuit would be an equally plausible suspect. A second, real cost: Phase 3 is ~47 points, roughly twice Phase 1, because it carries a full hardware bring-up the old plan had spread thin across two coarse phases.

**Revisit:** If the range test (3.2) shows the link can't close from the tank cluster, the gateway architecture branches (`HARDWARE_BUILD_PLAN.md` §3) but the sequencing still holds — that test has to happen for any node. If tank hardware stalls on procurement or weather, Phase 2 is parked, not dead: its issues are intact and can resume in place.

---
