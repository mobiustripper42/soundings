# Tank Node — Hardware Build Plan

*Bay Branch Farm. The build plan for the **first physical Soundings hardware**: one
tank-level node plus the gateway radio it talks to. Phase 3 (`PROJECT_PLAN.md`).*

> **Status tags** follow `SPEC.md`, plus one new tag for this doc:
>
> | Tag | Meaning |
> |-----|---------|
> | **[settled]** | Decided. Build on it. |
> | **[proposed]** | Current plan, not yet validated. |
> | **[verify]** | **Needs a real answer from a datasheet, vendor, or measurement before we buy or build.** Routed to `CHAT_HANDOFF.md` as an `HW-nn` question. |
>
> Nothing in the BOM gets ordered while it still carries a **[verify]** that would
> change the part. See "Open questions" below for the live list.

---

## 1. Why the tank node is the first hardware

It is the only node type that needs no analog front end. No AC excitation circuit
(§12 D11), no Watermark calibration against a commercial reader, no temperature
compensation. One sensor, one UART, one 4-byte frame.

That makes it the right *first* build for a reason beyond convenience: **when the
radio link doesn't close, the sensor is not a suspect.** Every hard unknown left
in the build — link budget, deep-sleep current, enclosure survival — is an
unknown shared by every future node type. The tank node isolates them.

**Scope of this plan:** exactly two boards and one sensor. One tank node, one
gateway radio. Not a fleet.

---

## 2. The first move: a range test, before anything else

**Do this before ordering an enclosure, cutting a tank lid, or writing node
firmware.**

Buy two Heltec WiFi LoRa 32 V3 boards and two antennas. Flash a trivial
ping/RSSI sketch on both. Walk one to the tank cluster, leave the other at the
intended gateway location, and record RSSI and packet loss.

That single afternoon resolves **both remaining hardware deferrals**:

- **D3 (node↔gateway PHY pairing)** — dissolved outright if both ends are
  SX1262. See §3.
- **D4 (gateway box)** — the test tells you whether the radio can live at the
  server or has to move.

Everything downstream branches on the answer, and the two boards are needed
regardless of which branch wins. There is no version of this project where
buying two Heltec V3s is wasted.

**Record:** RSSI and loss rate at the tank, at the intended server location, and
at the two tunnels — plus the same numbers with the tank lid closed and the node
inside its (eventual) enclosure, which both cost signal.

---

## 3. Gateway architecture — recommendation

### Recommended: second Heltec V3 as a dumb serial radio **[proposed]**

The gateway radio is a second Heltec V3, tethered by USB to the existing
headless Linux box (`SPEC.md` §7). Its firmware does one thing: receive a LoRa
frame, write the raw bytes over USB serial. It does not decode, does not run
WiFi, does not speak MQTT.

The Python decoder daemon already built in Phase 1 stays exactly where it is on
the server, and gains one new `IPacketSource` implementation — a
`SerialPacketSource` alongside the existing `FakePacketSource`
(`gateway/soundings_gateway/source.py:23`). The seam is already there.

**Why this over the SPEC's Pi Zero 2 W + RFM95W (~$60, `SPEC.md:281`):**

| | Heltec-as-serial-radio | Pi Zero 2 W + RFM95W |
|---|---|---|
| D3 (PHY pairing) | **Dissolved** — SX1262 both ends, identical chip | Live risk: SX1262↔SX127x cross-chip modem settings must be matched |
| D4 (gateway box) | **Dissolved** — no new box, server already exists | A new always-on box to provision and maintain |
| Wiring | A USB cable | SPI breakout, jumpers, a hat or solder |
| Spares | Boards are interchangeable with node stock | A second distinct part to stock |
| Firmware surface | ~1 file, no network stack | Pi OS to keep patched |

Two identical boards also means one flashing toolchain, one antenna type, and a
board that can be swapped between node and gateway roles while debugging.

### Fallback: Heltec V3 as a WiFi→MQTT bridge **[proposed]**

**Trigger:** the range test shows the link won't close from the tank to the
server's physical location.

The gateway radio moves to wherever the link *does* close (with mains power and
WiFi coverage) and publishes **raw, undecoded frames** to MQTT. The Python
daemon subscribes to raw frames instead of reading serial. Decoding still
happens in exactly one place — the Python parser — so the contract discipline
(`contracts/packet-v1.md`) is untouched.

Cost: WiFi + MQTT client + reconnect logic on the ESP32, which is real firmware
with real failure modes. Take it only if the range test forces it.

**Do not** put the decoder on the ESP32 under either option. A second
implementation of the parser is a second thing that can drift from the contract.

---

## 4. Bill of materials

### Node — at the tank

Per `SPEC.md` §4, minus the perfboard (no excitation circuit on a tank node).

| Item | Qty | Status | Notes |
|---|---|---|---|
| Heltec WiFi LoRa 32 V3 | 2 | [settled] | One node, one gateway radio. Buy both now for the range test. |
| A02YYUW ultrasonic sensor | 1 | [settled] | UART. `docs/tank-level-sensor.md` |
| 18650 cells | 2 | [settled] | Parallel, ~6000 mAh combined |
| 18650 holder, 2-cell parallel | 1 | [verify] | **HW-05** — connector must match the Heltec battery input |
| Low-voltage-cutoff board | 1 | [verify] | **HW-06** — needed only if the Heltec's own protection is inadequate |
| IP65 enclosure, light-colored, ~4×4×2" | 1 | [proposed] | Hinged lid preferred. Mounted **low and shaded** (`SPEC.md` §4 heat spec) |
| PG7 cable glands | 2 | [proposed] | Sensor cable + spare. Pointing **down**, siliconed both sides |
| SMA bulkhead + 3 dBi whip antenna | 2 | [verify] | **HW-01** — depends on the V3's onboard connector type |
| Wago lever-nut terminal block | 1 | [settled] | Serviceable internal connections |
| Silica gel desiccant | 1 | [settled] | Replaced annually |
| Sensor cable extension | 1 | [verify] | **HW-04** — stock cable almost certainly won't reach. See §5. |

### Mount — at the tank lid

| Item | Qty | Status | Notes |
|---|---|---|---|
| PVC pipe for the standoff tube | 1 | [verify] | **HW-03** — ID and length are a beam-cone calculation, see §5 |
| Hole saw | 1 | [verify] | Sized to the standoff OD, once HW-03 lands |
| Silicone sealant, UV-stable | 1 | [proposed] | Lid penetration + gland seals |
| Enclosure mounting hardware | — | [proposed] | Depends on what the node straps to at the tank cluster |

### Gateway — at the server

| Item | Qty | Status | Notes |
|---|---|---|---|
| Heltec WiFi LoRa 32 V3 | — | [settled] | The second board above |
| USB-C cable, long | 1 | [proposed] | Length depends on where the radio ends up relative to the box |
| Antenna | — | [verify] | Second antenna from the node row (**HW-01**) |

### Bench and validation tools

| Item | Status | Why |
|---|---|---|
| µA-capable current measurement | [verify] | **HW-07** — **you cannot validate the 2-year battery claim without this.** See §6. |
| Multimeter | [proposed] | Assumed on hand |
| Tape measure | [settled] | Calibration, §7 |

---

## 5. Mounting design

### The standoff tube is doing two jobs

`docs/tank-level-sensor.md:41` specifies recessing the transducer in a short PVC
standoff through the lid, to fight condensation dropout in the humid headspace.
It has a second effect worth making explicit:

**A recessed transducer sits higher, so every reading is longer by the standoff
length.** That pushes the tank's full line *further* from the sensor's blind
zone — the standoff buys back dead-zone headroom at the top of the tank, which
is the exact thing `docs/tank-level-sensor.md:45-49` accepts as a loss.

### The constraint that limits it

You cannot simply make the tube long. The ultrasonic beam leaves the transducer
as a cone; if the tube is too narrow or too long, the cone strikes the tube wall
and returns garbage instead of the water surface.

```
tube ID  ≥  2 × (standoff length + lid thickness) × tan(beam half-angle)
```

plus clearance. **This needs the A02YYUW's beam angle** — routed as **HW-03**.
Until that number lands, neither the pipe size nor the hole-saw size is known,
which is why both are [verify] in the BOM.

### The cable run is a real constraint

`SPEC.md` §4 requires the enclosure mounted **low and shaded** (the 18650s
degrade above ~45 °C). The sensor is in the lid of the *tallest* tank. Those
two requirements pull in opposite directions, and the sensor cable spans the
difference — roughly full tank height plus slack and a service loop.

The A02YYUW's stock cable will very likely be short. Whether it can be extended
(and how far, at 9600 baud) is **HW-04**. Answer it before ordering, or plan on
splicing in the field.

### Beam path

`docs/tank-level-sensor.md:43` — keep the cone clear of inlet pipes, fittings,
and baffles. Confirm the chosen cylinder's lid geometry before cutting.

---

## 6. Power budget — the number that must be measured

`SPEC.md` §4 asserts ~20–30 µA deep sleep, ~0.15–0.25 mA average, and ~2 years
real-world on 2× 18650. **Every one of those figures is [proposed], not
measured**, and the Heltec V3's onboard peripherals (OLED, LDO, Vext rail) are a
known place where a bare-ESP32-S3 sleep figure fails to hold. Treat the 2-year
target as unvalidated until it is measured on the bench.

```
I_avg  =  I_sleep  +  I_active × (t_active / (T_cycle × 60))
life   =  C / I_avg  ×  derate
```

| Term | Source |
|---|---|
| `C` — capacity | ~6000 mAh, 2× 18650 parallel (`SPEC.md` §4) |
| `T_cycle` | 10–15 min **[settled]** |
| `I_sleep` | **[verify] — measure. HW-02.** The dominant term. |
| `I_active`, `t_active` | **[verify] — measure.** See the sensor-warmup note below. |
| `derate` | `SPEC.md` §4 uses ~0.6 (cold + self-discharge) to get 3.4 yr → ~2 yr |

### Switch the sensor's power rail

The A02YYUW free-runs — powered, it transmits continuously. Left on the always-on
rail it would swamp the budget entirely. **Power it from a switched rail** (the
Heltec's Vext, or a MOSFET) so it draws only during the active window.

That makes `t_active` a firmware-visible cost: the sensor needs some settling
time after power-up before a reading can be trusted, and that settling time lands
directly in the battery equation. How long, and how many readings to take and
median, is **HW-08**.

### The tank node is the fleet's power testbed

One cheap sensor on a switched rail puts this node near the floor of what any
Soundings node draws. Measure it properly and you have calibrated the battery
model for every node type that follows — which is precisely what open issue
[#36](https://github.com/mobiustripper42/soundings/issues/36) asks for.

---

## 7. Calibration plan

### The problem with the current approach

`docs/tank-level-sensor.md:79-87` specifies a purely empirical fit — log sensor
reading against known volume at several fill levels, a couple below the IBC's
~46" top and a couple above, and let the breakpoint fall out. Its stated virtue
is that no tank dimensions are needed.

The hidden cost is a **calendar dependency**. The tanks are rain-fed. Getting
fill points on both sides of the breakpoint could take weeks or months, and it
requires an independent way to *know* the volume — a meter on the fill line or a
measured drawdown. Nothing in the repo says one exists.

### Recommended: analytic seed, empirical correction **[proposed]**

Measure the three tanks once with a tape measure — two cylinder diameters, the
IBC's footprint and height, and the height at which the IBC tops out. Compute the
two-segment curve directly from geometry. That is a twenty-minute job and it
yields a usable curve on day one.

Then correct it empirically as real fills happen, over whatever timeline the
weather provides.

This keeps everything `docs/tank-level-sensor.md` wanted — raw distance is
always published (`:100-102`), so any curve can be re-fit later from stored raw
with no reflash — while removing the schedule risk from the critical path.

### Consequence for sequencing

Calibration stops gating "up and running." Two milestones, and only the second
waits on weather:

- **M1 — first light:** real tank distance on a chart. Analytic curve. No fills needed.
- **M2 — calibrated gallons:** empirical correction from observed fills. Trails M1 by however long it takes.

**Still open:** is there an independent volume reference (a flow meter on the
fill, a metered drawdown)? It is no longer blocking under this plan, but it sets
how good M2 can get.

---

## 8. Build sequence

| # | Step | Gate to pass |
|---|---|---|
| 1 | Resolve the [verify] set via `CHAT_HANDOFF.md` | No BOM line still carries a part-changing [verify] |
| 2 | Order 2× Heltec V3 + 2 antennas | — |
| 3 | **Range test** (§2) | RSSI and loss recorded tank ↔ server. **D3 + D4 resolved.** |
| 4 | Order the rest of the BOM | Branch chosen: §3 recommended vs fallback |
| 5 | Bench: A02YYUW on a breadboard, USB power | A plausible distance in cm on the serial monitor |
| 6 | Bench: node firmware + gateway radio, end to end on the desk | A real packet decoded by the Python daemon |
| 7 | Measure sleep and active current (§6) | A real battery-life number replacing `SPEC.md`'s estimate |
| 8 | Measure the tanks, compute the analytic curve (§7) | A seed curve in gateway config |
| 9 | Build the enclosure and the lid mount (§5) | Sealed, glands down, node low and shaded |
| 10 | Deploy and watch | **M1 — first light** |
| 11 | Log fills as they happen | **M2 — calibrated gallons** |

Steps 5–6 need node firmware, which is the software half of Phase 3 and can be
built in parallel with steps 1–4 against the existing fakes.

---

## 9. Open questions

Live in `CHAT_HANDOFF.md` as `HW-01`…`HW-08`. Summarized here so this doc reads
standalone:

| ID | Question | Blocks |
|----|----------|--------|
| HW-01 | V3 antenna connector type and a suitable 900 MHz antenna | Ordering antennas |
| HW-02 | Achievable V3 deep-sleep current, and what must be disabled to get it | The 2-year claim |
| HW-03 | A02YYUW beam angle | Standoff tube ID/length, hole-saw size |
| HW-04 | A02YYUW stock cable length and whether UART can be extended to tank height | Cable BOM, mount feasibility |
| HW-05 | 18650 holder connector matching the Heltec battery input | Holder choice |
| HW-06 | Is the V3's onboard battery protection sufficient, or is a separate LVC board needed | LVC line item |
| HW-07 | A µA-capable current measurement option | Validating HW-02 at all |
| HW-08 | A02YYUW warm-up/settling time and read strategy | `t_active`, therefore battery life |

---

## 10. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| **Link won't close** tank → server | Forces the fallback architecture (§3) | Range test is step 3, before any other spend |
| **Deep sleep worse than 20–30 µA** | The 2-year target is wrong; service interval shrinks | Measure early (step 7), before building the enclosure around it |
| **Condensation dropouts** in the headspace | Intermittent bad readings | Standoff tube (§5); publish raw distance so dropouts are visible, not smoothed away |
| **Beam clipped** by a too-narrow standoff | Garbage readings that look plausible | HW-03 before cutting anything |
| **Cable can't reach** low-and-shaded enclosure | Node mounted hot, battery degrades | HW-04 before ordering; worst case accept a compromise mounting height and note it |
| **No volume reference** for M2 | Calibration stays approximate | Analytic seed curve (§7) makes M1 independent of this |

---

*This plan is a guide. When it disagrees with what the bench teaches us, the
bench wins and the plan gets updated.*
