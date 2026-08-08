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
>
> **🔒 §4 is the only purchasing authority.** If a part is not in the BOM below, it
> does not get ordered — a chat message or a research note is a *proposal*, never
> an order list. This is a rule with a scar behind it: on the sibling **tinkle**
> project, chat spec'd parts that never made it back into the repo, and the docs
> ended up describing a machine that no longer existed. Routing every order through
> this table forces promotion to happen first. See `docs/CHAT_HANDOFF.md` §1.

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

Buy two Heltec Wireless Stick Lite V3 boards and two antennas. Walk one to the
tank cluster, leave the other at the intended gateway location, and record RSSI
and packet loss **from the phone app** — no OLED, no laptop in the field.

### ⚠ Attach the antennas before the first power-on

Transmitting with no antenna reflects the power back into the SX1262's output
stage and can damage it. **Pigtail and antenna on both boards before either one
runs any TX code** — this is the single easiest way to destroy a board on day
one. It also happens to be the moment to seat the U.FL connector for good (RTV +
zip-tie, ~30 mating cycles, fragile latch).

### Powering the two boards

**No batteries for this test.** The JST 1.25 polarity is unverified (HW-05) and
the 2-slot holder has to be confirmed parallel-not-series — neither risk is worth
taking before the boards have proven they work.

| End | Power | Notes |
|---|---|---|
| Gateway (stationary) | **USB-C from the server or a laptop** | Also carries a serial log, if convenient |
| Node (walking) | **USB-C from a USB power bank** | Any 5 V bank; the board peaks around 150 mA on TX |

Two cautions:

- **Use USB-C *data* cables, not charge-only.** The board flashes over a
  USB-serial bridge; a charge-only cable powers it but won't flash it or return
  serial. Charge-only USB-C cables are common and indistinguishable by eye.
- **Some power banks auto-shut-off below a low-draw threshold.** A board idling
  between pings may fall under it and the bank cuts out mid-walk. Continuous
  transmission keeps draw up; if a bank still drops out, swap banks rather than
  debugging the link.

### Reading RSSI while walking

**Run it from the phone app** — that is the measurement surface, and it is why no
board needs an OLED (§3). The walking board needs power and nothing else: no
laptop, no serial tether, no display.

Still worth logging at the **gateway** end if a serial connection is convenient
there, since receive-side RSSI at the fixed location is what the link budget
ultimately turns on. But it is a convenience, not a requirement.

That single afternoon resolves **both remaining hardware deferrals**:

- **D3 (node↔gateway PHY pairing)** — dissolved outright if both ends are
  SX1262. See §3.
- **D4 (gateway box)** — the test tells you whether the radio can live at the
  server or has to move.

Everything downstream branches on the answer, and the two boards are needed
regardless of which branch wins. There is no version of this project where
buying two Heltec boards is wasted.

**Record:** RSSI and loss rate at the tank, at the intended server location, and
at the two tunnels — plus the same numbers with the tank lid closed and the node
inside its (eventual) enclosure, which both cost signal.

---

## 3. Gateway architecture — recommendation

### Board choice: 2× Heltec Wireless Stick Lite V3 **[proposed]**

> **Decided 2026-08-08.** Same ESP32-S3 + SX1262 as the WiFi LoRa 32 V3, without
> the OLED, ~$10 cheaper per board.

The money is the least interesting argument. **The OLED is dead weight in a sealed
outdoor node, and it is the direct cause of several problems Round 1 found:**

- Three of the five items on HW-02's deep-sleep disable list exist **only** because
  of the OLED (SPI/OLED pins to `INPUT`, `displayOff()`, Vext off).
- **F-6 — Vext's ~1.44 V residual is caused by the OLED's pull-up** (the V3.1
  schematic's 10K to VDD_3V3). No OLED plausibly means no residual, which is the
  entire reason a discrete P-FET load switch entered the BOM.
- One fewer component to fail in a condensing headspace, in a smaller board that
  is easier to seal.

**The one counter-argument is gone.** The OLED's only real use was reading
return-path RSSI during the range test without carrying a laptop — and the range
test is run from a phone app (§2), so no board-mounted display is needed at
either end. A briefly-considered mixed pair (Stick Lite node, OLED gateway) is
therefore dropped: it would have cost board interchangeability to buy a readout
that isn't used.

Two identical boards keeps what the original plan wanted from them — one flashing
toolchain, one antenna type, one pinout to learn, and a spare that can stand in
for either role while debugging.

**What this risks.** HW-02's disable list, HW-05's battery connector, and HW-14's
pin numbers were all researched against the **WiFi LoRa 32 V3**. If the Stick
Lite's pinout differs, part of Round 1 gets re-run — **HW-15…HW-17**, open. This
matters more now that *both* boards are Stick Lites, not less.

**It does not block the order or the range test.** A range test needs two boards
with matching radios and nothing else; the pin questions bite at build steps 5–6.
Order now, resolve HW-15…HW-17 before firmware.

### Recommended: the second board as a dumb serial radio **[proposed]**

The gateway radio is the second Stick Lite, tethered by USB to the existing
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

### Fallback: the board as a WiFi→MQTT bridge **[proposed]**

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

> **Round 1 resolved (2026-08-08).** Eight `[verify]` tags cleared, two parts
> added, two removed. Full answers and sources in `docs/CHAT_HANDOFF.md` §3;
> CC's review of them in §3.3.

| Item | Qty | Status | Notes |
|---|---|---|---|
| **Heltec Wireless Stick Lite V3** | 2 | [proposed] | §3. One node, one gateway radio — identical boards, so a spare covers either role. No OLED: removes 3 of 5 sleep-disable steps and probably the Vext residual (F-6). ⚠ Pinout transfer from the WiFi LoRa 32 V3 is **HW-15…HW-17**. |
| A02YYUW ultrasonic sensor | 1 | [settled] | UART, free-running, 9600 8N1. `docs/tank-level-sensor.md` |
| **DS18B20, headspace air** | **1** | **[settled]** | **New — DEC-007.** Required for temperature compensation, not optional. 1-Wire, one GPIO. Form factor is **HW-12**. |
| 18650 cells, **protected** | 2 | [settled] | Parallel, ~6000 mAh combined. **Protected** per DEC-006 (~$2/cell premium). ⚠ Match capacity and internal resistance, and **equalise state of charge before joining** — cells at different SoC dump unlimited current into each other. |
| 18650 holder, 2-cell **parallel** | 1 | [settled] | ⚠ **Confirm PARALLEL, not series.** Most 2×18650 lead holders are series (7.4 V) and **will destroy the board.** |
| Battery pigtail, **JST 1.25 2-pin** | 1 | [settled] | HW-05. Heltec calls it "SH1.25-2" — the name is wrong (real JST SH is 1.0 mm pitch). Order parts listed as *"JST 1.25 2-pin for Heltec/LilyGo."* ⚠ **Verify polarity with a meter**, not wire colours: USB in, **no battery**, probe each JP1 pin to a GND header pin; the one near 4.2 V is +. |
| ~~Low-voltage-cutoff board~~ | — | **removed** | **DEC-006.** The V3's TP4054 is a charger with no discharge-side protection, but protected cells + a 3.2 V/cell firmware cutoff cover it better. |
| **P-FET load switch** (+ gate resistor, pull-up) | 1 | [verify] | **New — HW-11.** Switches the sensor rail from VDD_3V3. ⚠ **Vext cannot do this job** — it holds ~1.44 V residual (F-6). |
| IP65 enclosure, light-colored, ~4×4×2" | 1 | [proposed] | Hinged lid preferred. Mounted **low and shaded** (`SPEC.md` §4 heat spec) |
| PG7 cable glands | 2 | [proposed] | Sensor cable + spare. Pointing **down**, siliconed both sides |
| **U.FL → SMA-female pigtail**, 100–150 mm | 2 | [settled] | HW-01. The V3 has **U.FL/IPEX only, no SMA on the PCB**. ⚠ Keep it **as short as it ships** — U.FL pigtail is ~1–1.5 dB/m at 900 MHz; a long run throws away more than the antenna gain provides. |
| SMA bulkhead | 2 | [settled] | Through the enclosure wall, antenna direct on the outside. ⚠ **Not weatherproof by default** — self-amalgamating tape on the outside joint. |
| **915 MHz antenna, 3–5 dBi omni, SMA male** | 2 | [settled] | HW-01. Vertical. ⚠ **Skip 8–10 dBi fiberglass sticks** — the gain comes from flattening the vertical pattern, so if gateway and tank sit at different heights you shoot over or under. |
| Wago lever-nut terminal block | 1 | [settled] | Serviceable internal connections |
| Silica gel desiccant | 1 | [settled] | Replaced annually |
| **Outdoor/direct-burial Cat5e**, ~tank height + slack | 1 | [settled] | HW-04. Stock A02YYUW lead length is unpublished; assume short and plan to extend. **Shielding not required.** Extension to several metres is fine — 9600 8N1 is 104 µs/bit, nowhere near a timing limit. |
| 100 nF + 10 µF capacitors | 1 ea | [settled] | At the **sensor end** of the run |

**Wiring the sensor run (HW-04).** Three conductors suffice: **V+, GND, TX**. RX
is a mode-select strap — hardwire it low at the sensor for real-time mode. V+ with
its own GND in one twisted pair, TX with a second GND in another; parallel the
spare conductors onto V+/GND. If shielded cable gets used anyway, **ground the
shield at the node end only** — both ends in a wet outdoor run invites a ground
loop. ⚠ **Keep out of any conduit shared with the Grundfos pump wiring** — a far
bigger noise source than cable length. Beyond ~10 m or on persistent checksum
failures, move the ESP32 to the lid and run RS-485 instead.

⚠ **U.FL is rated ~30 mating cycles and the latch is fragile.** Assume it gets
mated twice, ever: RTV dab plus a zip-tie strain relief once seated.

### Mount — at the tank lid

| Item | Qty | Status | Notes |
|---|---|---|---|
| **PVC pipe, 4" or 6" Sch40** | 1 | [settled] | HW-03. Cut to **50–75 mm**, sensor face flush with the top of the collar. A shallow wide *hood*, not a deep narrow tube. |
| **Hole saw, 4" or 6"** | 1 | [settled] | Matched to the collar OD |
| Silicone sealant, UV-stable | 1 | [proposed] | Lid penetration + gland seals |
| Enclosure mounting hardware | — | [proposed] | Depends on what the node straps to at the tank cluster |

### Gateway — at the server

| Item | Qty | Status | Notes |
|---|---|---|---|
| Heltec Wireless Stick Lite V3 | — | [settled] | The second board above — identical to the node, so either can take either role |
| USB-C cable, long | 1 | [proposed] | Length depends on where the radio ends up relative to the box |
| U.FL→SMA pigtail + bulkhead + antenna | — | [settled] | Second set from the node rows above |

> ⚠ **F-7 — the gateway shares the board but not the constraints.** It is
> mains/USB powered. **HW-01 (antenna) applies to it; the battery, holder,
> protected cells, load switch, and sleep budget do not.** Do not let the BOM
> merge the two roles.

### Bench and validation tools

| Item | Status | Why |
|---|---|---|
| **USB-C data cables** ×2 | [settled] | ⚠ **Data, not charge-only** — the V3 flashes over a CP2102 bridge. Likely on hand. |
| **USB power bank** | [settled] | Powers the walking board during the range test. Any 5 V bank; watch for low-draw auto-shutoff. Likely on hand. |
| Multimeter | [settled] | **The only current instrument this build needs.** DEC-006 — the design errors worth catching (SX1262 not slept, OLED left on) are 1–40 mA and a multimeter in series reads them fine. |
| ~~Nordic PPK2 (~$90–110)~~ | **not purchased** | DEC-006. Buys resolution *within* the µA band that the ~1.8× margin doesn't need. Revisit at 3+ distinct node designs. |
| Tape measure | [settled] | Calibration, §7 |

---

## 5. Mounting design

### What the standoff is for

`docs/tank-level-sensor.md` specifies recessing the transducer in a PVC collar
through the lid, to fight condensation dropout in the humid headspace. It has a
second effect: a recessed transducer sits higher, so every reading is longer by
the collar length, pushing the tank's full line further from the blind zone.

**That second effect turned out to be nearly free**, because HW-09 found the blind
zone is **3 cm**, not the 20–25 cm this doc originally assumed (that figure is the
JSN-SR04T's). A 50–75 mm collar already exceeds it, so the practical top-of-tank
dead zone is **effectively zero** and the earlier "top clamps to full" caveat is
gone. The collar's remaining job is condensation.

### The constraint that limits it — resolved (HW-03)

You cannot simply make the tube long. The beam leaves the transducer as a **60°
full cone (30° half-angle**, DFRobot spec table; some resellers claim 15° and
contradict the manufacturer). If the tube is too narrow or too long, the cone
strikes the wall and returns garbage that looks plausible.

```
L_max  ≈  1.73 × r_inner
```

| Pipe | Inner radius | Max standoff |
|---|---|---|
| 3" Sch40 | 39 mm | 67 mm |
| **4" Sch40** | 51 mm | **89 mm** |
| **6" Sch40** | 77 mm | **133 mm** |

**Use 4" or 6" Sch40, cut to 50–75 mm, sensor face flush with the top of the
collar.** A shallow wide hood, not a deep narrow tube — "short standoff tube" in
the original sensor doc is both shorter and much wider than the phrase implies.

### The bigger finding: the cone reaches the sidewall (F-3)

The cone keeps expanding past the collar. For an 1100-gal vertical cylinder
(≈1.4–1.6 m diameter depending on height), **it touches the tank wall
~1.24–1.41 m below the sensor** — well before it reaches the bottom of an empty
tank.

This does not break the measurement: a specular return off a flat water surface
directly below is normally the first and strongest echo. But it sets up
**occasional short-reading outliers at low level**, with internal ribs and a
centre draw pipe as candidate false targets. Three mitigations, all cheap:

- **Mount dead centre**, cone clear of the fill inlet stream.
- **Median of 5–7 frames**, not mean (HW-08) — these failures are outliers, and a
  median ignores what a mean chases.
- **Gateway band-check** — reject anything outside 3 cm–tank height before it
  reaches the volume curve.

Survey the chosen cylinder's interior for ribs and a draw pipe before cutting.

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

## 6. Power budget

**Round 1 changed the shape of this section.** `SPEC.md` §4's ~20–30 µA sleep
figure is **confirmed conservative** — a cited PPK2 measurement on a bare V3 puts
it at **~16 µA** (HW-02). But the same round found that **sleep was never the
problem**: wake energy dominates by 5:1.

### Budget at a 15-minute interval

| Term | Over 2 years |
|---|---|
| Sleep @ 20 µA | 350 mAh |
| Wakes @ ~2 s, ~45 mA avg (96/day) | ~1,750 mAh |
| Self-discharge ~3 %/yr | ~360 mAh |
| **Total** | **~2,460 mAh** |

Against 6000 mAh nominal, **derated to ~4,500 mAh effective for cold** (Li-ion
delivers ~70 % of rated near −10 °C; no charging, so no plating risk — just
capacity). **~1.8× margin.**

**Cadence: 15 minutes, not 10.** At a 10-minute interval the margin drops to
~1.3×, which is thin. `SPEC.md` §3 settled the range at 10–15 min; this picks the
top of it on evidence rather than taste. Tank level moves on rain events and
irrigation runs — it does not need 10-minute resolution.

**All of the margin lives in `t_active ≈ 2 s`.** That is the number to watch.

### Getting to 16 µA — the disable list (HW-02)

Ordered by how often each is missed:

1. **SX1262 explicitly to sleep** before `esp_deep_sleep_start()`. The radio does
   not sleep just because the MCU does.
2. **All LoRa SPI + OLED pins to `INPUT`** — NSS, MISO, MOSI, SCK, RST, BUSY,
   SDA_OLED, SCL_OLED, RST_OLED. This is the step that got the cited measurement
   from ~130 µA to 16 µA.
3. **OLED off** — both `displayOff()` and Vext.
4. **GPIO37 / ADC_Ctrl off.** It gates the 390K/100K battery-sense divider; left
   on it bleeds through 490K continuously. ⚠ But it must be switched **on**
   briefly each wake to read `battery_mv` — which DEC-006 makes the long-run
   validation path.
5. `gpio_deep_sleep_hold_en()` / `rtc_gpio_hold_en()` on anything that must hold
   state, or pins float on the way into sleep.

The LDO's (CE6260B33M) enable is not broken out — its quiescent current is the
floor. CP2102 runs off VBUS and costs nothing on battery. ⚠ **Pin numbers
(GPIO36 Vext, GPIO37 ADC_Ctrl) are cited from a V3.1 schematic — confirm on the
board actually received** (HW-14). Board revisions move pins.

### ⚠ Vext cannot switch the sensor rail (F-6)

The A02YYUW free-runs: powered, it streams frames continuously. It must sit on a
switched rail or it swamps the budget. **Vext is not that rail** — it holds
**~1.44 V residual** with the OLED present ([meshtastic/firmware#2591](https://github.com/meshtastic/firmware/issues/2591);
the V3.1 schematic shows a 10K pull to VDD_3V3).

Use a **discrete P-FET load switch from VDD_3V3**, GPIO held across sleep with
`rtc_gpio_hold_en()`. Part choice is **HW-11**. Any doc or code assuming Vext
gates the sensor is wrong.

### Sensor cost, in context (HW-08)

~800 ms rail-on at ~8 mA ≈ **6.4 mA·s ≈ 0.0018 mAh/wake**. The ESP32-S3 awake
beside it at ~40 mA burns ~32 mA·s over the same window — **5× the sensor.**
Optimise total awake time; do not contort the read strategy to save sensor
milliseconds.

### How this gets validated (DEC-006)

- **On the bench, with a multimeter:** confirm sleep current is **microamps, not
  milliamps.** That is the whole check. The design errors that matter (radio not
  slept, OLED on, SPI floating) are 1–40 mA and a multimeter in series reads them
  fine. A PPK2 buys resolution inside the µA band that the 1.8× margin has no use
  for — see DEC-006 for why it isn't being bought.
- **In the field, from telemetry:** `battery_mv` is already a fixed header field
  in every packet (`contracts/packet-v1.md:45`) — no schema change needed. A
  server-side alert at 3.4 V/cell (Phase 5.2) turns the deployed node's own
  discharge curve into the long-run evidence, measured in the real thermal
  environment rather than on a bench.

### The tank node is still the fleet's power testbed

One cheap sensor on a switched rail puts this node near the floor of what any
Soundings node draws. Characterise it and the battery model is calibrated for
every node type that follows — which is what open issue
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
| 1 | ~~Resolve the `[verify]` set~~ | ✅ **Done 2026-08-08** — Round 1 closed, 10 answers promoted |
| 2 | Order 2× Stick Lite V3, 2 antennas, 2 pigtails + bulkheads | Round 2 does **not** gate this |
| 3 | **Range test** (§2) | RSSI and loss recorded tank ↔ server. **D3 + D4 resolved.** |
| 4 | Order the rest of the BOM | Branch chosen (§3); HW-11/HW-12 answered |
| 5 | Bench: A02YYUW on a breadboard — **at 3.3 V** | A plausible distance in cm. **Resolves HW-13**; if flaky, a boost converter enters the BOM and §6 is re-derived. Confirm HW-14 pins with a meter here too. |
| 6 | Bench: node firmware + gateway radio, end to end on the desk | A real packet decoded by the Python daemon |
| 7 | Multimeter check: sleep current is **µA, not mA** (§6) | Confirms the disable list actually took |
| 8 | Measure the tanks, compute the analytic curve (§7) | A seed curve in gateway config |
| 9 | Build the enclosure and the lid mount (§5) | Sealed, glands down, node low and shaded, collar dead-centre |
| 10 | Deploy and watch | **M1 — first light** |
| 11 | Log fills as they happen | **M2 — calibrated gallons** |

**Steps 2–3 are unblocked right now.** Round 2 (HW-11…HW-14) only gates step 4
and beyond, and two of its four items are answered *by* the bench at step 5.

Steps 5–6 need node firmware, which is the software half of Phase 3 and can be
built in parallel with steps 1–4 against the existing fakes.

---

## 9. Open questions

Live in `CHAT_HANDOFF.md` as `HW-01`…`HW-08`. Summarized here so this doc reads
standalone:

**Round 1 (HW-01…HW-10) is closed** — all promoted 2026-08-08. It yielded DEC-006
(battery protection), DEC-007 (temperature compensation), and the corrections now
folded into §4, §5 and §6.

**Round 2 is open.** None of these block the range test (§2), which remains the
next physical step.

| ID | Question | Blocks |
|----|----------|--------|
| HW-11 | P-FET load switch part choice for the sensor rail | BOM — Vext can't do it (F-6). ⚠ May be **moot** if the Stick Lite has no Vext pull-up (HW-15). |
| HW-12 | DS18B20 form factor for a condensing headspace — bare TO-92 or stainless probe | BOM |
| HW-13 | Does the A02YYUW work reliably at 3.3 V? Resellers recommend 5 V (F-8) | If not: a boost converter enters the BOM and §6 is re-derived. **Bench-test at step 5.** |
| HW-14 | Confirm GPIO36 = Vext, GPIO37 = ADC_Ctrl on the board revision received | Firmware correctness. **Verify on receipt.** |
| HW-15 | Stick Lite V3 pinout, U.FL, and battery connector vs the WiFi LoRa 32 V3 | Whether HW-02/05/14 transfer or get re-run. Firmware, not the order. |
| HW-16 | Stick Lite V3 charge IC — still TP4054, still no discharge protection? | **DEC-006 rests on this** |
| HW-17 | Stick Lite V3 deep-sleep current — below the V3's 16 µA with no OLED? | Power budget §6; possibly shortens the disable list |
| HW-18 | Does the Stick Lite have a Vext rail, and does the ~1.44 V residual survive with no OLED? | **May delete HW-11 entirely** — no residual means no P-FET needed |

---

## 10. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| **Link won't close** tank → server | Forces the fallback architecture (§3) | Range test is step 3, before any other spend |
| **A02YYUW unreliable at 3.3 V** (F-8/HW-13) | A boost converter enters the BOM and §6 is re-derived | **Bench-test at step 5**, before the enclosure is built around it |
| **`t_active` exceeds ~2 s** | The entire 1.8× margin lives here; overrun eats it directly | Measure awake time at step 6–7; 15-min cadence already banked the cheap margin |
| **Series holder wired by mistake** | 7.4 V destroys the board | Confirm parallel at unboxing; verify JP1 polarity with a meter before first connection |
| **Sidewall / rib false echoes** at low level (F-3) | Short-reading outliers exactly where the pump-dry risk lives | Dead-centre mount, median-of-5–7, gateway band-check (§5) |
| **Headspace stratification** defeats a single temp sensor | Residual error after correction | Accepted (DEC-007) — turns ~14 cm into low-single-digit cm; second sensor is a cheap later fix |
| **Deep sleep worse than cited 16 µA** | Service interval shrinks | Multimeter check at step 7 confirms µA vs mA — the distinction that matters |
| **Condensation dropouts** in the headspace | Intermittent bad readings | Wide shallow collar (§5); publish raw distance so dropouts stay visible, not smoothed away |
| **Cable can't reach** low-and-shaded enclosure | Node mounted hot, battery degrades | Cat5e extension is settled (HW-04); beyond ~10 m, move the ESP32 to the lid and run RS-485 |
| **U.FL connector damaged** by re-mating | Dead antenna path, board rework | Rated ~30 cycles, fragile latch — RTV + zip-tie once seated, assume two matings ever |
| **No volume reference** for M2 | Calibration stays approximate | Analytic seed curve (§7) makes M1 independent of this |

---

*This plan is a guide. When it disagrees with what the bench teaches us, the
bench wins and the plan gets updated.*
