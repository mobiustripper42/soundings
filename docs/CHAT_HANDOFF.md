# Chat ↔ Claude Code Handoff

*The sync point between open-ended research in Claude chat and the repo that
Claude Code edits. Hand-maintained — paste questions out, paste answers back in.*

---

## Why this exists

Claude Code and Claude chat are good at different halves of a hardware build,
and neither can see the other's context.

| | Claude Code (this repo) | Claude chat |
|---|---|---|
| Sees | The repo, the docs, the code, the git history | Nothing from this repo unless pasted |
| Good at | Editing files, writing and running code, keeping SPEC/DECISIONS/PLAN coherent | Live web search, datasheets, current prices and availability, vendor links, open-ended design conversation |
| Bad at | Knowing what a part costs today or whether it's in stock | Remembering what the project already decided |

This file is the interchange. **It is the only channel** — anything that matters
and lives only in a chat window is lost.

---

## The ritual

1. **Out.** Copy the brief in §4 into a chat window. It carries enough project
   context that chat can answer without seeing the repo.
2. **Back.** Paste chat's answers into the ledger in §3 — fill `Answer`, set
   `Status` to `answered`, stamp `Date`. Raw and unedited is fine; don't tidy.
3. **Promote.** Tell Claude Code *"handoff updated"*. CC reads this file, moves
   each answer to where it actually belongs (§5), sets `Status` to `promoted`,
   and updates the affected docs in one commit.

Answers live here only in transit. **A `promoted` row's real home is elsewhere**
— this file is a log, not a source of truth.

> **Shortcut when CC is in a live session:** skip step 2 entirely — paste chat's
> answers straight into the conversation and CC writes the ledger *and* promotes
> in one pass. The full ritual above is for when the two halves are separated in
> time (research done today, promoted next week by a session with no memory of
> it). Step 1 is never optional: the brief in §4 is the only thing chat can see.

### Rules

- **Never let CC guess a part spec.** If a number isn't in the ledger as
  `answered` or `promoted`, it stays `[verify]` in the build plan. Cf. `CLAUDE.md`
  — don't write against a guess.
- **One ID per question.** Chat answers by ID so a partial reply still merges.
- **Cite the source.** A datasheet URL or vendor link in the `Answer` cell beats
  a bare number; it's what makes the answer re-checkable in a year.
- **Contradictions are findings.** If chat's answer conflicts with something in
  `SPEC.md` or `docs/tank-level-sensor.md`, say so in the answer rather than
  quietly overwriting. CC will surface it as a decision.

---

## 2. Status vocabulary

| Status | Meaning |
|---|---|
| `open` | Asked, no answer yet |
| `answered` | Answer pasted into the ledger, not yet moved into the real docs |
| `promoted` | Moved to its home doc by CC; the ledger row is now history |
| `blocked` | Needs a physical measurement or a purchase before it can be answered |

---

## 3. Question ledger

### Round 1 — tank node hardware (opened 2026-08-07, answered 2026-08-07)

Sourced from `docs/HARDWARE_BUILD_PLAN.md`. All eight block either an order or a
cut. Table cells carry the decision-relevant number and source; full rationale
and gotchas are in **§3.1 Round 1 detail** below, keyed by the same ID.

| ID | Question | Why it matters | Status | Answer | Date |
|----|----------|----------------|--------|--------|------|
| HW-01 | What antenna connector does the Heltec WiFi LoRa 32 V3 ship with (u.FL/IPEX vs SMA), and what 900 MHz antenna suits a fixed outdoor node? Does it need a pigtail to an SMA bulkhead? | Can't order antennas or the enclosure bulkhead without it | `promoted` | **U.FL/IPEX only on the PCB — no SMA.** Heltec: "reserved IPEX (U.FL) interface for LoRa use" ([heltec.org/project/wifi-lora-32-v3](https://heltec.org/project/wifi-lora-32-v3/)). What ships in the box varies by seller. **Yes, pigtail required:** short (100–150 mm) U.FL→SMA-female pigtail to a bulkhead through the enclosure wall, antenna direct on the bulkhead outside. Antenna: **915 MHz half-wave or 3–5 dBi collinear omni, SMA male**, vertical. Conversion parts as a bundle: [Rokland IPEX→SMA pigtail kit](https://store.rokland.com/products/915-mhz-antenna-ipex-sma-female-pigtail-conversion-package-for-heltec-wifi-lora-32v3). See §3.1. | 2026-08-07 |
| HW-02 | What deep-sleep current is actually achievable on a Heltec V3, and what has to be disabled to get there (OLED, Vext, LDO, onboard LED)? Real measured figures, not vendor claims. | `SPEC.md` §4 asserts 20–30 µA and a 2-year life. Unvalidated. | `promoted` | **~16 µA measured, PPK2, bare V3 board.** ([community.heltec.cn/t/heltec-lora-32-v3-deep-sleep-current/18332](http://community.heltec.cn/t/heltec-lora-32-v3-deep-sleep-current/18332) — same thread's 130 µA figure was attached sensors, not the board.) SPEC §4's 20–30 µA is **confirmed conservative**. Requires: SX1262 explicitly slept; all LoRa SPI + OLED pins set `INPUT`; OLED off; Vext off (GPIO36, active LOW); GPIO37/ADC_Ctrl off. LDO (CE6260B33M) EN is not broken out — its Iq is the floor. **⚠ Vext does not reach 0 V** — ~1.44 V residual with OLED present ([meshtastic/firmware#2591](https://github.com/meshtastic/firmware/issues/2591)); V3.1 schematic shows R12 10K pull to VDD_3V3. Do not power the sensor from Vext. See §3.1. | 2026-08-07 |
| HW-03 | What is the A02YYUW's beam angle / cone half-angle? | Sets the minimum standoff tube ID for a given length — and therefore the hole-saw size. Nothing gets cut until this lands. | `promoted` | **60° full cone → 30° half-angle.** DFRobot spec table, "Sensing Angle 60°" ([wiki.dfrobot.com/sen0311](https://wiki.dfrobot.com/sen0311/)). ⚠ Some resellers claim 15° (Robocraze) — contradicts the manufacturer; design to 60°. Clearance rule **`L_max ≈ 1.73 × r_inner`**: 3" Sch40 → 67 mm max; 4" Sch40 → 88 mm; 6" Sch40 → 133 mm. **Recommend 4" or 6" hole saw, standoff cut to 50–75 mm.** ⚠ Likely contradicts tank-level-sensor.md if it assumes a longer tube. See §3.1. | 2026-08-07 |
| HW-04 | How long is the A02YYUW's stock cable, and can its UART be extended to ~tank height (several metres) at 9600 baud? Shielding needed? | The enclosure must sit low and shaded while the sensor sits in the lid of the tallest tank | `promoted` | **Stock lead length is not published** by DFRobot — shipping list is sensor + PH2.0-4P connector only. Assume short (tens of cm); plan to extend. **Extension to several metres is fine.** 9600 8N1 = 104 µs/bit, nowhere near a timing limit. Precedent: A02YYUW + DS18B20 run on ~10 m UTP from an ESP32 on a rainwater reservoir lid ([forum.arduino.cc/t/1163633](https://forum.arduino.cc/t/wire-allocation-in-utp-cable-used-for-multiple-sensors/1163633)). **Shielding not required.** Outdoor/direct-burial Cat5e. **3 conductors suffice** — V+, GND, TX; RX is a mode-select strap, hardwire it at the sensor. 100 nF + 10 µF at the sensor end. See §3.1. | 2026-08-07 |
| HW-05 | What 2-cell parallel 18650 holder matches the Heltec V3's battery connector (type and polarity)? | Wrong connector is a soldering job or a returned part | `promoted` | **Connector: 2-pin 1.25 mm pitch.** Heltec calls it "SH1.25-2" and says to search "SH1.25 x 2" — ⚠ **the name is wrong**: real JST SH is 1.0 mm pitch. Order parts listed as **"JST 1.25 2-pin for Heltec/LilyGo."** **Polarity: verify with a meter, do not trust wire colours.** Rokland's own cable listing says to check the board before hooking up ([store.rokland.com](https://store.rokland.com/products/battery-connector-cables-battery-wires-jst-1-25-5pcs-for-lilygo-and-heltec)). Method: USB in, **no battery**, probe each JP1 pin to a GND header pin — the one near 4.2 V is +. **Holder: any 2-slot 18650, but confirm PARALLEL not series** — most 2×18650 lead holders are series (7.4 V) and will destroy the board. Match cells and equalise SoC before joining. See §3.1. | 2026-08-07 |
| HW-06 | Does the Heltec V3's onboard battery circuitry include adequate over-discharge protection, or is a separate LVC board required? | `SPEC.md` §4 lists a ~$3 LVC board; may be redundant | `promoted` | **No over-discharge protection. But the LVC board is still arguably redundant.** Charge IC is a **TP4054** — linear charger only (CHRG/GND/BAT/VCC/PROG), no discharge-side FET, no LVC ([V3.1 schematic](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.1_Schematic_Diagram.pdf)). ⚠ **Vendor claim conflict:** Heltec advertises "charge and discharge management"; the schematic supports charge management only. **Recommendation: protected 18650 cells (~$2/cell premium) + firmware cutoff at 3.2 V/cell. Drop the $3 LVC board.** At 16 µA a stuck-*asleep* node has decades of margin; the real risk is stuck-*awake*, which firmware catches and a hardware LVC does not catch early. ⚠ Contradicts `SPEC.md` §4 BOM — surface as a `DEC-nnn`. See §3.1. | 2026-08-07 |
| HW-07 | What's a reasonable option for measuring µA-range sleep current on a hobby budget? | Without it, HW-02 can't be validated and the 2-year claim stays a guess | `promoted` | **Not required this build — descoped.** The tool would be a Nordic PPK2 (~$90–110, 200 nA–1 A, 100 kS/s) and it is the right instrument if it is ever needed. **It is not needed here.** Validation moves to telemetry: the V3 already has battery sense on a switched 390K/100K divider — **report VBAT in every packet, threshold-alert at 3.4 V/cell on the server, replace cells on alert.** An empirical discharge curve from the deployed node in the real thermal environment is better evidence than a bench measurement, and the maintenance cost of a battery swap is ~5 minutes at a tank Eric already visits weekly in season. ⚠ **This makes VBAT-in-packet load-bearing** — it moves from nice-to-have to a required field in the packet schema. Revisit the PPK2 only if Soundings grows to 3+ distinct node designs. | 2026-08-07 |
| HW-08 | What is the A02YYUW's power-up settling time, and what's the right read strategy (how many samples, median vs mean)? Does it free-run when powered? | Sets `t_active`, which lands directly in the battery-life equation | `promoted` | **Yes, it free-runs** — apply power and it streams 4-byte frames (`0xFF`, DATA_H, DATA_L, SUM) at 9600 8N1, no trigger ([protocol ref](https://wiki.dfrobot.com/sen0311/docs/21651)). **No separate power-on settling spec is published**; response time is the only figure: **100 ms** with RX pulled low (real-time), 100–300 ms with RX floating/high (internally filtered). **Read strategy: RX low, ~100 ms boot delay, discard first 2–3 frames, collect 5–7 checksum-valid frames, take the MEDIAN.** Median not mean — failure modes are outliers (dropped echo → max range; multipath → short). **`t_sensor` ≈ 800 ms rail-on ≈ 6.4 mA·s.** ⚠ Sensor is **not** the dominant term — MCU awake alongside it costs ~5×. Optimise total awake time, not sensor-on time. Switch the rail with a P-FET load switch from VDD_3V3 (**not** Vext, see HW-02), GPIO held via `rtc_gpio_hold_en()`. See §3.1. | 2026-08-07 |
| HW-09 | *(from the §3 note)* `docs/tank-level-sensor.md:45-49` states a ~20–25 cm dead zone. Is that right? | If wrong in the optimistic direction, the dead-zone clamp shrinks and usable range at the top of the tank grows | `promoted` | **⚠ CONTRADICTION — the doc is wrong. Real blind zone is 3 cm.** DFRobot spec table: blind zone 3 cm, range 3–450 cm ([wiki.dfrobot.com/sen0311](https://wiki.dfrobot.com/sen0311/)). The 20 cm figure is the **JSN-SR04T**; DFRobot's own marketing draws exactly that contrast ([dfrobot.com/product-1935](https://www.dfrobot.com/product-1935.html)). **Net gain: ~17–22 cm of usable range at the top of the tank.** Update the dead-zone clamp in `docs/tank-level-sensor.md` and re-derive full-tank percentage. | 2026-08-07 |
| HW-10 | Do we need a temperature sensor on this node — headspace air, tank water, or both? | Not previously in the ledger. Speed of sound is temperature-dependent; if uncorrected it is the dominant error term | `promoted` | **Both. Two DS18B20 on one 1-Wire bus, one GPIO, ~$5 for the second.** **(a) Headspace air — REQUIRED, not optional.** Speed of sound moves ~0.17 %/°C. Over a ~2 m headspace, 0 °C→40 °C is **≈14 cm of apparent level change with no water moving** — an order of magnitude worse than the ±1 cm accuracy spec and worse than the HW-09 blind-zone gain. ⚠ **Vendor claim conflict:** DFRobot's spec table lists **no** temperature compensation, but Robocraze claims the sensor has it. DFRobot's own FAQ and ESP32 guide both say to implement compensation yourself. **Assume none; correct in firmware.** **(b) Water, mid-depth — OPPORTUNISTIC.** No agronomic action available (can't heat 2,200 gal) but three real uses: early-season irrigation *timing* (shift April/May runs to afternoon rather than drenching PEX-warmed soil with 45 °F water); summer biofilm/clog risk scheduling for header + tape flushes; freeze alerting for pump/Dosatron/header. Mount **mid-depth** — tanks stratify, surface reads air and bottom reads the coldest slug. | 2026-08-07 |

**Round 1 note — the original "also worth raising" item is now HW-09** and is a
confirmed contradiction, not a sanity check. **HW-10 is new**, raised by chat
rather than by the repo, and is the larger of the two accuracy findings.

---

## 3.1 Round 1 detail

Longer rationale and gotchas. Table cells above carry the decision; this carries
the reasoning CC needs to write the docs correctly.

### HW-01 — antenna

- Keep the internal pigtail **as short as it ships**. U.FL pigtails are 1.13 mm
  or RG-178 — roughly 1–1.5 dB/m at 900 MHz. The 20 ft extension cables sold for
  Meshtastic throw away more than the antenna gain provides.
- **Skip 8–10 dBi fiberglass sticks.** The gain comes from flattening the vertical
  pattern; if gateway and tank sit at different heights you shoot over or under.
  3–5 dBi is the right call for a farm-scale hop.
- U.FL is rated ~30 mating cycles and the latch is fragile — **RTV dab + zip-tie
  strain relief** once seated. Assume it gets mated twice, ever.
- SMA bulkheads are **not weatherproof by default**; the outside joint needs
  self-amalgamating tape.
- ⚠ **Gateway node:** same board, but mains/USB powered. HW-01 applies to it;
  HW-02/05/06/07 power constraints do **not**. Don't let the BOM merge them.

### HW-02 — deep sleep

Disable list, ordered by how often it's missed:

1. **SX1262 explicitly to sleep** before `esp_deep_sleep_start()`. The radio does
   not sleep because the MCU does.
2. **All LoRa SPI + OLED pins to `INPUT`** — NSS, MISO, MOSI, SCK, RST, BUSY,
   SDA_OLED, SCL_OLED, RST_OLED. This is what got the forum measurement to 16 µA.
3. **OLED off** — both `displayOff()` and Vext.
4. **GPIO37 / ADC_Ctrl off.** It gates the 390K/100K battery-sense divider
   (2N7002 on the V3.1 schematic). Left on, it bleeds through 490K continuously.
   ⚠ But see HW-07 — this divider is now the validation mechanism, so it must be
   switched on briefly each wake, then off again.
5. `gpio_deep_sleep_hold_en()` / `rtc_gpio_hold_en()` on anything that must hold
   state, or pins float on the way into sleep.

CP2102 runs off VBUS — costs nothing on battery. **Sleep is not the problem:**
20 µA × 17,520 h = **350 mAh over two years**. Wake energy dominates.

### HW-03 — beam angle and the standoff

"Short standoff tube" is **shorter than the phrase implies**. Prefer a **shallow
wide hood** — 4" or 6" pipe cut to 50–75 mm, sensor face flush with the top of
the collar, not recessed deep.

⚠ **Second-order finding, larger than the tube question:** the 60° cone keeps
expanding inside the tank. At 2 m below the lid the footprint is **~2.3 m across**
— wider than an 1100-gal vertical cylinder. Expect **sidewall echoes at low
levels** and a practical floor on how empty the node can read. Mount dead centre,
keep the cone off the fill inlet stream, and note internal ribs or a centre draw
pipe as candidate false targets. This belongs in `docs/tank-level-sensor.md`
alongside the range math, and in `HARDWARE_BUILD_PLAN.md` §5 mounting.

### HW-04 — cable run

- V+ with its own GND in one twisted pair; TX with a second GND in another.
  Parallel spare conductors onto V+/GND — irrelevant at 8 mA but free.
- If shielded cable is used anyway, **ground the shield at the node end only**.
  Both ends in a wet outdoor run invites a ground loop.
- **Keep out of the same conduit as the Grundfos pump wiring.** Bigger noise
  source than the cable length by a wide margin.
- Beyond ~10 m, or on persistent checksum failures: move the ESP32 to the lid and
  run RS-485, or split into a second node.

### HW-05 — cells

- `F1` on the schematic is a **6 V / 500 mA fuse** on the battery path. LoRa TX at
  ~120 mA peak is comfortably under it, but it caps what the board can ever source.
- Paralleling caveat: **only parallel cells of matched capacity and internal
  resistance, and charge both to the same voltage before joining.** Two cells at
  different SoC dump unlimited equalising current into each other.

### HW-06 — protection

Practically: the LDO keeps conducting as cells sag, and 16 µA on 6000 mAh is
decades. The failure mode worth defending against is a **firmware bug that leaves
the node awake at ~40 mA**, and a 3.2 V/cell firmware cutoff catches that earlier
and more gracefully than a hardware LVC would. Protected cells are the backstop
for everything else.

### HW-08 — read strategy

Reject before medianing: checksum failures, `0x0000`, and anything outside known
tank geometry.

**Budget:** ~800 ms rail-on at 8 mA ≈ 6.4 mA·s ≈ 0.0018 mAh/wake. The ESP32-S3
awake beside it at ~40 mA burns ~32 mA·s over the same window — **5× the sensor.**
Do not contort the read strategy to save sensor milliseconds.

⚠ **Open risk:** spec says 3.3–5 V, but several reseller pages recommend 5 V "for
best performance." On a 3.3 V rail this could be flaky. **Bench-test at 3.3 V
early.** If unreliable, a boost converter is required and the power budget changes
materially. Track as a `[verify]` until bench-confirmed.

### Power budget as it now stands (15-minute interval)

| Term | Over 2 years |
|---|---|
| Sleep @ 20 µA | 350 mAh |
| Wakes @ ~2 s, ~45 mA avg (96/day) | ~1,750 mAh |
| Self-discharge ~3 %/yr | ~360 mAh |
| **Total** | **~2,460 mAh** |

Against 6000 mAh nominal — but **derate to ~4,500 mAh effective for cold**
(Li-ion delivers ~70 % of rated near −10 °C; no charging, so no plating risk,
just capacity). **~1.8× margin.**

At a **10-minute** interval this drops to ~1.3×, which is thin. **Recommend
15 minutes.** Tank level changes on rain events and irrigation runs; it does not
need 10-minute resolution.

All of the margin lives in the assumption `t_active ≈ 2 s`. Per HW-07 that is now
validated empirically from the VBAT trace rather than on the bench.

---

## 3.2 Cross-cutting findings for CC

Not tied to a single ID. Each needs a home.

| # | Finding | Affects |
|---|---|---|
| F-1 | **Temperature compensation is the dominant error term** (~14 cm over a 2 m headspace across the seasonal range), larger than the blind-zone correction it would otherwise be masked by. Requires a headspace DS18B20 and a firmware correction. | `docs/tank-level-sensor.md` (math), `HARDWARE_BUILD_PLAN.md` §4 BOM |
| F-2 | **Dead-zone clamp is wrong by ~20 cm in the pessimistic direction.** Usable top-of-tank range grows. | `docs/tank-level-sensor.md:45-49` |
| F-3 | **60° cone illuminates tank sidewalls before reaching the bottom** in an 1100-gal vertical. Practical floor on low-level readings; sidewall/rib/draw-pipe false targets. | `docs/tank-level-sensor.md`, `HARDWARE_BUILD_PLAN.md` §5 |
| F-4 | **`SPEC.md` §4 LVC board ($3) is redundant** given protected cells + firmware cutoff. Rejected alternative exists → warrants a `DEC-nnn`. | `SPEC.md` §4, `docs/DECISIONS.md` |
| F-5 | **VBAT-in-packet is now load-bearing**, not optional telemetry — it is the sole validation path for the 2-year claim after HW-07 was descoped. Server-side threshold alert at 3.4 V/cell is part of the design, not an afterthought. | `SPEC.md` §4, packet schema, server side |
| F-6 | **Vext is not a usable switched rail** (~1.44 V residual). Any doc or code that assumes Vext gates the sensor is wrong; a discrete P-FET load switch is required. | `HARDWARE_BUILD_PLAN.md` §4, firmware |
| F-7 | **Gateway node shares the board but not the constraints.** Mains/USB powered — no sleep budget, no battery BOM, no LVC. Ensure the BOM does not inherit node-only line items. | `HARDWARE_BUILD_PLAN.md` §4 |
| F-8 | **A02YYUW at 3.3 V is unverified.** Reseller guidance recommends 5 V. If it proves flaky, a boost converter enters the BOM and the power budget is re-derived. | `[verify]` in `HARDWARE_BUILD_PLAN.md` §6 |

---

## 3.3 CC review of Round 1 (2026-08-08)

Written at promote time. Chat can't see the repo, so several answers land slightly
off against decisions already made here. Recorded so the next round's brief can
carry the missing context.

### Math re-derived independently — all of it holds

| Claim | Chat | CC re-derivation | Verdict |
|---|---|---|---|
| Speed-of-sound error, 2 m headspace, 0–40 °C | ~14 cm | **14.1 cm** (`c = 331.3 + 0.606·T`, ±7.06 cm about a 20 °C reference) | ✅ |
| Per-degree sensitivity | 0.17 %/°C | **0.176 %/°C** | ✅ (chat slightly conservative) |
| Standoff clearance `L_max ≈ 1.73·r` | 67 / 88 / 133 mm | **67 / 89 / 133 mm** for 3" / 4" / 6" Sch40 | ✅ |
| 60° cone reaches the sidewall of an 1100-gal vertical | "~2.3 m across at 2 m" | **Wall contact at ~1.24–1.41 m below the sensor**, tank dia 1.43–1.63 m depending on assumed height | ✅ and it bites sooner than stated |

### Corrections

- **C-1 — F-5 needs no packet change; `battery_mv` is already in the header.**
  Chat calls VBAT-in-packet "load-bearing… moves from nice-to-have to a required
  field in the packet schema." It has been a **fixed header field since packet v1**
  — `contracts/packet-v1.md:45`, u16 at offset 6, present in every packet, with
  `0xFFFF` reserved for unknown. No contract change, no `proto_ver` bump, no new
  channel bit. The only new work is the **server-side threshold alert**, which is
  Phase 5.2. F-5 is satisfied by the existing design.

- **C-2 — temperature correction runs gateway-side, not in firmware.** HW-10 and
  F-1 both say "correct in firmware." That contradicts **DEC-004** (all derivation
  gateway-side) and **DEC-003** (raw values on the wire — `contracts/packet-v1.md:17`).
  The node transmits **raw distance and raw DS18B20 counts**; the gateway applies
  the correction. This is not pedantry: it is what lets the correction be refined
  later (humidity has a smaller but real effect on the speed of sound) and
  **re-derived against years of stored raw without touching a node in a tank lid**.
  Chat could not have known this — the brief didn't carry it.

- **C-3 — chat's HW-07 descope is right, for the wrong reason, and it discards a
  cheap check along with the expensive one.** The argument given is that a
  telemetry discharge curve beats a bench measurement. It doesn't: a VBAT trace
  takes *months* to reveal a rate, and by then the node is built, sealed, and up a
  tank. What actually justifies skipping the PPK2 is narrower — **the failure mode
  worth catching is milliamp-scale, not microamp-scale.** "I forgot to sleep the
  SX1262" or "the OLED is still on" costs 1–40 mA, which **any multimeter in series
  reads perfectly well.** A PPK2 only buys resolution *within* the µA band, and the
  1.8× margin says there is nothing to optimise there. So: **skip the PPK2, but do
  the multimeter check** — it is the difference between a design error found in an
  afternoon and one found in month three. Chat's reasoning would have skipped both.

- **C-4 — HW-10(b) water temp is not "+$5."** It is $5 plus a submerged
  stainless-sheathed probe, a suspension method at mid-depth, a wet penetration or
  a shared one on the first hardware build the farm has ever deployed, and a
  fouling/service story for a sensor hanging in irrigation water. The *uses* chat
  lists are real (freeze alerting for the pump and Dosatron especially). The cost
  framing is not. **Parked** to `docs/FUTURE_IDEAS.md` as **FI-1**.
  **Owner's decision 2026-08-08:** parked on **physical placement difficulty and
  added complexity**, not on scope timing — which is the sturdier objection, since
  it doesn't decay as the project matures. Headspace air is a different matter: it
  is not new scope, it is an accuracy fix to a measurement already committed to.
  **Taken.**

- **C-5 — pin assignments are bench-confirm, not gospel.** GPIO36 (Vext, active
  LOW) and GPIO37 (ADC_Ctrl) are cited from a V3.1 schematic. Board revisions move
  pins. Cheap to verify with a meter at build step 5; carried as such rather than
  written into firmware on faith (`CLAUDE.md` — don't write against a guess).

### Ranking, for what it's worth

F-1 (temperature) is the most valuable thing in this round by a wide margin — it
is a **14 cm** error on a sensor specified to ±1 cm, and it was invisible to the
repo. F-3 (sidewall echoes) is second and interlocks neatly with HW-08's
median-of-5–7 strategy, which is already the right defence against it. F-2
(blind zone) is a real correction but smaller than it looks — see the standoff
geometry note now in `docs/tank-level-sensor.md`.

### Carried into Round 2

Open items that came *out* of this round rather than being closed by it: F-8
(3.3 V operation), C-5 (pin confirmation), the DS18B20 waterproof-probe form
factor, and the P-FET load-switch part choice. See §3.4.

---

## 3.4 Round 2 — open

| ID | Question | Why it matters | Status | Answer | Date |
|----|----------|----------------|--------|--------|------|
| HW-11 | What P-FET load switch (or integrated load-switch IC) suits gating a ~8 mA, 3.3 V sensor rail from an ESP32 GPIO — part number, and does it need a pull-up plus a gate resistor? | F-6 killed Vext as the switched rail; this replaces it. Blocks the BOM. | `open` | | |
| HW-12 | What DS18B20 form factor for tank headspace air — bare TO-92, or a waterproof stainless probe? The headspace is condensing but the sensor is not submerged. | Condensation on a bare sensor is a corrosion and accuracy risk | `open` | | |
| HW-13 | Does the A02YYUW actually work reliably on a 3.3 V rail? (F-8 — resellers recommend 5 V.) If not, what boost converter, and what does it cost in quiescent draw? | If it needs 5 V the power budget is re-derived and a converter enters the BOM | `blocked` | Bench test at build step 5 | |
| HW-14 | Confirm GPIO36 = Vext (active LOW) and GPIO37 = ADC_Ctrl on the actual board revision received. (C-5) | Firmware correctness; cheap to check with a meter | `blocked` | Verify on receipt | |
| HW-15 | **Heltec Wireless Stick Lite V3** — does it share the WiFi LoRa 32 V3's pinout for the load-bearing pins (Vext control, ADC_Ctrl + battery-sense divider, SX1262 SPI)? Same U.FL connector? Same JST 1.25 battery connector? Same USB-serial bridge chip? | **Both** boards are now Stick Lites (§3). If the pinout differs, HW-02's disable list and HW-05/HW-14's answers **do not transfer** and Round 1 gets partly re-run. | `open` | | |
| HW-18 | Does the Stick Lite V3 have a **Vext rail at all**, and if so does it still show the ~1.44 V residual with no OLED to pull it up? | If Vext works cleanly here, **HW-11's P-FET load switch may be unnecessary** — one BOM line and a chunk of firmware removed | `open` | | |
| HW-16 | Stick Lite V3 — same **TP4054** charge IC, same absence of discharge-side protection? | **DEC-006 rests on the TP4054.** If this board has real discharge protection, the protected-cells-plus-firmware-cutoff decision needs revisiting. | `open` | | |
| HW-17 | Stick Lite V3 — achievable deep-sleep current. Is it **below** the V3's cited 16 µA, given there is no OLED to disable and no OLED pull-up on Vext? | Would improve the power budget and remove three of the five items from HW-02's disable list | `open` | | |

---

## 4. Brief to paste into chat — **Round 2**

> Copy everything between the rules. It's self-contained — chat sees nothing else.
> Round 1's brief is superseded; its questions are preserved verbatim in the §3
> ledger.

---

I'm building a battery-powered wireless sensor node for a farm rain-catchment
tank. A previous research round settled most of the sensor and cabling questions;
this round is about the **board**, which has since changed, plus three loose ends.
Please answer by ID, cite datasheets, schematics, or vendor pages where you can,
and flag anywhere the common wisdom disagrees with the vendor's claims.

**The build.** One field node measuring water level in a cluster of three
plumbed-together rain tanks. It reports over LoRa to a second identical board
acting as a gateway radio, tethered by USB to a Linux server on the farm LAN.
Read-only telemetry — it measures and reports, never actuates.

- **Board: Heltec Wireless Stick Lite V3** (ESP32-S3 + SX1262), ×2 — one node, one
  gateway radio. **This changed from the WiFi LoRa 32 V3**, which is what the
  previous round researched. No OLED wanted: the node is sealed outdoors, and the
  range test is run from a phone app.
- **Sensor: A02YYUW ultrasonic** (UART, free-running, 9600 8N1), in the tank lid
  pointing down, in a shallow wide PVC collar.
- **Plus a DS18B20** in the tank headspace — required, because the speed of sound
  moves 0.176 %/°C and that's ~14 cm of apparent level error across 0–40 °C over a
  2 m headspace.
- **Power:** 2× protected 18650 in parallel (~6000 mAh), no solar, deep sleep,
  waking every 15 minutes. Target ~2 years between battery swaps.
- **Enclosure:** IP65, light-coloured, mounted low and shaded, PG7 glands down.

**Already settled — please don't re-derive these:** the A02YYUW's 60° beam cone,
3 cm blind zone, 100 ms response and median-of-5–7 read strategy; U.FL→SMA pigtail
plus a 3–5 dBi 915 MHz omni; 3-conductor Cat5e for the sensor run, unshielded;
protected cells plus a 3.2 V/cell firmware cutoff instead of a hardware LVC board;
15-minute cadence; all unit conversion done on the server, not on the node.

**Questions — the first four are the important ones, and they may make the fifth
unnecessary, so please take them in order.**

- **HW-15** — Does the **Wireless Stick Lite V3** share the **WiFi LoRa 32 V3's**
  pinout for the pins that matter: Vext control, the ADC_Ctrl gate and battery-sense
  divider, and the SX1262 SPI lines? Same U.FL antenna connector? Same JST
  1.25 mm battery connector and polarity convention? Same USB-serial bridge chip?
  I researched all of that against the WiFi LoRa 32 V3 and need to know what
  transfers.
- **HW-16** — Does the Stick Lite V3 use the same **TP4054** charge IC, with charge
  management only and **no discharge-side protection**? I dropped a hardware
  low-voltage-cutoff board specifically because the WiFi LoRa 32 V3 has no
  discharge FET and protected cells plus a firmware cutoff cover it better. If this
  board is different, that decision needs revisiting.
- **HW-17** — What **deep-sleep current** is realistically achievable on a Stick
  Lite V3? Real measured figures if they exist. On the WiFi LoRa 32 V3 the cited
  figure is ~16 µA, but three of the five things you had to disable to get there
  were OLED-related. With no OLED, is it lower, and is the disable list shorter?
- **HW-18** — Does the Stick Lite V3 **have a Vext switchable rail at all**? On the
  WiFi LoRa 32 V3, Vext never reaches 0 V — about 1.44 V residual, traced to a 10K
  pull-up associated with the OLED. **If that pull-up is gone on this board, does
  Vext switch cleanly to 0 V?** I need to power the ultrasonic sensor only during
  the wake window, and if Vext works I can drop the discrete load switch below.
- **HW-11** — *(possibly moot — depends on HW-18.)* If Vext still can't be used:
  what **P-FET load switch**, or integrated load-switch IC, suits gating a ~8 mA
  3.3 V sensor rail from an ESP32 GPIO? Part number, and does it need a gate
  resistor and a pull-up?
- **HW-12** — What **DS18B20 form factor** for tank headspace *air* — bare TO-92 or
  a waterproof stainless probe? The headspace is condensing but the sensor is not
  submerged, and it needs to last years in there.
- **HW-13** — The A02YYUW is spec'd 3.3–5 V but several resellers recommend 5 V "for
  best performance." I'll bench-test at 3.3 V, but **if it proves flaky, what boost
  converter would you suggest**, and what would its quiescent draw cost a node
  waking every 15 minutes?

---

## 5. Where answers go when promoted

CC moves each `answered` row to its real home and marks it `promoted`.

| Answer type | Home |
|---|---|
| A part choice, quantity, or spec | `docs/HARDWARE_BUILD_PLAN.md` §4 BOM — the `[verify]` tag comes off |
| A dimension or physical constraint | `docs/HARDWARE_BUILD_PLAN.md` §5 mounting |
| A current, timing, or battery figure | `docs/HARDWARE_BUILD_PLAN.md` §6, and `SPEC.md` §4 if it contradicts the estimate there |
| A sensor behaviour that changes the math | `docs/tank-level-sensor.md` |
| A choice with a real tradeoff and a rejected alternative | A new `DEC-nnn` in `docs/DECISIONS.md` |
| Something that resolves a deferred decision | The `SPEC.md` §12 register, struck through with a pointer |

**Contradictions get surfaced, never silently applied.** If an answer conflicts
with an existing `[settled]` item, CC raises it rather than editing over it.

---

## 6. Going the other way — CC to chat

Occasionally chat needs repo state to answer well. Paste it in directly; there's
no ceremony. The three that carry the most context per line:

- `docs/tank-level-sensor.md` — the whole sensor design, ~120 lines
- `docs/HARDWARE_BUILD_PLAN.md` §4 and §9 — the BOM and what's still open
- `SPEC.md` §4 — the node hardware spec and the power claim

---

## 7. Round log

| Round | Opened | Topic | Closed |
|-------|--------|-------|--------|
| 1 | 2026-08-07 | Tank node hardware — HW-01…HW-08, plus HW-09 (blind zone) and HW-10 (temperature) raised in chat | **2026-08-08 — all 10 `promoted`.** CC review in §3.3; math re-derived and confirmed; five corrections logged. Yielded DEC-006, DEC-007, and four new open items. |
| 2 | 2026-08-08 | **Board change to Stick Lite V3** (HW-15…HW-18) + loose ends (HW-11…HW-13). HW-14 is a meter check on receipt, not a chat question. Brief in §4. | — |
