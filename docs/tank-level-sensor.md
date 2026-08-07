# Tank Level Sensor

*Bay Branch Farm. Add-on to the Soundings V1 sensor mesh. Spec status: ✅ approved for V1.*

A single ultrasonic level sensor on one dedicated wireless node, measuring the
shared water level across the farm's three plumbed-together rain-catchment tanks.

---

## Overview

**Tank cluster:**
- 2× 1100-gal vertical cylinders (Norwesco-style)
- 1× 330-gal rectangular tank (IBC tote, ~46" tall)
- **Total capacity: 2530 gal**
- All three plumbed together at the bottom → communicating vessels, one shared
  water level (in height terms)
- All bottoms at the same elevation; all tops open

Because the tanks share a level, **one sensor covers all three**.

---

## Sensor

**A02YYUW ultrasonic distance sensor** (~$18).

- Non-contact — nothing touches the water, no fouling or contamination of
  irrigation water.
- Cleaner serial (UART) output than the cheaper JSN-SR04T; worth the few extra
  dollars for a mount-once-and-forget sensor.
- Measures distance to the water surface; water height = tank height − measured
  distance.
- Low, brief power draw — easily clears the node's 2-year battery target.

### Mounting

- In the lid of one of the **1100-gal cylinders** (tallest tank → best vertical
  shot, most dead-zone headroom).
- Pointing straight down at the surface.
- Recess the transducer face in a short **PVC standoff tube** through the lid to
  fight condensation dropouts in the humid headspace.
- Keep the beam path clear — no inlet pipes, fittings, or baffles in the cone.

### Dead zone

Ultrasonic sensors can't read the first ~20–25 cm below the transducer, so the
very top of the tank clamps to "full." Acceptable here — the priority is the
lower range, where running the irrigation pump dry is the real risk.

---

## Node

Standard Soundings node — no deviation from the mesh spec:

- Heltec WiFi LoRa 32 V3
- 2× 18650 parallel, no solar, low-voltage cutoff
- Light-colored IP65 enclosure, mounted shaded
- Sensor wired through a PG7 gland to the internal Wago terminal block
- WiFi disabled, deep sleep, ±30 s wake jitter

Lives at the tank cluster as its own dedicated node.

---

## Volume math — piecewise linear, one breakpoint

All bottoms level, all tops open, so gallons-per-inch steps as the level crosses
the IBC's top (~46"):

- **Below ~46":** all three tanks rising together — steeper gallons-per-inch.
- **Above ~46":** only the two cylinders still rising — shallower
  gallons-per-inch.

Total volume vs. height is therefore **two linear segments** with a single
breakpoint where the IBC tops out.

### Calibration — analytic seed, empirical correction (DEC-005)

> **Revised 2026-08-07 (DEC-005).** The original plan here was a purely empirical
> fit, whose virtue was needing no tank dimensions. Its cost was a **calendar
> dependency** — the tanks are rain-fed, so fill points on both sides of the IBC
> breakpoint could take months, and knowing the volume at each point needs a flow
> meter or a metered drawdown that may not exist. The revised plan removes that
> from the critical path.

1. **Seed analytically.** Measure the three tanks once with a tape measure — the
   two cylinder diameters, the IBC's footprint and height, and the height at
   which the IBC tops out. Compute the two-segment curve from geometry. Twenty
   minutes, and it yields a usable curve on day one.
2. **Mount the sensor and start publishing raw distance immediately** — even
   before the radio link works, tethered or read by hand. Every reading banked is
   a future calibration point.
3. **Correct empirically** as real fills happen, on whatever timeline the weather
   provides.

Because raw distance is always on the wire, any curve re-fits from stored raw
with no reflash and no re-calibration trip to the tank.

**Consequence:** calibration no longer gates "up and running." **M1 — first
light** (real distance on a chart, analytic curve) is independent of rainfall;
**M2 — calibrated gallons** trails it. See `docs/HARDWARE_BUILD_PLAN.md` §7.

**Still open:** whether an independent volume reference exists (a meter on the
fill line, a metered drawdown). No longer blocking, but it sets how good M2 gets.

---

## Integration

**MQTT topics** — under the `farm/soundings/…` namespace (DEC-004, namespace
conflict resolved by DEC-005; the exact hierarchy below the root is designed in
Phase 3.6):

| Topic | Payload | Notes |
|-------|---------|-------|
| `farm/soundings/water/cluster/level_gal` | calibrated total gallons | from the two-segment curve |
| `farm/soundings/water/cluster/percent` | percent of full (2530 gal) | |
| `farm/soundings/water/cluster/distance_mm` | **raw sensor distance** | **published always** |

**Publish raw distance** so the volume curve can be re-fit later in software
without re-calibrating or touching hardware.

Derivation runs **gateway-side, in Python** (DEC-004) — never in firmware, so the
curve is re-fittable without reflashing a node in a tank lid.

> Earlier drafts of this doc used a bare `farm/water/cluster/*` namespace. That
> predates DEC-004. Everything soundings publishes now lives under
> `farm/soundings/…`; if tinkle wants a `farm/water/…` view for its future pump
> lockout, that's a broker-side republish on the consumer's side of the boundary.

### Downstream: tinkle pump lockout

This is the sensor that protects the irrigation pump from running the cluster
dry. It feeds the **tinkle** irrigation controller's low-tank pump lockout — see
tinkle `DEC-017` for the consumer side. That integration is **V2 on the tinkle
side** (a future `TankMonitor` gate, not built yet); Soundings' V1 job is to
publish the level, and that closed-loop control logic lives in tinkle, not here.

---

## Spec status: ✅ approved for V1

A02YYUW in a cylinder lid · one dedicated node · two-segment calibrated curve ·
raw distance + gallons + percent published.
