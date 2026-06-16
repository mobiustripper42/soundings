# Soundings — V1 Specification

*Bay Branch Farm wireless sensor mesh. This SPEC is a **guide to keep us on
track**, not a contract. Source: a planning chat consolidated 2026-05-23, plus
the decisions made while reframing the project for a software-first build.*

> **Read this doc with the status tags in mind.** Almost nothing here is verified
> against real hardware yet. The tags say how much weight each item bears:
>
> | Tag | Meaning |
> |-----|---------|
> | **[settled]** | A real decision already made. Build on it. |
> | **[proposed]** | The current plan, **not yet validated** in simulation or on the bench. May change. |
> | **[deferred]** | A decision we are deliberately *not* making yet — see the Deferred Decisions register (§12). |
> | **[stretch]** | Wanted, not required for V1. |
>
> **Governing principle — nothing gets locked until it has to.** We defer every
> decision we can to the moment simulation or the bench forces it. The one
> exception: a *hardware* choice that would change the *software* we write gets
> made early — but only after we've confirmed it can't sit behind an adapter and
> be faked in simulation (most can).

---

## 1. What Soundings is

**Soundings** is a LoRa wireless sensor network for Bay Branch Farm. Battery
field nodes measure soil and air conditions and report them over radio to a
gateway on the farm LAN, which decodes the readings and stores them on an
existing headless server for viewing in a dashboard.

The name is nautical — a *sounding* is a measurement of the conditions below,
which is what the sensors do. Plural because there will be many.

**The one-sentence pipeline:**

```
battery node  →  LoRa radio  →  gateway (decodes)  →  message bus  →  database  →  dashboard
```

**Primary goal:** soil-moisture *tension* at multiple depths in multiple beds —
the measurement that can't be gotten any other way. Tunnel air temp/humidity
(for VPD) and soil temperature are secondary but integrated from day one.

---

## 2. Philosophy

- **Read-only telemetry.** V1 sensors observe and report. No actuation, no
  control outputs, no valves. A failed node means *missing data* — nothing
  worse. This keeps V1 low-stakes on purpose. **[settled]**
- **Nothing locked until it has to be.** See the governing principle above.
  Infrastructure decisions in particular (the server stack, the gateway box, the
  packet details) are deferred until simulation or the bench forces a real
  answer.
- **Farm-owned, no cloud, no subscription.** Both ends of every radio link are
  ours. No LoRaWAN network server, no vendor lock-in, no recurring fees. The
  building, programming, and dashboarding *are the point*, not just the data.
  **[settled]**
- **Software first, hardware second.** Build and prove the entire software stack
  against simulation now, so the winter bench build is tuning, not authoring.
  Sensors and radios sit behind adapters; fakes drive them in simulation and get
  swapped for real drivers at the bench.
- **Build crappy, learn, rebuild.** The first node will be ugly — breadboard,
  taped battery. Deploy it anyway; 30 days of real operation teaches more than a
  month of planning.

---

## 3. Scope

### In for V1

- One replicated battery node design (Heltec WiFi LoRa 32 V3).
- **Bed nodes** — soil moisture (Watermark) + soil temp (DS18B20) + canopy VPD
  (SHT45).
- **Tunnel-air nodes** — canopy VPD (SHT45).
- **Tank-level node** — ultrasonic level on the catchment cluster (see §5.4).
- Raw LoRa point-to-point radio, US 902–928 MHz.
- Gateway that decodes packets and publishes them onto the farm network.
- Storage + a phone-usable dashboard + basic alerting.
- Davis weather station data pulled into the same store (§7).
- The full software stack proven in simulation before any of the above is built
  in hardware.

### Not V1

- **Irrigation control / actuation of any kind.** Soundings only measures. The
  irrigation controller is a separate project (**tinkle**), which may *consume*
  Soundings data but Soundings never drives a valve. **[settled]**
- **Closed-loop / advisory-to-tinkle layers.** Forecasting drydown or emitting
  irrigation hints (the "Tiller" VPD-forecast idea) is a future project — parked,
  not V1.
- **Orisha tunnel-controller replacement** — separate, parked.
- **OTA firmware updates** — V1 reflashes over USB during the annual service
  window. Don't build OTA before there's firmware worth updating.
- **Continuous soil pH, light/PAR** — rejected (wrong tool / available elsewhere).
- **Soil EC, CO₂** — parked.

---

## 4. Field node — hardware

One node design, replicated. **Exactly one programmable unit per node**: the
Heltec board is the microcontroller *and* the LoRa radio on a single board.
"Arduino" refers only to the programming environment — all logic runs on the one
Heltec.

### Core components per node **[settled]** (component list) / **[proposed]** (specific parts)

| Component | Notes |
|---|---|
| Heltec WiFi LoRa 32 V3 | ESP32-S3 + SX1262 LoRa radio, one board, USB-C flashing |
| 2× 18650 cells, parallel | ~6000 mAh combined |
| Low-voltage-cutoff protection board | ~$3, protects cells from over-discharge |
| IP65 enclosure | Light-colored, ~4×4×2", hinged lid preferred |
| Wago lever-nut terminal block | Internal, serviceable sensor connections |
| PG7 cable glands | One per sensor cable entry, pointing down |
| SMA bulkhead + 3 dBi whip antenna | Through top of enclosure |
| Silica gel desiccant | Replaced annually |
| Perfboard | For the AC excitation circuit |

### Power — no solar **[settled]**

Deep sleep ~20–30 µA; active cycle ~3–5 s every 15 min; average ~0.15–0.25 mA.

- 2× 18650 parallel ≈ 3.4 yr theoretical; derated for cold ≈ **~2 yr real-world**.
- **Solar deliberately rejected for V1** — power needs are tiny, and solar adds a
  panel, a charge controller (a common moisture-failure point), wiring, and
  another enclosure penetration. Not worth it.
- **WiFi disabled in firmware** — leaving it on destroys the battery budget.
- The annual battery swap is also the service window: pull node, blow out
  enclosure, check sensors, replace desiccant, reflash via USB.

### Enclosure & survivability **[settled]**

- IP65 min, UV-stable, light-colored to limit solar heat gain.
- Cable entries: PG7 glands pointing **down**, silicone both sides.
- **Heat is a battery-placement spec.** ESP32 is fine to 85 °C; the 18650 cells
  degrade above ~45 °C. Mitigation (all three): mount **low and shaded**,
  light-colored enclosure, **never on a south wall in full sun**.

---

## 5. Sensors

### 5.1 Soil moisture — Watermark **[settled]** (anchor sensor)

- 2× Irrometer Watermark 200SS (commercial reference) + 6–8 homemade gypsum-block
  sensors (plaster of Paris + stainless bolts), deployed as **matched pairs
  side-by-side** for a commercial-vs-homemade head-to-head. Log raw resistance,
  calculated tension, and soil temp separately for each.
- **Depths:** 6" (root zone) and 12" (deeper reserve). No 18" — confirmed
  unnecessary.
- **Excitation:** DIY AC circuit — ~1 kHz square wave across two GPIOs, sample
  both half-cycles, average to cancel DC offset. **AC is mandatory** — DC
  polarizes the gypsum and corrodes the sensor. (v2 circuit design **[deferred]**.)
- **Calibration (resistance → tension), temperature-compensated — mandatory:**
  ```
  kPa = (4.093 + 3.213·R) / (1 − 0.009733·R − 0.01205·T)
  ```
  R = resistance (kΩ), T = soil temp (°C). Skipping temp compensation causes
  20–30% drift between morning and afternoon. *Where this math runs (on-node vs
  gateway)* is **[deferred]** — see §12.
- **Known limit:** noisy/compressed below ~10 cb (wet end). Good for "is it time
  to irrigate," not precise saturation. Acceptable.
- **Install discipline:** 24 h soak-prime, slurry backfill, no air gaps. A bad
  install is permanently bad data.

### 5.2 Soil temperature — DS18B20 **[settled]**

- Waterproof stainless-probe DS18B20 (±0.5 °C).
- **Depths:** 6" and 12", **co-located with the Watermarks** — each Watermark
  needs a temp reading at its own depth for the kPa compensation. Paired by
  design.
- **Wiring:** 1-Wire bus; both probes daisy-chain on 3 conductors; one 4.7 kΩ
  pull-up at the node. Reliable to 20–30 ft.
- **Optional:** a third DS18B20 at surface/2" for spring soil-warming. Not
  required for V1. **[stretch]**

### 5.3 Air temp/humidity → VPD — SHT45 **[settled]**

- **SHT45** (±0.1 °C, ±1% RH), chosen over SHT31 because RH error propagates
  straight into VPD. BME280/DHT22 rejected (RH drift).
- **Produces VPD, not RH.** Vapor Pressure Deficit is the air's drying power and
  drives transpiration and disease pressure.
  ```
  SVP = 0.61078 · exp((17.27·T) / (T + 237.3))   [kPa]
  AVP = SVP · (RH / 100)
  VPD = SVP − AVP
  ```
  Tomato daytime band ~0.8–1.2 kPa. VPD is **computed, not measured** — measure T
  and RH, compute downstream.
- **Placement:** one SHT45 at **canopy height (~5 ft)** per tunnel, in its own
  radiation shield (§6), **mounted outside the electronics enclosure** — or it
  reports box temperature.
- **Wiring:** I2C, within 3–4 ft of the node (or an extender). Multiple SHT45s on
  one bus need a TCA9548A I2C multiplexer (they share an address) — only relevant
  to the multi-SHT45 stratification rig.

### 5.4 Tank level — A02YYUW ultrasonic **[settled]**

A single ultrasonic distance sensor on one dedicated node, measuring the shared
level across the farm's three plumbed-together catchment tanks (2× 1100-gal
cylinders + 1× 330-gal IBC, 2530 gal total). The tanks share a level
(communicating vessels), so one sensor covers all three.

- Non-contact (nothing touches the irrigation water); cleaner UART output than
  the cheaper JSN-SR04T; mounted in the lid of the tallest cylinder, recessed in
  a short PVC standoff to fight condensation dropout.
- ~20–25 cm dead zone clamps the very top to "full" — acceptable; the load-
  bearing range is the bottom, where running a pump dry is the real risk.
- Distance → gallons via a **two-segment empirical curve** (the IBC tops out
  around 46", so gallons-per-inch steps there). Fit empirically from a few known
  fills; no tank measurements needed. **Always publish raw distance** alongside
  derived gallons/percent so the curve can be re-fit in software without touching
  hardware.
- **Consumer:** the tank level is what tinkle would use for a future low-tank
  pump lockout (tinkle's side, V2). Soundings' job ends at publishing the level.
- Full detail: `docs/tank-level-sensor.md`.

### 5.5 Leaf wetness **[stretch — high priority]**

A flat grid sensor mimicking a leaf, reporting hours of leaf wetness — the
primary driver of most fungal infection models, more actionable than humidity
alone. Commercial (~$100+) or DIY grid. Highest-priority stretch goal; a
candidate for promotion to core.

### 5.6 Vertical stratification rig **[stretch]**

A **portable diagnostic tool**, not permanent infrastructure — one node moved
between structures for a few weeks at a time. Sensors at 2'/7'/ridge: one SHT45
at canopy + DS18B20s above and below (vertical *temperature* profile matters more
than vertical humidity). Purpose: quantify heat stratification and whether the
HAF fans actually destratify. Built freestanding for easy relocation, after core
canopy nodes work.

### Rejected / parked

| Sensor | Decision | Reason |
|---|---|---|
| Continuous soil pH | rejected | Probes drift; pH moves over weeks — a sampling measurement, not telemetry. |
| Soil EC / salinity | parked | Handheld covers spot-checks; good continuous sensors are expensive. |
| CO₂ (SCD41) | parked | Power draw breaks the no-solar budget; no enrichment in use. |
| Light / PAR | rejected | Available from the Davis station; not a daily decision input. |
| Wind / solar radiation | elsewhere | Davis Vantage Vue via WeatherLink Live (§7). |

---

## 6. Radiation shields **[settled]**

Any SHT45 sits in a radiation shield, or it reports radiant/enclosure heat
instead of true air temp and corrupts VPD. DIY stacked-plate shields (light-
colored discs with airgaps) are proven and acceptable. One per SHT45 location.

---

## 7. Radio, gateway & server

### Radio **[settled: P2P]** / **[deferred: chip pairing]**

- **Raw LoRa point-to-point, not LoRaWAN.** Single farm, both ends ours;
  LoRaWAN's network-server layer adds complexity with no benefit. US 902–928 MHz.
- Range is a non-issue across a half-acre. Nodes transmit once every 10–15 min,
  ~50–100 ms each — far below any regulatory or collision concern.
- **Wake jitter:** randomize each node's wake by ±30 s so nodes sharing a clock
  don't collide.
- **Chip pairing is [deferred]:** the node's radio is the Heltec's onboard SX1262;
  the spec's gateway radio (RFM95W) is a different SX127x family. They interoperate
  if they share PHY settings, but the gateway's real driver depends on the chip.
  Deferrable because the radio sits behind an adapter and is faked in simulation —
  decided at the bench. See §12.

### Gateway **[proposed]**

A small always-on box on the LAN, **near the center of the farm**, holding the
LoRa antenna. Its only job: catch raw packets off the air, decode the binary,
map node-ID → location, and publish onto the message bus. It must handle lost/
malformed packets gracefully and log bad packets for debugging.

- Spec's proposal is a **Raspberry Pi Zero 2 W + RFM95W breakout** (~$60),
  running a custom Python decoder daemon.
- Central + on-LAN placement means antenna reach isn't a constraint, so the exact
  box is **[deferred]** — a Pi, a Heltec acting as a LoRa→WiFi bridge, or hanging
  the radio off the server directly are all live options.

### Server stack **[proposed — all of it unvalidated]**

Runs on an **existing headless Linux box** (not a Pi — the Pi, if used, is only
the radio gateway). `mill-dev` (the Tailscale dev VPS) is the natural place to
develop and simulate against, separate from the box that runs it on the farm.

```
gateway → message bus → ingestion → time-series DB → dashboard
```

- **Message bus:** Mosquitto (MQTT). Pub/sub broker — same idea as MQ Series,
  lighter wire protocol. Topics like `farm/tunnel/red/bed3/soil_moisture_6in`.
- **Time-series DB:** *contested — see §12 D6.* The source spec proposed
  TimescaleDB (PostgreSQL extension) so SQL JOINs can correlate sensor data
  against existing farm records (yield, fertigation, journal). A separate
  @architect review argued for **VictoriaMetrics** instead (single Go binary,
  retention-as-a-flag, lowest unattended-ops burden) and against InfluxDB
  (forced version-churn migrations). The deciding question is whether we truly
  need relational JOINs to farm records — the only thing that requires a SQL
  store. Set up **downsampling/retention early** regardless: raw 10–15 min for
  1 yr, hourly for 5 yr, daily indefinitely.
- **Dashboard:** Grafana — one per tunnel plus an overview, phone-usable.
- **Alerting:** tunnel temp over threshold; soil tension over ~80 cb in a tomato
  bed; node silent > 45 min (likely dead battery or wet enclosure).

Every piece of this stack is a *proposal carried from the source spec*, not a
validated choice. We confirm or replace each one when we reach it in simulation.

### Weather — separate but co-located **[proposed]**

Weather (rain, wind, solar radiation, barometric) comes from the existing **Davis
Vantage Vue** via a **WeatherLink Live** bridge (acquisition pending), which
exposes a **local HTTP JSON API** — no cloud, polled ~60 s by the server. Two
parallel systems, one server, one dashboard: Davis owns weather at one canonical
point; Soundings owns soil + per-tunnel VPD at many points. Both feed the same DB
and dashboard.

---

## 8. Firmware approach **[proposed]**

- **One firmware**, configured per node type (bed / tunnel-air / tank / rig).
- **On-node logic:** AC excitation waveform, ADC sampling, DS18B20 1-Wire reads,
  SHT45 I2C reads, deep-sleep management, packet assembly + LoRa transmission.
  WiFi disabled.
- **Host-testable core.** Sensor math, packet (de)serialization, and the wake
  cycle live in platform-independent code exercised by host unit tests with fake
  sensors and a fake clock — the same pattern that let tinkle's firmware be built
  and proven before any hardware. **Toolchain (PlatformIO vs Arduino IDE) is
  [deferred]** to when we stand up the firmware skeleton.
- **Packet:** ~20–30 bytes binary — node ID, sequence number, battery voltage,
  sensor values, CRC, **and a firmware-version field in every packet**. Exact
  layout + CRC choice **[deferred]** (designed in the simulation-spine work).
- **Updates:** USB flashing at deploy/service time only. No OTA in V1.

---

## 9. Node types — summary

| Node type | Sensors |
|---|---|
| **Bed node** | 2–4× Watermark (6"/12", commercial + homemade pairs), 2× DS18B20 (6"/12"), 1× SHT45 at canopy in a shield |
| **Tunnel-air node** | 1× SHT45 at canopy in a shield (+ optional DS18B20 for a ground/ridge differential) |
| **Tank-level node** | 1× A02YYUW ultrasonic in the cluster lid |
| **Stratification rig** *(stretch)* | 1× SHT45 at canopy + DS18B20 at 2'/7'/ridge; portable mount |
| **Leaf-wetness node** *(stretch)* | Leaf-wetness grid; may co-locate with a bed node |

One LoRa node serves every sensor within wire reach: one radio, one battery, one
enclosure, one packet. Wire limits: Watermark 50+ ft on twisted pair; DS18B20
1-Wire 20–30 ft daisy-chainable; SHT45 I2C 3–4 ft (or an extender).

---

## 10. Build gotchas (carried from the field spec)

- Enclosures fail at the **cable glands**, not the box — PG7 down, silicone both
  sides, snug.
- **Hardwire through glands; no external connectors** — they wick moisture. Use
  the internal Wago block for serviceability.
- **Antenna placement over antenna gain** — vertical, external, above canopy.
- **Watermark install discipline** — soak-prime, slurry backfill, no air gaps.
- **Temp compensation is mandatory** — co-located DS18B20 at each depth.
- **±30 s wake jitter** to prevent collisions.
- **Configure DB downsampling early.**
- **Don't gold-plate; don't build OTA before there's firmware worth updating.**

---

## 11. Indicative cost

First real build — 3 nodes + gateway: **~$500** with new Watermarks, **~$320**
with homemade. Server software (Mosquitto, TimescaleDB, Grafana) is free. Full
BOM lives with the build phase, not here.

---

## 12. Deferred decisions register

Tracked so none are forgotten. Each is deferred until simulation or the bench
forces it; where a default keeps options open, it's noted.

| # | Decision | Why deferred / default that keeps it open |
|---|----------|-------------------------------------------|
| D1 | **kPa & VPD math: on-node vs gateway** | Default: packet carries **raw** resistance + raw T/RH, so the math can run either place and the equations stay re-revisable against stored raw data. Decide for real in simulation. |
| ~~D2~~ | **Binary packet layout + CRC choice** — RESOLVED 2026-06-16 → **see DEC-003** | Packet v1 designed in Phase 1.2 (#5): 14-byte LE header + manifest-driven channel values + CRC-16/CCITT-FALSE, pinned by shared golden vectors. Full contract in `contracts/packet-v1.md`. |
| D3 | **Gateway radio chip / node↔gateway PHY pairing** | SX1262 (node) vs SX127x/RFM95 (gateway). Behind an adapter, faked in sim; decided at the bench. *(Hardware-affects-software — evaluated and found safely deferrable.)* |
| D4 | **Gateway box** | Pi Zero 2 W vs Heltec LoRa→WiFi bridge vs radio-on-the-server. Central/on-LAN placement removes the antenna-reach pressure that would force this. |
| ~~D5~~ | **Firmware toolchain** — RESOLVED 2026-06-15 → **PlatformIO** | Picked while standing up the firmware skeleton (Phase 1.1, #4): mirrors tinkle's proven `native`+`esp32`+Unity setup, gives a one-command host-test tier, and was already on the box. See `firmware/platformio.ini`. |
| D6 | **Server stack** — esp. the time-series DB | Contested. **TimescaleDB** (source spec, for SQL JOINs to farm records) vs **VictoriaMetrics** (@architect review: single binary, retention-as-a-flag, lowest unattended-ops burden) vs InfluxDB (rejected — version-churn) vs SQLite (rejected — loses Grafana-native source + alerting). **Deciding question: do we genuinely need SQL JOINs between sensor data and farm records?** Yes → Timescale/Postgres; visual correlation enough → VM. VM's Pi-specific edges (SD wear, low RAM) shrink now that the DB runs on the headless box, not the gateway Pi. The likely JOIN counterparty is the owner's separate farm recording/analysis tool (daily log, harvest, diagnosis, labor) — if that lands Postgres-backed and the correlations are real, D6 tips to Timescale/Postgres. Mosquitto + Grafana also proposals. Resolve in simulation. |
| D7 | **Node-ID → location mapping** | Config file vs DB table. Designed with the gateway. |
| D8 | **TimescaleDB schema + JOIN shape to farm records** | Designed with ingestion. |
| D9 | **MQTT topic hierarchy** | Finalized with the gateway/ingestion. |
| D10 | **Grafana dashboard layout** | Designed with the dashboard work. |
| D11 | **Homemade gypsum-block recipe; AC excitation v2 circuit** | Bench-time hardware design; firmware models them behind the soil-moisture adapter. |

---

## 13. Rollout (hardware track — for orientation, not the build order)

The hardware deployment runs *behind* the software, which is built and simulated
first. Rough sequence once software is ready: bench proof-of-concept (one node +
gateway, watch a real reading hit the dashboard) → 2–3 nodes in Red Tunnel beds
before transplant, calibrated against the commercial Watermark reader → one bed
node per Red Tunnel tomato bed + a canopy-air node per tunnel → integration and
stretch goals. The software-first build order lives in `PROJECT_PLAN.md`.

---

*This spec is a guide. When it disagrees with what simulation or the bench
teaches us, the bench wins and the spec gets updated.*
