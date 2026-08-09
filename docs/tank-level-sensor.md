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
  shot), **dead centre** (see *Beam cone*).
- Pointing straight down at the surface.
- Mount in a **shallow, wide PVC collar** through the lid — not a deep narrow
  tube. It fights condensation dropout in the humid headspace *and* lifts the
  blind zone clear of the water (see *Dead zone*).
- Keep the beam path clear — no inlet pipes, fittings, or baffles in the cone.

**Standoff geometry (HW-03).** A 60° cone clears the tube wall only while

```
L_max  ≈  1.73 × r_inner
```

| Pipe | Inner radius | Max standoff length |
|------|-------------|---------------------|
| 3" Sch40 | 39 mm | 67 mm |
| **4" Sch40** | 51 mm | **89 mm** |
| **6" Sch40** | 77 mm | **133 mm** |

**Use 4" or 6", cut to 50–75 mm, with the sensor face flush to the top of the
collar.** "Short standoff tube" in earlier drafts is shorter and much wider than
the phrase implies — a deep narrow tube clips the cone and returns garbage that
looks plausible.

### Read strategy (HW-08)

The A02YYUW **free-runs**: apply power and it streams 4-byte frames (`0xFF`,
`DATA_H`, `DATA_L`, `SUM`) at 9600 8N1, no trigger. There is no published
power-on settling spec; response time is 100 ms with RX pulled low.

- Pull RX low (real-time mode), allow ~100 ms after power-up.
- **Discard the first 2–3 frames**, then collect 5–7 checksum-valid frames.
- Reject checksum failures, `0x0000`, and anything outside known tank geometry.
- **Take the median, not the mean** — the failure modes are outliers (dropped echo
  reads max range; multipath and sidewall echoes read short), and a mean drags
  toward them while a median ignores them.

Sensor rail-on is ~800 ms ≈ 6.4 mA·s. **Do not contort this to save sensor
milliseconds** — the ESP32 awake alongside it costs roughly 5× as much, so total
awake time is the term worth optimising.

### Dead zone — **3 cm, not 20–25 cm** (HW-09)

> **Corrected 2026-08-08.** This doc previously claimed a ~20–25 cm blind zone and
> accepted that the top of the tank clamps to "full." **That figure belongs to the
> JSN-SR04T, not the A02YYUW** — DFRobot's own marketing draws exactly that
> contrast. The A02YYUW's blind zone is **3 cm**, range 3–450 cm
> ([wiki.dfrobot.com/sen0311](https://wiki.dfrobot.com/sen0311/)).

The practical dead zone at the water surface is **effectively zero**, because the
standoff tube already exceeds it. With the transducer face 50–75 mm above the lid
plane (see *Mounting*), the 3 cm blind zone terminates **inside the tube**, still
20–45 mm above the lid. Water can never rise into it — tanks have an overflow well
below the lid regardless.

So the top-of-range clamp described in earlier drafts is gone. The load-bearing
range is still the bottom, where running the irrigation pump dry is the real risk.

### Beam cone — sidewall echoes at low level (F-3)

The A02YYUW's sensing angle is **60° full cone (30° half-angle)**, per DFRobot's
spec table. Some resellers claim 15°; that contradicts the manufacturer, and the
design assumes 60°.

The cone keeps expanding all the way down. For an 1100-gal vertical cylinder
(≈1.4–1.6 m diameter depending on its height), **the cone reaches the sidewall
about 1.24–1.41 m below the sensor** — well before it reaches the bottom of an
empty tank.

Consequences, in order of how much they matter:

- **Expect occasional short-reading outliers at low tank levels.** A specular
  return off a flat water surface directly below is normally the first and
  strongest echo, so this usually resolves correctly — but ripples, internal ribs,
  or a centre draw pipe are candidate false targets.
- **The median-of-5–7 read strategy is the defence** (see *Read strategy*), and it
  is a good one: these failures are outliers, not bias.
- **The gateway must band-check.** Reject anything outside 3 cm–tank height before
  it reaches the volume curve.
- **Mount dead centre**, and keep the cone clear of the fill inlet stream.

---

## Node

Standard Soundings node, with deviations from the mesh spec that the hardware
research rounds forced:

- **Heltec Wireless Stick Lite V3** — no OLED. Same ESP32-S3 + SX1262 and the same
  pinout as the WiFi LoRa 32 V3 (HW-15). ⚠ **Two U.FL sockets: E2 is LoRa, E3 is
  2.4 GHz. Label E2** — the wrong one transmits into an unmatched load, silently.
- 2× 18650 parallel, no solar. **Protected cells + a 3.2 V/cell firmware cutoff
  instead of the SPEC's LVC board** (DEC-006).
- Light-colored IP65 enclosure, mounted shaded
- Sensors wired through a PG7 gland to the internal Wago terminal block
- WiFi disabled, deep sleep, **15 min** cadence, ±30 s wake jitter
- **A DS18B20 in the tank headspace** — required for temperature compensation.
  Waterproof stainless probe, ~3 m lead; placement matters far more than accuracy
  grade (see below).
- **The sensor rail is switched.** Vext is the likely switch on this board — the
  ~1.44 V residual seen on the WiFi LoRa 32 V3 is an **OLED back-power artefact**,
  not a board property, and there is no OLED here. ⚠ Meter it before relying on
  it; a discrete P-FET is the fallback (`HARDWARE_BUILD_PLAN.md` §6).

Lives at the tank cluster as its own dedicated node.

**Channel usage.** The headspace DS18B20 rides on channel bit 4 (`SOIL_TEMP_0`) —
the encoding is exactly right (DS18B20 raw i16, 1/16 °C) and the channel name is
a wart, not a defect. The gateway's node→location map knows this node is the tank
node and labels it correctly downstream. This spends **zero** of the three
remaining reserved channel bits (`contracts/packet-v1.md:89`), which matters given
the 16-channel ceiling. A documentation-only rename to a sensor-neutral
`DS_TEMP_0` is a cheap future tidy-up; it would touch the vectors and both
parsers, so it is not worth doing on its own.

---

## Temperature compensation — the dominant error term (F-1, DEC-007)

**This is larger than every other error source combined, and it was missing from
every earlier draft of this doc.**

The speed of sound in air rises ~**0.176 %/°C** (`c = 331.3 + 0.606·T` m/s). An
uncompensated sensor converts echo time to distance using one fixed speed, so
every degree of headspace temperature drift shows up as apparent level change
with no water moving.

Over a 2 m headspace, referenced to 20 °C:

| Headspace air | Apparent level error |
|---|---|
| 0 °C | **−7.1 cm** |
| 10 °C | −3.5 cm |
| 20 °C | 0 |
| 30 °C | +3.5 cm |
| 40 °C | **+7.1 cm** |

**A 14 cm swing across the seasonal range** — on a sensor specified to ±1 cm, and
larger than the blind-zone correction that previously masked it. In a
sun-exposed tank headspace, 0–40 °C is not an extreme; it is the year.

### Therefore: a DS18B20 in the headspace is required, not optional

One DS18B20 on a 1-Wire bus, in the tank headspace, **shaded from the lid**
(a sun-warmed lid radiates and the air stratifies — measure the air, not the lid).

**Vendor claim conflict:** DFRobot's spec table lists **no** temperature
compensation; one reseller claims the sensor has it. DFRobot's own FAQ and ESP32
guide both tell you to implement it yourself. **Assume none.**

### The correction runs gateway-side, in Python (DEC-007)

The node puts **raw distance and raw DS18B20 counts** on the wire and derives
nothing — DEC-003 (`contracts/packet-v1.md:17`) and DEC-004. The gateway applies

```
d_corrected  =  d_raw  ×  c(T_headspace) / c(T_reference)
```

before the volume curve.

Doing it on-node would bake the correction into firmware sitting in a tank lid.
Doing it gateway-side means it can be refined later — humidity has a smaller but
real effect on the speed of sound — and **re-derived against years of stored raw
without touching the hardware.** Same argument that put the volume curve there.

### ⚠ Stratification — the residual is a signed bias, not noise (HW-19)

**Sharpened 2026-08-08.** An earlier draft called this "approximate," which
undersold it.

The correction integrates over the *whole* sound path, but the headspace is not
isothermal. A sun-loaded lid can sit **15–20 °C above** the air just over the
water, so a lid-mounted probe reads the top of the gradient and **over-corrects**.

Two things make that worse than a tolerance:

- **The bias is signed and seasonal.** A median across samples cannot remove it —
  every sample is wrong in the same direction at the same time of day.
- **It is worst exactly when it matters most.** The gradient is largest when the
  headspace is tallest — empty tank, longest path, biggest correction. That is
  the low-level regime the sensor exists to protect against pump-dry.

**Mitigations, cheapest first:**

1. **Thermally isolate the probe from the lid** — standoff, not bolted flat
   against sun-warmed plastic.
2. **Hang it partway into the headspace** on its own lead rather than flush at the
   top. (This is why the BOM specifies a ~3 m probe lead.)
3. **Fit the gradient empirically, later.** Because raw counts go on the wire and
   derivation is gateway-side, the model can be calibrated against manual dip
   readings and **re-derived over stored history without opening a tank.**

Build 1 + 2, log raw, calibrate in the first season. Mitigation 3 is what makes
gateway-side derivation load-bearing rather than merely tidy — this is the second
time that call has paid for itself.

**Probe accuracy barely matters, which is worth knowing before overspending.** At
a 2 m path, **1 °C of probe error is 3.5 mm** — against the 14 cm being corrected,
even a sloppy ±2 °C part captures most of the benefit. Placement dominates.

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
| `farm/soundings/water/cluster/level_gal` | calibrated total gallons | from the two-segment curve, on the **corrected** distance |
| `farm/soundings/water/cluster/percent` | percent of full (2530 gal) | |
| `farm/soundings/water/cluster/distance_mm` | **raw sensor distance** | **published always**, uncorrected |
| `farm/soundings/water/cluster/headspace_temp_c` | headspace air temperature | the compensation input — published so the correction is auditable |

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

A02YYUW **plus a headspace DS18B20** in a cylinder lid · one dedicated node ·
temperature-corrected distance · two-segment calibrated curve · raw distance,
raw headspace temp, gallons and percent published.

*Revised 2026-08-08 from Round-1 research (`CHAT_HANDOFF.md`): temperature
compensation added (F-1/DEC-007), blind zone corrected 20–25 cm → 3 cm (HW-09),
beam cone and standoff geometry specified (HW-03/F-3), read strategy specified
(HW-08), battery protection changed (DEC-006).*
