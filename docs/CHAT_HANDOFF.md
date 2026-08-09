# Chat ↔ Claude Code Handoff

*The sync point between open-ended research in Claude chat and the repo that
Claude Code edits. Hand-maintained — the whole file goes out, gets edited, comes
back.*

---

## 0. If you are Claude chat and this file was just handed to you

**Your job: answer the `open` questions in §3.4 and edit this file in place.** Then
hand the whole file back. Everything you need is in it.

**Edit freely:**

- The `Answer`, `Status`, and `Date` cells of **Round 2** rows (§3.4). Set
  `Status` to `answered`.
- A new `## 3.5 Round 2 detail` section, mirroring §3.1 — the tables hold the
  decision, that section holds the reasoning, gotchas, and anything that didn't
  fit in a cell.
- The cross-cutting findings table (§3.2) — append new `F-n` rows for anything
  that doesn't belong to a single ID.
- **New question IDs.** Add them. Round 1's two most valuable findings — HW-09
  (the blind zone was wrong by 20 cm) and HW-10 (temperature compensation, a
  14 cm error nobody had noticed) — were both raised by chat going off-script,
  not asked by the repo. If the research surfaces something that should have been
  asked, ask it and answer it.

**Do not edit:**

- **Round 1 rows (§3).** They are `promoted` — a historical record of what was
  decided and why. Tidying them destroys the audit trail.
- **§3.3.** That is CC's review of your previous round, including corrections.
  Disagree with it in your Round 2 answers if you think it's wrong — don't
  rewrite it.
- §0–§2, §5, §6 — the workflow itself.

**Conventions that make the merge work:**

- **Answer by ID.** A partial reply still merges cleanly.
- **Cite sources** — a datasheet, schematic, or vendor URL, not a bare number.
  It's what makes an answer re-checkable in a year.
- **Flag contradictions, don't resolve them.** If an answer conflicts with
  something recorded here as settled, say so explicitly in the answer. CC surfaces
  it as a decision rather than silently applying it.
- **Say when you don't know.** An honest "not published, here's the closest
  proxy" is worth more than a confident number that turns out to be a different
  part. Round 1 did this well on the A02YYUW cable length.
- Mark a row `blocked` if it genuinely needs a bench measurement or a purchase
  before anyone can answer it.

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

### This already happened once, on tinkle

> **Owner, 2026-08-08:** *"shit got out of sync between when i was building tinkle
> (irrigation controller) because chat was specing parts that didn't make it back
> into the repo."*

That is the failure this file exists to prevent, and it is not hypothetical. The
mechanism is worth naming precisely, because it is quiet: **chat specs a part, the
part gets ordered, the repo never hears about it.** Nothing breaks. No test fails.
The repo simply describes a machine that no longer exists, and every later
decision is made against a stale picture — until someone opens an enclosure and
finds a component the docs have never heard of.

Two rules fall out of it, and the first one is the load-bearing one:

- **🔒 The BOM is the only purchasing authority.** If a part is not in
  `docs/HARDWARE_BUILD_PLAN.md` §4, it does not get ordered. Not "should not" —
  *does not*. A chat message is a **proposal**, never an order list. This inverts
  the tinkle failure: instead of hoping research finds its way back into the repo,
  the repo becomes the thing you buy from, so promotion has to happen first or
  nothing ships. **If you catch yourself about to order from a chat window, stop
  and promote it first.**
- **An open round is a drift alarm.** §7's round log closes a round only when
  every row is `promoted`. A round that has been open a while means research is
  sitting somewhere unpromoted — which is exactly the tinkle state. Check the log
  before ordering anything.

Neither rule survives being merely written down; they work because ordering runs
through the BOM. That is the whole design.

**Worth backporting.** Nothing in this file is soundings-specific, and tinkle
demonstrably needed it. Once the pattern has a couple more rounds proving itself
here, it belongs in the seeds templates so every project gets it — by whatever
mechanism seeds takes now.

---

## The ritual

**Hand chat the whole file.** Not an extract — the whole thing. §0 tells chat what
to do with it, and the surrounding context (Round 1's answers, CC's review of
them, the promotion rules) is what stops it re-deriving settled ground or
contradicting decisions it can't see.

1. **Out.** Give this entire file to a chat window.
2. **Research + edit.** Chat answers the `open` rows in place, per §0.
3. **Back.** Hand the edited file to Claude Code — paste it, or drop it in and say
   *"handoff updated."*
4. **Promote.** CC **diffs the returned file against the committed version**
   first — that catches anything accidentally clobbered, and separates chat's real
   edits from incidental reformatting. Then it moves each answer to its real home
   (§5), sets `Status` to `promoted`, and commits.

Answers live here only in transit. **A `promoted` row's real home is elsewhere**
— this file is a log, not a source of truth.

> **§4 is for the other path.** If you ever want a lighter round, §4 is a
> self-contained brief that works without the rest of the file. When you hand over
> the whole document it's redundant but harmless — it states the round's ask.

> **Shortcut when CC is in a live session:** answers can come straight into the
> conversation instead of via the file, and CC writes the ledger *and* promotes in
> one pass. The file round-trip matters when research and promotion are separated
> in time — done today, promoted next week by a session with no memory of it.

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
| F-9 | **The Stick Lite V3's advertised "≤800 µA deep sleep" is a stale V2-era figure.** Heltec's own `HTIT-WS_V3` datasheet carries a V2-vs-V3 comparison table listing deep sleep as **800 µA (V2, ESP32-D0/SX1276/Micro-USB) vs <10 µA (V3, ESP32-S3/SX1262/Type-C)**. The hardware update log attributes the 800 µA line to the 2019 V2 revision. The product page never updated it. **Do not budget against 800 µA.** | `SPEC.md` §4, `HARDWARE_BUILD_PLAN.md` §6 |
| F-10 | **Forum "high sleep current" reports on this board are almost all LIGHT sleep, not deep sleep.** The one WSL V3 thread with an actual PPK2 trace states deep sleep base is "a few µA" and "close to zero"; its 7.5 mA and 2.0 mA figures are light-sleep baselines. Anyone quoting mA figures for this board should be checked for which sleep mode they measured. | `SPEC.md` §4 |
| F-11 | **⚠ The Stick Lite V3 has TWO U.FL/IPEX sockets** — E2 (LoRa, behind the UPG2179 RF switch) and E3 (2.4 GHz Wi-Fi/BT, alongside the E1 spring antenna). The WiFi LoRa 32 V3 has only one. **Plugging the 915 MHz antenna into the 2.4 GHz socket transmits into an unmatched load.** Physically label the correct socket at build time; this is a silent, hard-to-diagnose failure. | `HARDWARE_BUILD_PLAN.md` §5, build checklist |
| F-12 | **Board footprint and packing list both changed.** 58.08 × 22.6 × 8.2 mm (vs 50.2 × 25.5 × 10.2) — longer, narrower, thinner. Headers are **2×20** (vs 2×18). Packing list is board + LoRa antenna + SH1.25×2 connector + pin-map sticker — **no header pins included**, unlike the WiFi LoRa 32. Order headers separately if anything gets breadboarded. | `HARDWARE_BUILD_PLAN.md` §4 BOM, §5 enclosure sizing |
| F-13 | **Vext outputs 3.3 V, so it cannot solve HW-13.** If the A02YYUW proves unreliable at 3.3 V, Vext is not a route to 5 V. A zero-cost intermediate exists: gate the sensor from **VBAT (3.4–4.2 V)** with a discrete switch rather than the 3.3 V rail — more headroom, no new part, no boost. Only viable if HW-11's switch is built; Vext cannot do it. | `HARDWARE_BUILD_PLAN.md` §4, `docs/DECISIONS.md` |

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

## 3.4 Round 2 — promoted 2026-08-08

**Headline: the board change is cheaper than feared.** The Stick Lite V3 is the
same design with the OLED block deleted — the schematic still carries an orphan
`OLED Display` section label with nothing under it. Every load-bearing part is
identical. Two Round 1 answers get *better*, one gets a correction, and three new
IDs came out of the research.

| ID | Question | Why it matters | Status | Answer | Date |
|----|----------|----------------|--------|--------|------|
| HW-15 | **Heltec Wireless Stick Lite V3** — does it share the WiFi LoRa 32 V3's pinout for the load-bearing pins (Vext control, ADC_Ctrl + battery-sense divider, SX1262 SPI)? Same U.FL connector? Same JST 1.25 battery connector? Same USB-serial bridge chip? | **Both** boards are now Stick Lites (§3). If the pinout differs, HW-02's disable list and HW-05/HW-14's answers **do not transfer** and Round 1 gets partly re-run. | `promoted` | **Effectively all of it transfers.** Compared the [WSL V3 schematic](https://resource.heltec.cn/download/Wireless_Stick_Lite_V3/HTIT-WSL_V3_Schematic_Diagram.pdf) net-by-net against the [WiFi LoRa 32 V3.1 schematic](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.1_Schematic_Diagram.pdf). **Identical:** U1 CP2102 USB-serial; U2 CE6260B33M LDO; U3 TP4054; Q2 AO3401 + D2 1N5819 USB/battery switchover; F1 6 V 500 mA fuse; JP1 `1.25X2P-LiPo`; R14 390K/1% + R17 100K/1% divider gated by R7 2N7002BKS on ADC_Ctrl; U9 SX1262 on the same `LoRa_NSS/MISO/MOSI/SCK/RST/BUSY/DIO1` nets. **Pins confirmed by the [WSL V3 Rev1.1 datasheet](https://resource.heltec.cn/download/Wireless_Stick_Lite_V3/HTIT-WSL_V3(Rev1.1).pdf): GPIO36 = Vext Ctrl, GPIO37 = ADC Ctrl, GPIO1 = Read VBAT Voltage, GPIO35 = LED Write Ctrl, GPIO43/44 = CP2102.** Same as the V3. **Differences:** headers 2×20 (was 2×18); **two IPEX sockets, not one** (see F-11); Vext switched by discrete AO3401s rather than the V3's AO7801 dual; RF switch part numbers now visible (U8 UPG2179, Q6 2SC3356). **HW-05 transfers verbatim** — same connector, same `VBAT`/`GND` nets on JP1, same meter-check procedure. See §3.5. | 2026-08-08 |
| HW-18 | Does the Stick Lite V3 have a **Vext rail at all**, and if so does it still show the ~1.44 V residual with no OLED to pull it up? | If Vext works cleanly here, **HW-11's P-FET load switch may be unnecessary** — one BOM line and a chunk of firmware removed | `promoted` | **Yes, Vext exists** — GPIO36, active LOW, datasheet describes it as "Output 3.3V, power supply for external sensor." Topology is a textbook P-FET high-side switch: AO3401, R11 10K gate pull-up to VDD_3V3 (holds OFF), R12 1K series gate drive from Vext_Ctrl. **The residual is very likely gone, but this is inference, not measurement — mark `[verify]`.** ⚠ **Correction to HW-02:** Round 1 attributed the 1.44 V residual to "R12 10K pull to VDD_3V3." **That attribution was wrong.** On both boards the 10K is a *gate* pull-up to the FET's source — standard, and present here too. The residual is almost certainly **back-powering through the SSD1306's I²C pins**, whose pull-ups sit on VDD_3V3 and feed the OLED through its ESD diodes when Vext is low. Consistent with 1.44 V (≈2 diode drops) and with the Meshtastic report conditioning it on *"if an OLED screen is present."* **No OLED ⇒ no back-power path.** **Meter it before trusting it:** GPIO36 HIGH, measure Vext to GND. If <50 mV, **HW-11 is moot and F-6 is retired.** | 2026-08-08 |
| HW-16 | Stick Lite V3 — same **TP4054** charge IC, same absence of discharge-side protection? | **DEC-006 rests on the TP4054.** If this board has real discharge protection, the protected-cells-plus-firmware-cutoff decision needs revisiting. | `promoted` | **Same TP4054, same absence. DEC-006 stands unchanged — no revisit needed.** WSL V3 schematic: `U3 TP4054` with pins CHRG / GND / BAT / VCC / PROG, identical to the V3.1 schematic. Charge management only: no discharge-side FET, no low-voltage cutoff, no load disconnect. ⚠ **Same vendor-claim conflict carries over verbatim** — the Stick Lite product page also advertises "charge and discharge management, overcharge protection." The schematic supports charge management and charge-side protection only. Protected cells + 3.2 V/cell firmware cutoff remains the right answer, for the reason recorded in DEC-006 (the risk is stuck-*awake*, not stuck-*asleep*). | 2026-08-08 |
| HW-17 | Stick Lite V3 — achievable deep-sleep current. Is it **below** the V3's cited 16 µA, given there is no OLED to disable and no OLED pull-up on Vext? | Would improve the power budget and remove three of the five items from HW-02's disable list | `promoted` | **Likely comparable or slightly better — call it ≤16 µA and do not change the budget. ⚠ But the headline vendor number is a trap in the other direction.** The product page advertises "**basic low-power design (sleep current ≤800uA)**" — that is a **stale V2-era spec** (F-9). Heltec's own `HTIT-WS_V3` datasheet carries a V2-vs-V3 table: **800 µA (V2) vs <10 µA (V3)**, and the [hardware update log](https://docs.heltec.cn/en/node/esp32/hardware_update_log.html) attributes the 800 µA line to the 2019 V2 revision (which used GPIO21 for Vext and GPIO13 for battery sense — *not* V3 pins). **Independent measured support:** a WSL V3 user with a Nordic PPK2 reports deep sleep base is "a few µA" and "close to zero" ([thread](http://community.heltec.cn/t/what-is-using-so-much-power-in-light-sleep-mode-wsl-v3/11866)). **Disable list shrinks from 5 items to 3**, but gains a new one — see HW-20 and §3.5. **Recommendation: leave `SPEC.md` §4 at 20–30 µA.** The 1.8× margin does not depend on winning here, and the measured number stays 16 µA until metered on the actual board. | 2026-08-08 |
| HW-11 | What P-FET load switch (or integrated load-switch IC) suits gating a ~8 mA, 3.3 V sensor rail from an ESP32 GPIO — part number, and does it need a pull-up plus a gate resistor? | F-6 killed Vext as the switched rail; this replaces it. Blocks the BOM. | `promoted` | **Probably moot — resolve HW-18 with a meter first.** If a switch is still needed, two options. **(a) Copy Heltec's own circuit**, which is proven on this exact board: **AO3401** P-FET (SOT-23), source to VDD_3V3, drain to sensor, **10K gate pull-up to source** (holds OFF at boot and through sleep), **1K series gate resistor** from the GPIO. Active LOW. This is literally the Vext circuit — R11/R12/AO3401 on the WSL schematic. **(b) Integrated:** **TI TPS22860** ([datasheet](https://www.ti.com/lit/gpn/TPS22860)) — ultra-low-leakage load switch, 1.65–5.5 V, 200 mA, **positive** logic ON, SOT-23-6 or SC70-6, 1 µF at VIN. One part, no discrete network, ~10 nA leakage class. Breakout available if hand-soldering SOT-23-6 is unappealing. **Either way:** hold the control GPIO through sleep with `rtc_gpio_hold_en()` or the rail glitches on wake. ⚠ Both are SMD — no sane through-hole option at this current and leakage. | 2026-08-08 |
| HW-12 | What DS18B20 form factor for tank headspace air — bare TO-92, or a waterproof stainless probe? The headspace is condensing but the sensor is not submerged. | Condensation on a bare sensor is a corrosion and accuracy risk | `promoted` | **Waterproof stainless probe, ~3 m lead. Not the bare TO-92.** In a condensing headspace, water bridging DQ↔GND on a bare part corrupts readings (the DS18B20's parasitic-power path makes it leakage-sensitive) and the leads corrode over a multi-year deployment. The stainless sheath's added thermal mass is a *benefit* here — headspace air is a slow thermal environment and averaging transients is desirable. **Accuracy grade does not matter much, and that is worth knowing before overspending:** at a 2 m path, **1 °C of probe error = 0.176 % = 3.5 mm.** Even a ±2 °C counterfeit clone yields ~7 mm against the **14 cm** error being corrected — the correction captures >95 % of the benefit with the cheapest probe on the shelf. ⚠ **The real error is placement, not the part — see HW-19.** Buy two (one spare); they are ~$8. Sheath is stainless, so no galvanic concern in an air gap. | 2026-08-08 |
| HW-13 | Does the A02YYUW actually work reliably on a 3.3 V rail? (F-8 — resellers recommend 5 V.) If not, what boost converter, and what does it cost in quiescent draw? | If it needs 5 V the power budget is re-derived and a converter enters the BOM | `blocked` | **Still a bench test at build step 5.** Contingency answered so the BOM has a path: **try VBAT before a boost.** The sensor is spec'd 3.3–5 V; gating it from **VBAT (3.4–4.2 V)** instead of the 3.3 V rail costs nothing and adds no part, and is strictly more headroom (F-13). ⚠ Vext cannot do this — it outputs 3.3 V. Requires HW-11's discrete switch sourced from VBAT. **If a boost is genuinely needed: put it downstream of the load switch and its quiescent draw stops mattering.** Off-budget entirely between wakes; during the ~800 ms window even 1 mA of Iq is 0.8 mA·s against the sensor's 6.4 mA·s. Boost losses are the larger term: 8 mA at 5 V from 3.3 V at ~80 % ≈ 15 mA in ≈ 12 mA·s — roughly doubles the sensor cost and remains negligible beside the MCU's ~32 mA·s. **Conclusion: HW-13 cannot break the power budget either way.** Downgrade F-8's severity. | — |
| HW-14 | Confirm GPIO36 = Vext (active LOW) and GPIO37 = ADC_Ctrl on the actual board revision received. (C-5) | Firmware correctness; cheap to check with a meter | `blocked` | **Still a meter check on receipt — C-5 is right and the board change makes it more so, not less.** Documentary support is now stronger and board-specific: the **WSL V3 Rev1.1 datasheet** lists GPIO36 = Vext Ctrl and GPIO37 = ADC Ctrl for *this* board, and the WSL schematic shows the matching `Vext_Ctrl` / `ADC_Ctrl` nets. ⚠ **Concrete precedent for why C-5 exists:** the V2-era hardware update log documents Vext on **GPIO21** and battery sense on **GPIO13** — Heltec has moved these exact pins across revisions before. **Fold HW-18's Vext-to-GND measurement into the same bench session.** | — |
| HW-19 | *(new — raised by chat)* Does a single DS18B20 at the tank lid actually measure the air the sound travels through? | The correction assumes one temperature for the whole 2 m path. If the headspace stratifies, the probe reads the hottest air and over-corrects. | `promoted` | **⚠ No, and this is a bigger error than anything in HW-12.** The speed-of-sound correction integrates over the *whole* path, but the headspace is not isothermal — a sun-loaded lid can sit 15–20 °C above the air just over the water, and the gradient is worst exactly when the headspace is tallest (empty tank, longest path, biggest error). A lid-mounted probe reads the top of the gradient and **over-corrects**, and the bias is signed and seasonal, not noise the median can remove. **Mitigations, in order of cost:** (1) thermally isolate the probe from the lid — standoff, not bolted to sun-warmed plastic; (2) hang it partway into the headspace on its own lead rather than flush at the top; (3) exploit **C-2** — because raw counts go on the wire and derivation is gateway-side, the gradient model can be **fitted empirically later against manual dip readings and re-derived over stored history without opening a tank**. ⚠ This makes C-2 load-bearing rather than merely tidy. **Recommend: build option (1)+(2), log raw, calibrate in the first season.** | 2026-08-08 |
| HW-20 | *(new — raised by chat)* Is HW-02's deep-sleep disable list complete for the Stick Lite? | HW-02 is `promoted`. If the list is wrong for this board, firmware is written against a stale answer. | `promoted` | **No — it loses three items and gains one, and the one it gains is the expensive one.** **Remove** (no OLED on this board): `displayOff()`; `SDA_OLED`/`SCL_OLED`/`RST_OLED` to input; the OLED-related half of the Vext step. **Add — ⚠ new and worth ~3 mA:** **`pinMode(43, ANALOG)` on U0TXD.** Documented on the WSL V3 by a PPK2 user: *"It is also important to stop leakage on TXD, saving 3mA (!)"* ([thread](http://community.heltec.cn/t/what-is-using-so-much-power-in-light-sleep-mode-wsl-v3/11866)). Do GPIO44 (U0RXD) as well. ⚠ **Also a correction to HW-02's wording:** it says set the LoRa SPI pins to `INPUT`. Heltec's own engineer, in the same thread, uses **`ANALOG`** — which disconnects the digital input buffer rather than just leaving it high-impedance, and is the lower-leakage choice. **Use `ANALOG`, not `INPUT`.** This does not invalidate HW-02's 16 µA figure (measured on a board with no serial attached) but it matters on a node with a live UART to the sensor. | 2026-08-08 |
| HW-21 | *(new — raised by chat)* Does anything about the board change affect the **gateway** end? | F-7 says the gateway shares the board but not the constraints. The board changed for both. | `promoted` | **One thing, and it is a footgun: F-11, the second U.FL socket.** The gateway is the board most likely to be antenna-swapped during range testing, and both sockets are the same connector. **Label E2 (LoRa) with a paint pen before the first antenna goes on.** Otherwise: gateway is USB-powered from the Beelink, so HW-16/17/20 and the whole sleep discussion do not apply to it, exactly as F-7 says. ✅ **A small bonus:** the Stick Lite's dedicated 2.4 GHz IPEX (E3) means the gateway *could* take an external Wi-Fi/BLE antenna if BLE range to a phone ever matters — not needed, since it is wired to the server, but it removes a future constraint the WiFi LoRa 32 V3 imposed (spring antenna only, and Heltec's FAQ says it cannot be replaced). Not scope. Noted only so it is not rediscovered. | 2026-08-08 |

---

## 3.5 Round 2 detail

### The board change is a near-non-event — here is why

The WSL V3 schematic is the WiFi LoRa 32 V3 schematic with the OLED block
deleted. The strongest evidence is an artefact: the WSL schematic **still carries
the section label `OLED Display` with nothing under it**, and still carries the
same misspelled `LiPo electtricity&Vext Ctrl` label. Same power tree, same charge
IC, same fuse, same battery divider, same radio nets, same USB bridge.

Practically: **HW-05 and HW-06 transfer verbatim. HW-01 transfers with one added
warning (F-11). HW-02 needs the amendment in HW-20. Nothing from Round 1 has to
be re-run.**

### Two vendor numbers point in opposite directions

Worth naming explicitly, because they are the two ways this board gets
mis-specified:

- **≤800 µA (product page) — too pessimistic, by ~80×.** Stale V2 figure (F-9).
  Budgeting against it would have killed the design outright: 800 µA × 17,520 h ≈
  **14,000 mAh**, against a ~4,500 mAh cold-derated pack.
- **<10 µA (datasheet comparison table) — too optimistic to bank on.** It is a
  best-case figure, and every forum report of a *worse* number turns out to be
  light sleep or an un-parked pin.

**Neither is a design input.** The number to build against remains the
independently measured **16 µA**, which is conservative for this board and
already what `SPEC.md` §4 brackets.

### Reading forum sleep-current reports on this board

Nearly every alarming number in the WSL V3 threads is **light sleep**, where the
ESP32-S3 baseline sits at 2–7.5 mA. The one thread with a real PPK2 trace is
explicit that deep sleep is separate and fine. When a figure appears without the
sleep mode named, assume light sleep. (F-10.)

### The 1.44 V Vext residual — Round 1 got the mechanism wrong

Round 1 blamed "R12 10K pull to VDD_3V3." That resistor exists on **both**
boards and is the P-FET's **gate** pull-up to its own source — the thing that
holds the switch *off*. It cannot hold the drain at 1.44 V.

The mechanism is almost certainly **back-powering through the SSD1306's I²C
pins**: SDA/SCL pull-ups sit on the always-on VDD_3V3 rail, and with Vext low the
OLED's ESD clamp diodes conduct into the dead rail. 1.44 V is about two diode
drops. It also explains why the Meshtastic report conditions the behaviour on
*"if an OLED screen is present."*

**Consequence:** the conclusion for the Stick Lite is favourable — no OLED, no
back-power path, Vext should reach 0 V. But it is **inference from a corrected
mechanism, not a measurement**, and the previous mechanism was confidently stated
and wrong. Meter it before retiring F-6.

### Bench session on receipt — one sitting, four answers

Everything still `blocked` or `[verify]` collapses into one session with a DMM:

1. **HW-14** — GPIO36 HIGH/LOW, watch Vext. Confirms pin *and* active-LOW sense.
2. **HW-18** — same probe, GPIO36 HIGH: is Vext <50 mV? If yes, **HW-11 is moot,
   F-6 retires, one BOM line and its firmware disappear.**
3. **HW-13** — sensor on 3.3 V, checksum-valid frame rate over a few minutes.
4. **HW-05** — JP1 polarity, USB in, no battery, before any cell is connected.

⚠ Do (4) first. It is the only one that is destructive if skipped.

### Reinforcing C-3, which was right

C-3 argued for skipping the PPK2 but keeping a plain multimeter check, because
the failure modes worth catching are **milliamp-scale**. This round is direct
evidence: the TXD leak in HW-20 is **3 mA**, and the light-sleep baselines are
2–7.5 mA. **A DMM in series reads every one of those perfectly.** Nothing found
this round would have required µA resolution to catch.

### Power budget — unchanged

No term moves. Sleep stays bracketed at 20–30 µA; HW-13 cannot break it
(see that row); HW-20's TXD leak is a *bug class*, not a budget line, and the DMM
check catches it. **~1.8× margin at 15 minutes stands.**

---

## 3.6 CC review of Round 2 (2026-08-08)

**Chat caught an error I promoted.** That is the headline, and it is worth
recording plainly.

### C-6 — Round 1's Vext mechanism was wrong, and §3.3 didn't catch it

HW-02 attributed the 1.44 V Vext residual to "R12 10K pull to VDD_3V3." I
promoted that into `HARDWARE_BUILD_PLAN.md` §6 and into F-6 without auditing it.
HW-18 shows it cannot be right: that 10K is the P-FET's **gate** pull-up to its
own source — present on *both* boards, and the thing that holds the switch off.
It cannot hold the drain at 1.44 V.

The real mechanism is almost certainly back-powering through the OLED's I²C
pull-ups via its ESD clamps — consistent with ~2 diode drops, and with the
Meshtastic report conditioning the symptom on *"if an OLED screen is present."*

My §3.3 review re-derived every **number** in Round 1 and confirmed them, then
took the **mechanism** on faith. Numbers are easy to check and mechanisms are
not, which is exactly why the mechanism was the thing that was wrong. Worth doing
differently next round.

Practical effect is favourable — no OLED, no back-power path — but it is
inference twice over now. **Meter it before retiring F-6.**

### C-7 — HW-19 is a real correction to DEC-007, and it is right

DEC-007 recorded the single-sensor stratification limit as a "residual
limitation, accepted," framing it as *approximate*. HW-19 sharpens that
correctly: the bias is **signed and seasonal**, not noise — a sun-loaded lid
reads the top of the gradient and **over-corrects** — and it is worst exactly
when the headspace is tallest, which is the empty-tank case the sensor exists to
protect. "Approximate" undersold it. DEC-007 amended.

It also promotes **C-2** from tidy to load-bearing: because raw counts go on the
wire and derivation is gateway-side, the gradient model can be fitted empirically
against manual dip readings and **re-derived over stored history without opening
a tank**. That is now the mitigation, not just good hygiene.

### C-8 — HW-20's TXD leak is direct evidence for C-3

C-3 argued for skipping the PPK2 but keeping a multimeter, because the failure
modes worth catching are milliamp-scale. Round 2 found one: **3 mA leaking on
U0TXD** — roughly 150× the sleep budget, and trivially visible on a DMM. Nothing
in this round would have needed µA resolution to catch.

Also promoted from HW-20: use **`ANALOG`, not `INPUT`**, when parking pins.
Round 1 said `INPUT`; `ANALOG` disconnects the digital input buffer and is the
lower-leakage choice. HW-02's 16 µA figure stands (measured with no serial
attached) but the wording was wrong for a node with a live UART to the sensor.

### The vendor number that would have killed the design

F-9 deserves its own line. The Stick Lite product page advertises **≤800 µA**
sleep. Budgeted against, that is 800 µA × 17,520 h ≈ **14,000 mAh** over two
years, against a ~4,500 mAh cold-derated pack — a ~3× deficit that would have
made the whole no-solar design look impossible. It is a stale V2-era figure that
the product page never updated. **Neither vendor number is a design input**; the
budget stays on the independently measured 16 µA, bracketed at 20–30 µA in
`SPEC.md` §4.

### What this round changed about the order

**F-12 is the order-relevant finding**, and it is easy to miss inside a
cross-cutting table: the packing list is board + LoRa antenna + SH1.25×2
connector + pin-map sticker — **no header pins**, unlike the WiFi LoRa 32. Build
step 5 breadboards the sensor, so **2×20 headers are a gap**. The separately
ordered antennas are *not* redundant despite a LoRa antenna being in the box: the
bundled one mates directly to U.FL, and enclosure mounting needs the U.FL→SMA
bulkhead run (HW-01).

### Nothing this round moved the power budget

Confirmed independently: HW-13 cannot break it either way (a boost sits
downstream of the load switch, and its worst case is ~12 mA·s against the MCU's
~32 mA·s); HW-20's TXD leak is a bug class, not a budget line. **~1.8× at 15
minutes stands.**

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
