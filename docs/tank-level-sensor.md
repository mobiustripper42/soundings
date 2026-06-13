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

### Calibration — empirical, no tank dimensions needed

1. Mount the sensor.
2. Log sensor reading vs. known volume at several fill levels — get a couple of
   points **below** the IBC top and a couple **above**.
3. Fit the two-segment line in the decoder. The breakpoint falls out of the data.

This calibrates out all geometry and offsets at once. No measuring of tank
heights or fitting elevations required.

---

## Integration

**MQTT topics** (`farm/water/cluster/*` namespace):

| Topic | Payload | Notes |
|-------|---------|-------|
| `farm/water/cluster/level_gal` | calibrated total gallons | from the two-segment curve |
| `farm/water/cluster/percent` | percent of full (2530 gal) | |
| `farm/water/cluster/distance_mm` | **raw sensor distance** | **published always** |

**Publish raw distance** so the volume curve can be re-fit later in software
without re-calibrating or touching hardware.

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
