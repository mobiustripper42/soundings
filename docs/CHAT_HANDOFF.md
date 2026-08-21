# Chat ↔ Claude Code Handoff

*The sync point between open-ended research in Claude chat and the repo that
Claude Code edits. Hand-maintained — the whole file goes out, gets edited, comes
back.*

---

## 0. If you are Claude chat and this file was just handed to you

**Your job: answer the `open` rows in §3.8 (Round 3 — sourcing) and edit this file
in place.** Then hand the whole file back. Everything you need is in it — the brief
is §4, and it is written for this round.

⚠ **Round 3 is a sourcing round, not a research round.** Every part *type* is
already decided. You are being asked for specific products, vendor links, and
current prices — not for design advice, and not for improvements to settled
choices. §4 lists what is settled; treat that list as closed.

**Edit freely:**

- The `Answer`, `Status`, and `Date` cells of **Round 3** rows (§3.8). Set
  `Status` to `answered`.
- A new `## 3.9 Round 3 detail` section, mirroring §3.1 — the table holds the line
  item, that section holds anything that didn't fit in a cell: stock caveats,
  shipping consolidation, a cheaper bundle, a part that forced a rethink.
- The cross-cutting findings table (§3.2) — append new `F-n` rows for anything
  that doesn't belong to a single ID.
- **New question IDs.** Add them. Round 1's two most valuable findings — HW-09
  (the blind zone was wrong by 20 cm) and HW-10 (temperature compensation, a
  14 cm error nobody had noticed) — were both raised by chat going off-script,
  not asked by the repo. If the research surfaces something that should have been
  asked, ask it and answer it.

**Do not edit:**

- **Rounds 1 and 2 (§3, §3.4) and their detail sections.** All `promoted` — a
  historical record of what was decided and why. Tidying them destroys the audit
  trail, including the parts that turned out to be **wrong**: §3.6 carries a struck
  sentence about headers and a correction under it, and that pairing is the point.
- **§3.3 and §3.6.** Those are CC's reviews of your previous rounds, including
  corrections to your own answers. Disagree in your Round 3 answers if you think
  one is wrong — don't rewrite them.
- **§4.1.** Round 2's superseded brief, kept for the record.
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
- **Round 3 only — a price and a link, or it isn't an answer.** "A 2-slot parallel
  holder" is what the row already says. The row is answered when someone could
  place the order from it without deciding anything further.

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

> **§4 is for the other path — but check that it still is.** For Rounds 1 and 2,
> §4 was a self-contained brief that worked without the rest of the file, because
> those rounds' questions were written out in it. **Round 3 is not**: its questions
> are fourteen line items in a §3.8 table, and duplicating them into §4 would put
> the same rows in two places to drift apart. A round whose questions live in a
> table gets a §4 that is *context for* the table, not a replacement — and it says
> so at the top. **Hand over the whole document; that is the default and always
> works.**

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
| F-6 | ~~**Vext is not a usable switched rail** (~1.44 V residual). Any doc or code that assumes Vext gates the sensor is wrong; a discrete P-FET load switch is required.~~ ✅ **RETIRED 2026-08-20 — measured 3.0 mV off, 3.3 V on.** The residual was an OLED back-power artefact and there is no OLED on this board. **Vext gates the sensor; no P-FET.** HW-11 closed with it. See §3.7. | `HARDWARE_BUILD_PLAN.md` §4, firmware |
| F-7 | **Gateway node shares the board but not the constraints.** Mains/USB powered — no sleep budget, no battery BOM, no LVC. Ensure the BOM does not inherit node-only line items. | `HARDWARE_BUILD_PLAN.md` §4 |
| F-8 | ~~**A02YYUW at 3.3 V is unverified.** Reseller guidance recommends 5 V.~~ ✅ **CLOSED 2026-08-21 on the operator's call** — DFRobot's spec table reads 3.3~5 V with no 5 V preference and no low-end warning; the contrary guidance is unsourced reseller copy from the same listings that produced the 15° beam angle and the phantom temperature compensation. **The bench test at step 5 still runs, confirming rather than deciding.** Dissent recorded in `HARDWARE_BUILD_PLAN.md` §9. If it does prove flaky, a boost enters the BOM and the budget is re-derived. | `HARDWARE_BUILD_PLAN.md` §6, §9 |
| F-9 | **The Stick Lite V3's advertised "≤800 µA deep sleep" is a stale V2-era figure.** Heltec's own `HTIT-WS_V3` datasheet carries a V2-vs-V3 comparison table listing deep sleep as **800 µA (V2, ESP32-D0/SX1276/Micro-USB) vs <10 µA (V3, ESP32-S3/SX1262/Type-C)**. The hardware update log attributes the 800 µA line to the 2019 V2 revision. The product page never updated it. **Do not budget against 800 µA.** | `SPEC.md` §4, `HARDWARE_BUILD_PLAN.md` §6 |
| F-10 | **Forum "high sleep current" reports on this board are almost all LIGHT sleep, not deep sleep.** The one WSL V3 thread with an actual PPK2 trace states deep sleep base is "a few µA" and "close to zero"; its 7.5 mA and 2.0 mA figures are light-sleep baselines. Anyone quoting mA figures for this board should be checked for which sleep mode they measured. | `SPEC.md` §4 |
| F-11 | **⚠ The Stick Lite V3 has TWO U.FL/IPEX sockets** — E2 (LoRa, behind the UPG2179 RF switch) and E3 (2.4 GHz Wi-Fi/BT, alongside the E1 spring antenna). The WiFi LoRa 32 V3 has only one. **Plugging the 915 MHz antenna into the 2.4 GHz socket transmits into an unmatched load.** Physically label the correct socket at build time; this is a silent, hard-to-diagnose failure. | `HARDWARE_BUILD_PLAN.md` §5, build checklist |
| F-12 | **Board footprint and packing list both changed.** 58.08 × 22.6 × 8.2 mm (vs 50.2 × 25.5 × 10.2) — longer, narrower, thinner. Headers are **2×20** (vs 2×18). Packing list is board + LoRa antenna + SH1.25×2 connector + pin-map sticker — **no header pins included**, unlike the WiFi LoRa 32. ~~Order headers separately if anything gets breadboarded.~~ ⚠ **Corrected 2026-08-21 — nothing gets breadboarded, and headers are not a purchase.** See §3.6. The footprint half of this row still stands and still sizes the enclosure. | `HARDWARE_BUILD_PLAN.md` §4 BOM, §5 enclosure sizing |
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
| HW-18 | Does the Stick Lite V3 have a **Vext rail at all**, and if so does it still show the ~1.44 V residual with no OLED to pull it up? | If Vext works cleanly here, **HW-11's P-FET load switch may be unnecessary** — one BOM line and a chunk of firmware removed | `promoted` — ✅ **and now measured** | ✅ **2026-08-20: Vext off reads 3.0 mV. The inference held; F-6 retired, HW-11 closed, the P-FET is off the BOM.** See §3.7. Original answer follows. **Yes, Vext exists** — GPIO36, active LOW, datasheet describes it as "Output 3.3V, power supply for external sensor." Topology is a textbook P-FET high-side switch: AO3401, R11 10K gate pull-up to VDD_3V3 (holds OFF), R12 1K series gate drive from Vext_Ctrl. **The residual is very likely gone, but this is inference, not measurement — mark `[verify]`.** ⚠ **Correction to HW-02:** Round 1 attributed the 1.44 V residual to "R12 10K pull to VDD_3V3." **That attribution was wrong.** On both boards the 10K is a *gate* pull-up to the FET's source — standard, and present here too. The residual is almost certainly **back-powering through the SSD1306's I²C pins**, whose pull-ups sit on VDD_3V3 and feed the OLED through its ESD diodes when Vext is low. Consistent with 1.44 V (≈2 diode drops) and with the Meshtastic report conditioning it on *"if an OLED screen is present."* **No OLED ⇒ no back-power path.** **Meter it before trusting it:** GPIO36 HIGH, measure Vext to GND. If <50 mV, **HW-11 is moot and F-6 is retired.** | 2026-08-08 |
| HW-16 | Stick Lite V3 — same **TP4054** charge IC, same absence of discharge-side protection? | **DEC-006 rests on the TP4054.** If this board has real discharge protection, the protected-cells-plus-firmware-cutoff decision needs revisiting. | `promoted` | **Same TP4054, same absence. DEC-006 stands unchanged — no revisit needed.** WSL V3 schematic: `U3 TP4054` with pins CHRG / GND / BAT / VCC / PROG, identical to the V3.1 schematic. Charge management only: no discharge-side FET, no low-voltage cutoff, no load disconnect. ⚠ **Same vendor-claim conflict carries over verbatim** — the Stick Lite product page also advertises "charge and discharge management, overcharge protection." The schematic supports charge management and charge-side protection only. Protected cells + 3.2 V/cell firmware cutoff remains the right answer, for the reason recorded in DEC-006 (the risk is stuck-*awake*, not stuck-*asleep*). | 2026-08-08 |
| HW-17 | Stick Lite V3 — achievable deep-sleep current. Is it **below** the V3's cited 16 µA, given there is no OLED to disable and no OLED pull-up on Vext? | Would improve the power budget and remove three of the five items from HW-02's disable list | `promoted` | **Likely comparable or slightly better — call it ≤16 µA and do not change the budget. ⚠ But the headline vendor number is a trap in the other direction.** The product page advertises "**basic low-power design (sleep current ≤800uA)**" — that is a **stale V2-era spec** (F-9). Heltec's own `HTIT-WS_V3` datasheet carries a V2-vs-V3 table: **800 µA (V2) vs <10 µA (V3)**, and the [hardware update log](https://docs.heltec.cn/en/node/esp32/hardware_update_log.html) attributes the 800 µA line to the 2019 V2 revision (which used GPIO21 for Vext and GPIO13 for battery sense — *not* V3 pins). **Independent measured support:** a WSL V3 user with a Nordic PPK2 reports deep sleep base is "a few µA" and "close to zero" ([thread](http://community.heltec.cn/t/what-is-using-so-much-power-in-light-sleep-mode-wsl-v3/11866)). **Disable list shrinks from 5 items to 3**, but gains a new one — see HW-20 and §3.5. **Recommendation: leave `SPEC.md` §4 at 20–30 µA.** The 1.8× margin does not depend on winning here, and the measured number stays 16 µA until metered on the actual board. | 2026-08-08 |
| HW-11 | What P-FET load switch (or integrated load-switch IC) suits gating a ~8 mA, 3.3 V sensor rail from an ESP32 GPIO — part number, and does it need a pull-up plus a gate resistor? | F-6 killed Vext as the switched rail; this replaces it. Blocks the BOM. | `promoted` | **Probably moot — resolve HW-18 with a meter first.** If a switch is still needed, two options. **(a) Copy Heltec's own circuit**, which is proven on this exact board: **AO3401** P-FET (SOT-23), source to VDD_3V3, drain to sensor, **10K gate pull-up to source** (holds OFF at boot and through sleep), **1K series gate resistor** from the GPIO. Active LOW. This is literally the Vext circuit — R11/R12/AO3401 on the WSL schematic. **(b) Integrated:** **TI TPS22860** ([datasheet](https://www.ti.com/lit/gpn/TPS22860)) — ultra-low-leakage load switch, 1.65–5.5 V, 200 mA, **positive** logic ON, SOT-23-6 or SC70-6, 1 µF at VIN. One part, no discrete network, ~10 nA leakage class. Breakout available if hand-soldering SOT-23-6 is unappealing. **Either way:** hold the control GPIO through sleep with `rtc_gpio_hold_en()` or the rail glitches on wake. ⚠ Both are SMD — no sane through-hole option at this current and leakage. | 2026-08-08 |
| HW-12 | What DS18B20 form factor for tank headspace air — bare TO-92, or a waterproof stainless probe? The headspace is condensing but the sensor is not submerged. | Condensation on a bare sensor is a corrosion and accuracy risk | `promoted` | **Waterproof stainless probe, ~3 m lead. Not the bare TO-92.** In a condensing headspace, water bridging DQ↔GND on a bare part corrupts readings (the DS18B20's parasitic-power path makes it leakage-sensitive) and the leads corrode over a multi-year deployment. The stainless sheath's added thermal mass is a *benefit* here — headspace air is a slow thermal environment and averaging transients is desirable. **Accuracy grade does not matter much, and that is worth knowing before overspending:** at a 2 m path, **1 °C of probe error = 0.176 % = 3.5 mm.** Even a ±2 °C counterfeit clone yields ~7 mm against the **14 cm** error being corrected — the correction captures >95 % of the benefit with the cheapest probe on the shelf. ⚠ **The real error is placement, not the part — see HW-19.** Buy two (one spare); they are ~$8. Sheath is stainless, so no galvanic concern in an air gap. | 2026-08-08 |
| HW-13 | Does the A02YYUW actually work reliably on a 3.3 V rail? (F-8 — resellers recommend 5 V.) If not, what boost converter, and what does it cost in quiescent draw? | If it needs 5 V the power budget is re-derived and a converter enters the BOM | `blocked` — **on the part, not on research** | ⚠ **2026-08-20: research is exhausted; only the sensor can answer this.** DFRobot's own spec table reads **"Operating Voltage 3.3~5V"** with **no** recommendation of 5 V and **no** note of degraded performance at the low end ([wiki](https://wiki.dfrobot.com/sen0311/), [datasheet](https://media.digikey.com/pdf/Data%20Sheets/DFRobot%20PDFs/SEN0311_Web.pdf)). The "prefer 5 V" line is reseller folklore with no manufacturer backing — **F-8 downgrades again.** In-spec is not the same as reliable at the bottom of the range, which is the whole reason F-8 exists, so the bench check stands. Original answer follows. **Still a bench test at build step 5.** Contingency answered so the BOM has a path: **try VBAT before a boost.** The sensor is spec'd 3.3–5 V; gating it from **VBAT (3.4–4.2 V)** instead of the 3.3 V rail costs nothing and adds no part, and is strictly more headroom (F-13). ⚠ Vext cannot do this — it outputs 3.3 V. Requires HW-11's discrete switch sourced from VBAT. **If a boost is genuinely needed: put it downstream of the load switch and its quiescent draw stops mattering.** Off-budget entirely between wakes; during the ~800 ms window even 1 mA of Iq is 0.8 mA·s against the sensor's 6.4 mA·s. Boost losses are the larger term: 8 mA at 5 V from 3.3 V at ~80 % ≈ 15 mA in ≈ 12 mA·s — roughly doubles the sensor cost and remains negligible beside the MCU's ~32 mA·s. **Conclusion: HW-13 cannot break the power budget either way.** Downgrade F-8's severity. | — |
| HW-14 | Confirm GPIO36 = Vext (active LOW) and GPIO37 = ADC_Ctrl on the actual board revision received. (C-5) | Firmware correctness; cheap to check with a meter | `promoted` **(GPIO36 half)** | ✅ **Measured 2026-08-20 — GPIO36 = Vext, active LOW, confirmed.** `Ve` reads 3.3 V with firmware running (so GPIO36 is being driven low) and 3.0 mV with the chip held in reset. Promoted to `HARDWARE_BUILD_PLAN.md` §6. ⚠ **GPIO37 = ADC_Ctrl was not measured** and remains documentary only — see §3.7. Original answer follows. **Still a meter check on receipt — C-5 is right and the board change makes it more so, not less.** Documentary support is now stronger and board-specific: the **WSL V3 Rev1.1 datasheet** lists GPIO36 = Vext Ctrl and GPIO37 = ADC Ctrl for *this* board, and the WSL schematic shows the matching `Vext_Ctrl` / `ADC_Ctrl` nets. ⚠ **Concrete precedent for why C-5 exists:** the V2-era hardware update log documents Vext on **GPIO21** and battery sense on **GPIO13** — Heltec has moved these exact pins across revisions before. **Fold HW-18's Vext-to-GND measurement into the same bench session.** | — |
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
connector + pin-map sticker — **no header pins**, unlike the WiFi LoRa 32.
~~Build step 5 breadboards the sensor, so **2×20 headers are a gap**.~~ The
separately ordered antennas are *not* redundant despite a LoRa antenna being in
the box: the bundled one mates directly to U.FL, and enclosure mounting needs the
U.FL→SMA bulkhead run (HW-01).

⚠ **Correction, 2026-08-21 — headers are not a gap and not a purchase.** The
struck sentence above is wrong and a sourcing round acted on it. Build step 5
**solders the six sensor wires directly to the board pads; nothing is
breadboarded** (`HARDWARE_BUILD_PLAN.md` §8), which is the better choice for a
sealed outdoor node anyway — headers plus jumpers are a vibration and corrosion
liability. The owner also has header stock. **Do not source headers.**

### Nothing this round moved the power budget

Confirmed independently: HW-13 cannot break it either way (a boost sits
downstream of the load switch, and its worst case is ~12 mA·s against the MCU's
~32 mA·s); HW-20's TXD leak is a bug class, not a budget line. **~1.8× at 15
minutes stands.**

---

## 3.7 Bench session, 2026-08-20 — three of four checks, no research involved

Not a research round. Recorded here because §3.5 predicted this session and named
what it would resolve, and three of those four predictions came true with a
multimeter.

| Check | Reading | Resolves |
|---|---|---|
| JP1 polarity, USB in, no cell | **4.0 V** on the pin nearer the **USER** button; the pin nearer **RST** is GND | HW-05 |
| `Ve` with firmware running | **3.3 V** — GPIO36 is being driven low, so active-LOW confirmed | HW-14 (GPIO36 half) |
| `Ve` with RST held | **3.0 mV** | **HW-18 — F-6 retired, HW-11 closed** |
| A02YYUW at 3.3 V | — | HW-13, blocked on the order |

**The finding worth carrying: check 3 does not need firmware.** §3.5 wrote it as
*"GPIO36 HIGH: is Vext <50 mV?"*, which reads like it needs a sketch or the
Meshtastic remote-hardware module — and that module has an
[open ESP32-S3 bug](https://github.com/meshtastic/firmware/issues/6276) that would
likely have eaten the afternoon. **Holding the RST button gets the reading.** In
reset GPIO36 should be high-impedance, so the 10K gate pull-up ties the AO3401's
gate to its own source; Vgs = 0 and the FET is off — which should be the same drain
state driving the pin high produces. GPIO36 is not an S3 strapping pin.

⚠ **That equivalence is inference, mine, uncited — and HW-18's literal test
(drive GPIO36 high, measure) was never run.** The measurement stands on its own for
what HW-18 is actually asking, which is whether anything back-powers the drain;
nothing does, and a back-power path would show in reset just as readily. But do not
carry this forward as a general fact about Heltec V3s — it is validated on this
board and this revision.

**Sequencing, promoted to `HARDWARE_BUILD_PLAN.md` §8:** these checks were pulled
ahead of "order the rest of the BOM" on purpose. Check 3 decides whether the P-FET
is bought at all and needs nothing that has to be bought. Where a bench check gates
a purchase and requires only what is already on hand, it runs before the order.

⚠ **Two things this session did *not* establish, stated so they are not assumed:**

- **GPIO37 = ADC_Ctrl was not measured.** HW-14 asks about both pins; only GPIO36
  was checked. The battery-sense gate is still documentary (WSL V3 Rev1.1
  datasheet), and C-5 exists precisely because Heltec moved that pin between V2 and
  V3. Free to check next time a board is on the bench.
- **Polarity is one sample of one board.** The JP1 orientation above is a
  measurement, not a datasheet guarantee. Meter each board before connecting a cell.

---

## 3.8 Round 3 — sourcing (opened 2026-08-21)

**A different kind of round.** Rounds 1 and 2 asked *what kind of part*. Every one
of those is answered. This round asks **which SKU, from whom, at what price** — and
it is the last thing between here and an order.

**Answer by ID. One row, one line item.** Each answer wants: a **specific product**,
a **vendor link**, a **price**, and the **spec that proves it meets the constraint
in the `Why it matters` column**. A part that meets the constraint but is out of
stock is not an answer; say so and give the next one.

| ID | Line item | Why it matters | Status | Answer | Date |
|----|-----------|----------------|--------|--------|------|
| SR-01 | **Protected 18650 cells ×2** | ⚠ **The constraint that will bite: protected cells are longer than bare ones**, and it has to match SR-02. Give the length. Matched capacity/IR — same make, same model, ideally same batch. | `promoted` | **Panasonic NCR18650B Protected Button Top, 3400 mAh — ~$12–14 ea, buy 2 (same listing, same order, so same batch).** [18650batterystore.com](https://www.18650batterystore.com/products/panasonic-18650-protected) (Nevada, tests cells) or [Illumn](https://illumn.com/18650-keeppower-3400mah-panasonic-ncr18650b-protected-button-top.html) (KeepPower P1834J, same cell). **Proving spec — the length, which is the whole point of this row: 68.9 mm** (Illumn, stated explicitly) **to 69.5 mm** (18650batterystore's NCR18650GA listing, "≈69.5 mm Protected Button-Top Length"). **Call it 70 mm for fit purposes.** Bare cells are 65 mm; **the protection PCB and button top add ~4–5 mm.** ⚠ **Do not buy from Amazon** — the 18650 market is a counterfeit swamp and Amazon is the worst of it. ⚠ **UN3480: loose lithium cells ship ground only, always a separate shipment.** Never combine into an air/international order. **2× 3400 = 6800 mAh, above the 6000 mAh SPEC figure** — margin improves, no re-derivation needed. | 2026-08-21 |
| SR-02 | **2-cell 18650 holder, PARALLEL** | ⚠ **Two silent failures.** Series holders (7.4 V) destroy the board — confirm parallel. And it must **seat the SR-01 cells at their actual length**. Quote the holder's stated cell length. | `promoted` — ⚠ **with a caveat that matters** | **⚠ Research cannot fully close this row, and pretending otherwise would be the wrong answer.** Almost no holder vendor publishes an internal bay length; the ones that mention it do so as a warning. Direct evidence the risk is real: PowerMav's parallel 2-cell holder states **"Fits standard unprotected 18650 cells (~65 mm length). May not fit longer protected cells"** ([listing](https://powermavelectronics.com/shop/battery/2x-battery-holder-for-18650-lithium-battery-parallel-with-wire/)), and an Amazon reviewer of a common 2-slot holder reports button-tops fit "extremely tight — such that you have to pry them back out with a flat edge tool." **Recommendation: do not buy a 2-slot holder. Buy two SINGLE-cell holders and wire them parallel yourself.** [SparkFun PRT-12899](https://www.digikey.com/en/products/detail/sparkfun-electronics/PRT-12899/17828154), **$1.36 ea at DigiKey, 4 in stock** — single cell, wire leads, chassis mount. **Why this is strictly better, not just a workaround:** (1) **it deletes the series/parallel failure mode entirely** — you make the joint, so it cannot arrive wrong; (2) each cell gets its own independent spring travel instead of sharing a moulding cut for 65 mm; (3) two singles can be mounted apart, which suits a 4×4×2" box better than one 76×41 mm brick; (4) a dead holder is $1.36, not a whole assembly. ⚠ **Even so, no single-cell holder publishes a bay length either.** Order the cells and holders together, and **check the fit before building anything around them** — bend a contact or shim with a spring if it's tight. **Alternative if a 2-slot is wanted anyway:** [Keystone 1049](https://www.digikey.com/en/products/detail/keystone-electronics/1049/2745670), $6.17, 4,202 in stock, UL94V-0, spring steel contacts — but it is **PC-pin, PCB-mount**, which is wrong for this build, and Keystone does not publish a max cell length either. **See §3.9.** | 2026-08-21 |
| SR-03 | **IP65 enclosure, light-coloured, ~4×4×2"** | Board is 58.08 × 22.6 × 8.2 mm; also holds the holder, cells, and Wago. Hinged lid preferred. Light-coloured is a heat spec, not aesthetics — it sits outdoors in summer. | `promoted` — ⚠ **size up** | **Recommend a light-grey ABS/polycarbonate IP65/IP67 junction box, ~150 × 110 × 70 mm (6 × 4.3 × 2.75"), clear or grey hinged lid, ~$15–25.** Bud Industries PN-series, Hammond 1554/1555, and Polycase WC/WH series all qualify and are stocked at DigiKey and Mouser; the generic grey Chinese IP65 boxes on Amazon are also fine for this and a third the price. ⚠ **Contradiction with the row as written: 4×4×2" is too small, and the shortfall is in depth, not footprint.** Stack-up on the lid-to-base axis is board (8.2 mm) + cells (**18.6 mm diameter**, and SR-02's two singles sit side by side) + holder base + the U.FL pigtail's ~10 mm minimum bend radius (RG178 spec) + gland bodies. **2" (50 mm) internal depth leaves no room for the coax bend, which is the item that will actually force a re-order.** 70 mm is comfortable. ⚠ **The light colour is worth holding firm on and is under-argued in the row:** the constraint is the *cells*, which degrade above ~45 °C, not the electronics. A dark box in Cleveland August sun is plausibly 20 °C over ambient. **Grey or white, and mount it shaded regardless.** ⚠ **Wall thickness drives SR-05** — measure it before ordering the pigtail, or order the 14.5 mm thread version and stop worrying. | 2026-08-21 |
| SR-04 | **PG7 cable glands ×2** | Sensor cable plus one spare. Must suit the SR-08 cable OD. | `promoted` — ⚠ **wrong size for the cable** | **Buy PG9, not PG7, and buy a 10-pack of assorted PG7/PG9/PG11 nylon glands for ~$10 rather than two of anything.** ⚠ **This row contradicts SR-08 and the contradiction is a re-order:** **PG7 clamps roughly 3–6.5 mm OD. Outdoor/direct-burial Cat5e is typically 5.5–6.8 mm OD, and gel-filled or armoured variants run 7–8 mm.** PG7 is at best marginal and quite possibly too small. **PG9 covers ~4–8 mm** and swallows the whole range. **Get the actual OD off the SR-08 spool listing before ordering, and buy the assortment as insurance** — glands are pennies and a second shipment is not. ⚠ **Also: two glands is one short.** Penetrations needed: (a) sensor cable, (b) DS18B20 lead — *unless* the DS18B20 shares the SR-08 jacket, which it should, since Cat5e has 8 conductors and only 3 are spoken for. **Decide that now**, because it is the difference between one gland and two. Plus (c) the SMA bulkhead, which is its own hole. **Glands point down (already settled) and want a nylon washer plus a smear of the SR-13 silicone on the threads.** | 2026-08-21 |
| SR-05 | **U.FL → SMA-female pigtail, 100–150 mm ×2** | ⚠ **As short as it ships** — 1–1.5 dB/m at 900 MHz, so a long one costs more than the antenna gains. | `promoted` — ⚠ **may already be bought; see §3.9** | **[Data Alliance IPEX→SMA-female bulkhead cable](https://www.data-alliance.net/ipex-to-sma-female-cable-2-inch-3-in-4-in-5-in-6-in-7-in-8-in-9-in-10-in/), 4–6 inch, US vendor, ~$10–13 ea — select the 14.5 mm extended-thread option.** Their standard thread is 3/8" (9.5 mm); the longer option **has an O-ring embedded in the flange to waterproof the port where the cable enters an enclosure**, which is exactly this application and removes a failure point. **Alternative, in stock on Amazon:** Proxicast **ANT-105-SMA-2PK**, 8", 1.37 mm low-loss coax, bulkhead mount, explicitly listed for 900 MHz / LoRa, ~$15/pair. ⚠ **SR-05 and SR-06 are ONE part, not two — see §3.9.** ⚠ **The single most likely sourcing error on this whole list is SMA vs RP-SMA.** LoRa uses **standard SMA**; most cheap U.FL pigtails are **RP-SMA**, because they are repurposed WiFi parts, and an RP-SMA bulkhead will not mate with an SMA-male antenna. **Bulkhead must be SMA FEMALE = outer thread, centre SOCKET.** Both parts above are correct as specified; verify at checkout anyway. ⚠ Note the ProBots 11 mm-thread part that turns up first in searches **is RP-SMA** — right idea, wrong gender. | 2026-08-21 |
| SR-06 | **SMA bulkhead ×2** | Through the enclosure wall. Not weatherproof by default — self-amalgamating tape goes on the outside joint. | `promoted` — **⚠ DUPLICATE, do not order** | **⚠ There is no such separate line item. SR-06 is already inside SR-05.** A "U.FL → SMA-female **bulkhead** pigtail" *is* the bulkhead: the SMA-female end is a threaded barrel supplied with nut and washer (and, on the Data Alliance 14.5 mm option, an O-ring) that mounts through the enclosure wall. Ordering SR-05 and SR-06 separately yields **two pigtails and two loose connectors that cannot be joined.** **Action: merge SR-06 into SR-05 and strike this row from the BOM.** The one real item hiding in this row's `Why it matters` column is the **self-amalgamating tape**, which is named as required but **appears in no BOM row at all** — see the new **SR-17**. | 2026-08-21 |
| SR-07 | **915 MHz antenna, 3–5 dBi omni, SMA male ×2** | ⚠ **Not 8–10 dBi.** The gain comes from flattening the vertical pattern; gateway and node sit at different heights. See the range-test note in §4. | `promoted` — **⚠ ALREADY BOUGHT, do not order** | **⚠ This row contradicts §4 of this same file.** §4 states under *Already bought, do not re-source*: **"3× Heltec Wireless Stick Lite V3 and 3× 915 MHz whip antennas ($67.20 delivered)."** SR-07 asks for two more. **Three are in hand for two radios.** The Heltec 868/915 MHz whip is **4 dBi, VSWR ≤1.5, DC ground** — inside the 3–5 dBi window this row specifies, better VSWR than the 3 dBi glue-rod alternative, and the DC-ground path is a genuine plus on a mast in an open field ([heltec.org/project/sma-antenna](https://heltec.org/project/sma-antenna/)). **The row is satisfied by the existing purchase. Strike it.** ⚠ One spec to note rather than act on: the whip is rated **−40 … +55 °C**, a lower ceiling than typical 4 dBi parts. It is a bare whip in moving air, so it is fine — but do not mount it against a dark surface in full sun. | 2026-08-21 |
| SR-08 | **Outdoor / direct-burial Cat5e** | Length = tank height + slack. Unshielded is fine. 3 conductors used. | `promoted` | **Direct-burial / outdoor-rated CMX Cat5e, UV-resistant PE jacket, solid copper (not CCA) — ~$0.20–0.35/ft, or a 100 ft spool for ~$25–35.** Any of Monoprice, trueCABLE, Cable Matters, or a local supply house; this is a commodity and the brand does not matter. **Two specs that do: solid copper, not copper-clad aluminium** (CCA work-hardens and snaps at a strain point, which a tank lid is), and a **UV-rated jacket** — "outdoor" alone sometimes means moisture-rated only. ⚠ **Buy 100 ft, not "tank height plus slack."** A spool is ~$30 against ~$12 for a cut length, and the run gets re-pulled, re-routed, and re-terminated more than once on a first build. ⚠ **Get the published OD and hand it to SR-04 before ordering glands.** ⚠ **8 conductors, 3 spoken for** — put the DS18B20 on the spare pairs in the same jacket rather than running a second cable and cutting a second gland hole. That decision belongs in SR-04. **Gel-filled is unnecessary** for an above-ground run and pushes the OD past PG9. | 2026-08-21 |
| SR-09 | **DS18B20 waterproof stainless probe, ~3 m lead ×2** | Cheapest acceptable grade — at a 2 m path, 1 °C of probe error is 3.5 mm against a 14 cm error being corrected. One is a spare. | `promoted` | **Generic waterproof stainless DS18B20, 3 m lead, ~$8–10 ea in a 2- or 5-pack — Amazon or AliExpress is correct here, and this is the one row where the cheap part is genuinely the right answer.** The arithmetic in the `Why it matters` column is the justification and it holds: **a ±2 °C counterfeit clone still yields ~7 mm of residual against the 14 cm being corrected — >95 % of the benefit.** Do not pay for ±0.5 % Maxim-authentic parts from DigiKey at 4× the price. **Buy 4, not 2** — they are ~$2 each in a 5-pack and clones do occasionally arrive dead, which you want to discover on the bench and not up a tank. ⚠ **Missing part this row implies: a 4.7 kΩ pull-up resistor on the DQ line.** Not in SR-13's consumables list. Folded into **SR-17**. ⚠ **Not a sourcing issue but it governs whether this part earns its price: HW-19.** The dominant error is *where the probe hangs*, not what it costs — thermally isolated from the lid, hanging partway into the headspace, not bolted flush to sun-warmed plastic. | 2026-08-21 |
| SR-10 | **A02YYUW ultrasonic sensor ×1** | The one part with an open question against it (HW-13, 3.3 V reliability). Buy it anyway; only the part can answer. | `promoted` | **DFRobot SEN0311, ~$25–30. In stock at [DigiKey](https://www.digikey.com/en/products/detail/dfrobot/SEN0311/11202577) (ships same day), [Mouser](https://www.mouser.com/ProductDetail/DFRobot/SEN0311), and [DFRobot direct](https://www.dfrobot.com/product-1935.html).** ⚠ **I do not have a confirmed current price** — the distributor pages returned stock status but not the price line. **Buy from DigiKey or Mouser, not a marketplace:** this part has clones, and the reseller-copy problems already logged against it (15° beam angle, "has temperature compensation," "prefer 5 V") all originate from marketplace listings. **Proving spec, restated because two rounds of reseller folklore contradicted it:** DFRobot's own spec table reads **3.3~5 V operating voltage, 3 cm blind zone, 3–450 cm range, 60° sensing angle, ≤8 mA average** ([wiki](https://wiki.dfrobot.com/sen0311/), [datasheet](https://media.digikey.com/pdf/Data%20Sheets/DFRobot%20PDFs/SEN0311_Web.pdf)). ⚠ **Buy TWO, not one.** HW-13 is `blocked` on this part and can only be answered by testing it at 3.3 V; if the first one is marginal you cannot tell a bad unit from a bad rail with a sample size of one. At ~$27 that ambiguity is not worth a second shipment. **Ships with a PH2.0-4P connector; stock lead is short (HW-04), so SR-08 is doing the real run.** | 2026-08-21 |
| SR-11 | **JST 1.25 2-pin battery pigtail** | ⚠ Heltec calls it "SH1.25-2" and **the name is wrong** — real JST SH is 1.0 mm pitch. Look for *"JST 1.25 2-pin for Heltec/LilyGo."* | `promoted` | **[Rokland JST 1.25 battery connector cables, 5-pack for LilyGo and Heltec](https://store.rokland.com/products/battery-connector-cables-battery-wires-jst-1-25-5pcs-for-lilygo-and-heltec) — ~$7–9, Florida, free US shipping.** Sold specifically for these boards, which is what makes it the right SKU rather than a guess at pitch. Generic "JST 1.25 2-pin pigtail" 10-packs on Amazon are ~$7 and also fine. **Buy a multipack regardless — one ships with each board, and they are fragile.** ⚠ **Rokland's own listing warns to check positive and negative on the board before hooking up.** That check is already done and recorded in §3.7: **4.0 V on the pin nearer the USER button; the pin nearer RST is GND.** ⚠ **§3.7 also says that is one sample of one board — meter each of the three before connecting a cell.** Wire colour on the pigtail means nothing and varies between vendors. | 2026-08-21 |
| SR-12 | **PVC 4" or 6" Sch40 + matching hole saw** | Cut to 50–75 mm. A shallow wide hood, not a deep tube. Local hardware is likely cheaper than shipped — say so if that's the answer. | `promoted` — **buy local** | **Yes, buy local. Home Depot / Lowe's / any plumbing supply. 4" Sch40 PVC by the 2 ft length ~$8–12; a 4" bi-metal hole saw with arbor ~$25–35.** Shipping 4" pipe is absurd and the hole saw is a stocked item everywhere. **Take 4", not 6"** — 6" pipe and a 6" hole saw are meaningfully more expensive, a 6" hole in a tank lid is a bigger irreversible commitment, and 4" already permits **88 mm** of standoff against the **50–75 mm** actually wanted (HW-03: `L_max ≈ 1.73 × r_inner`, 4" Sch40 ID 4.03"). **The clearance is not the binding constraint at 4".** ⚠ **Check the hole saw's own cutting depth** — many bi-metal saws cut ~1.5–1.9", which is fine for a tank lid but worth confirming at the shelf. ⚠ **Buy the pipe before cutting the lid.** A 2 ft length gives four or five attempts at a 50–75 mm collar; the tank lid gives one. | 2026-08-21 |
| SR-13 | **Consumables** — UV-stable silicone, silica gel, Wago lever nuts, 100 nF + 10 µF caps, long USB-C | Low-value, high-annoyance-if-missing. One row so they don't get lost. | `promoted` | **~$40–50 total, mostly local.** **UV-stable silicone:** any neutral-cure exterior-grade silicone, hardware store, ~$8. ⚠ **Neutral-cure, not acetoxy** — acetoxy silicone releases acetic acid while curing and corrodes electronics and metal contacts in a sealed box. Read the tube. **Silica gel:** rechargeable indicating desiccant packs, ~$10 for several, Amazon. Buy indicating (blue/orange) so the colour tells you the seal has failed. **Wago 221 lever nuts:** assorted 2/3-conductor pack, ~$15, hardware store or Amazon. **Caps (100 nF ceramic + 10 µF, per HW-04, at the SENSOR end not the board end):** assorted kit ~$10, or you likely have both in stock. **Long USB-C:** ~$10, for bench work — ⚠ note it is bench-only, since the deployed node is battery-powered and the gateway sits at the Beelink. ⚠ **This row is missing items that other rows assume exist — see SR-17.** | 2026-08-21 |
| SR-15 | **Enclosure mounting hardware** | ⚠ **Answer this one "deferred" unless something obvious fits.** `HARDWARE_BUILD_PLAN.md` §4 carries it as `[proposed]`, and it genuinely depends on what the node straps to at the tank cluster — which nobody has stood in front of and decided. It gets a row so it is a *named* deferral rather than a silent gap; a strap or bracket that arrives with the rest is worth more than a second shipment. | `promoted` — **deferred, as instructed** | **Deferred. Nothing obvious fits, and the row is right that it should not be guessed.** The genuine unknown is what the box straps to — tank wall, a post, the frame between cylinders — and that is a fifteen-second decision standing at the tank cluster that cannot be made from here. **The cheap hedge, if a hedge is wanted with the rest of the order: a pack of stainless hose clamps in the 2–4" range (~$10) and a handful of stainless U-bolts.** Between them they cover strapping to almost any pipe or post, and the leftovers are farm-useful regardless. **But the honest answer to the row as written is: go look at the tank first.** ⚠ Whatever is chosen, **stainless or UV-stable nylon only** — zinc-plated hardware on a wet tank in Ohio is a two-season part. And the box needs to sit **low and shaded** (SPEC), which is a placement constraint as much as a hardware one. | 2026-08-21 |
| SR-16 | *(new — raised by chat)* **Li-ion charger capable of charging cells individually** | ⚠ **HW-05 requires that paralleled cells be charged to the same voltage before joining, and nothing in this order can do that.** Two cells at different SoC dump unlimited equalising current into each other through no current limit. The board's TP4054 charges the *pack*, not the cells, and only after they are already joined. | `promoted` | **XTAR VC2SL or VC4SL, or Nitecore i2 / D4 — ~$20–35, Amazon or any of the SR-01 vendors.** Any independent-bay Li-ion charger works; the requirement is **per-bay independent channels** (all four listed have them) and, ideally, a **voltage readout** so you can confirm both cells sit within ~0.05 V of each other before joining. **This is not a nice-to-have.** SR-01's cells arrive at storage charge, typically ~3.6–3.7 V, but not identically; and every future battery swap — which is the entire maintenance plan after HW-07 was descoped — needs the same equalisation step. **Without this, the plan is "buy new cells every time," which defeats the point of a rechargeable node.** ⚠ Also the only way to verify the cells are what the label says before sealing them in a box up a tank: a charger with a capacity-test mode catches a counterfeit in one cycle. **Recommend the XTAR VC2SL** — USB-C, two bays, voltage display, ~$20. | 2026-08-21 |
| SR-17 | *(new — raised by chat)* **Three parts named in other rows' constraints but present in no BOM row** | ⚠ **Each is named as required somewhere in this ledger and sourced nowhere.** This is the SR-14 failure mode happening in miniature — the constraint got written down, the line item did not. | `promoted` | **(a) Self-amalgamating tape** — named in **SR-06's own `Why it matters`** as required for the outside SMA joint, and SR-06 turns out not to be a real line item, so it would have vanished with the row. 3M 130C or any generic rubber splicing tape, ~$10/roll, hardware store or Amazon. **This is the part that keeps water out of the antenna connector**, and it is not optional outdoors. **(b) 4.7 kΩ resistor** — the DS18B20 1-Wire bus pull-up. Implied by SR-09, listed nowhere. Pennies; likely in stock, but confirm before build day. **(c) Heat-shrink assortment + adhesive-lined heat-shrink**, ~$12 — every joint in this build is outdoors: the two-single-holder parallel joint from SR-02, the Cat5e-to-sensor splice, the DS18B20 splice. **Adhesive-lined specifically for the splices in the damp end**, plain for strain relief. ⚠ **Worth naming as a pattern, not just three parts:** all three were discoverable only by reading other rows' constraint columns rather than the line items. SR-14 exists to catch exactly this and **did not fire on its own** — the row's own note already concedes it caught SR-15 "only because a reviewer looked." | 2026-08-21 |
| SR-14 | **Anything this list is missing** | Rounds 1 and 2 both found their most valuable items by going off-script. If assembling this order surfaces a part nobody asked for, add an ID and answer it. **Do not rely on this row to catch a line item the table forgot** — it caught SR-15 only because a reviewer looked, not because the mechanism worked. | `promoted` | **Four findings, two of which delete rows rather than add them.** **(1) SR-06 is not a real part** — it is already inside SR-05. **(2) SR-07 is already bought** and contradicts §4 of this same file. **(3) SR-16 — no charger**, which silently breaks HW-05's equalisation requirement and the entire battery-swap maintenance plan. **(4) SR-17 — three parts named in constraint columns and sourced nowhere**, including the self-amalgamating tape that would have disappeared along with SR-06. **Also flagged inside existing rows rather than as new IDs:** SR-03 is undersized in depth (coax bend radius, not footprint); SR-04 specifies PG7 where the SR-08 cable likely needs PG9, and two glands may be one short; SR-10 should be quantity 2, not 1, because HW-13 cannot be diagnosed with a sample size of one. ⚠ **The pattern across all four: the misses were in rows that already existed, not in categories nobody thought of.** Rounds 1 and 2 found things by researching an unfamiliar part. This round found things by **cross-reading rows against each other** — which is a different check, and the one that should run before the next order. | 2026-08-21 |

⚠ **Do NOT source: 2×20 headers** (owner has stock; wires solder direct to the pads
— see the correction in §3.6) or a **P-FET load switch** (F-6 retired 2026-08-20;
Vext gates the sensor). Both were live in earlier rounds and both are now wrong.

---

## 3.9 Round 3 detail

Rationale and gotchas that did not fit the cells. Keyed by ID.

### The two rows that are deletions

Worth stating first, because they are the findings that change the order rather
than fill it in.

**SR-06 is not a part.** A *U.FL → SMA-female **bulkhead** pigtail* **is** the
bulkhead — the SMA-female end ships as a threaded barrel with nut and washer, and
on the extended-thread version with an O-ring in the flange. Ordering SR-05 and
SR-06 as separate line items produces two pigtails and two loose connectors that
cannot be joined to each other. The row should be struck and its one real
requirement — the self-amalgamating tape — moved to SR-17, or it disappears with
the row.

**SR-07 was bought before the round opened.** §4 of this file lists *"3× Heltec
Wireless Stick Lite V3 and 3× 915 MHz whip antennas ($67.20 delivered)"* under
*Already bought, do not re-source*, and SR-07 asks for two more of the same thing.
Three antennas, two radios. The Heltec whip is **4 dBi, VSWR ≤1.5, DC ground** —
inside the window SR-07 specifies and arguably the better of Heltec's two options.

⚠ **Both errors are internal to this file**, not failures of research: the
information needed to catch them was already written down two sections apart. That
is worth more attention than the parts themselves — see the closing note.

### SR-01 / SR-02 — the fit problem is real and research cannot close it

The row correctly predicts the failure. What research adds is that **it cannot be
designed out from a listing**, because holder vendors do not publish internal bay
length. The two data points that exist are both warnings rather than specs:
PowerMav states its parallel holder fits ~65 mm cells and *"may not fit longer
protected cells,"* and an Amazon reviewer of a common 2-slot holder describes
having to pry button-tops back out with a screwdriver.

**Two singles instead of one double is the answer**, and it is better on four
independent grounds rather than being a workaround:

1. **It deletes the series/parallel failure mode.** The row lists that as one of
   two silent failures. With two single holders there is nothing to get wrong in
   the listing — you make the joint, so you know what it is.
2. **Independent spring travel per cell**, instead of two cells sharing one
   moulding cut for 65 mm.
3. **Better fit in the box.** Two singles can sit apart or stacked; a 76 × 41 mm
   two-slot brick constrains SR-03 more than the board does.
4. **A dead holder is $1.36.**

⚠ **The residual risk does not vanish** — single-cell holders do not publish a bay
length either. **Order cells and holders in the same shipment and test-fit before
building anything around them.** A contact can be bent or a spring shimmed; that
is a five-minute fix if discovered on the bench and a re-order if discovered after
the box is drilled.

### SR-03 — the depth, not the footprint

4 × 4 × 2" fails on the **2"**. The stack-up is board (8.2 mm) + cells (18.6 mm
diameter, two abreast) + holder base + gland bodies — and then the item that
actually decides it, the **U.FL pigtail's ~10 mm minimum bend radius**. RG178 is
semi-flexible and a sharp fold damages the shield. 50 mm of internal depth does not
leave room to route it. **70 mm does.**

The light colour deserves defending because the row understates the reason: the
constraint is **the cells**, which degrade above ~45 °C — not the electronics,
which are happy well past that. A dark box in full Cleveland August sun is
plausibly 20 °C over ambient, which puts a 30 °C day at the cell limit. Grey or
white, **and shaded regardless.**

### SR-04 / SR-08 — these two rows contradict each other

**PG7 clamps roughly 3–6.5 mm. Outdoor Cat5e is typically 5.5–6.8 mm, and
gel-filled or armoured runs 7–8 mm.** PG7 is marginal at best. **PG9 (~4–8 mm)
covers the whole range.** Get the OD off the actual SR-08 listing before ordering,
and buy an assorted PG7/PG9/PG11 pack as insurance — glands are pennies, a second
shipment is not.

**Gland count is also probably wrong.** Penetrations: sensor cable, DS18B20 lead,
SMA bulkhead. The DS18B20 **should share the Cat5e jacket** — 8 conductors, 3
spoken for, and a shared jacket means one gland and one hole instead of two. That
decision belongs in SR-04 and should be made before ordering.

### SR-05 — SMA vs RP-SMA is the highest-probability sourcing error on the list

LoRa uses **standard SMA**. WiFi uses **RP-SMA**. Most cheap U.FL pigtails are
RP-SMA because they are repurposed WiFi parts, and an RP-SMA bulkhead physically
will not mate with the SMA-male antennas already in hand.

**Bulkhead must be SMA FEMALE: outer thread, centre socket.** The ProBots
11 mm-thread part that surfaces first in searches is **RP-SMA** — right idea,
wrong gender, and its own listing makes the useful point that *most SMA connectors
have only 6 mm of thread, making them impossible to mount on outdoor waterproof
boxes.* Hence the extended-thread recommendation.

**Measure the SR-03 wall before ordering**, or take the 14.5 mm option and stop
thinking about it. The O-ring in that flange is doing real work on an outdoor box.

### SR-16 — the charger is a genuine gap, not a nice-to-have

HW-05 requires that paralleled cells be **charged to the same voltage before
joining**, because two cells at different SoC equalise through each other with no
current limit. **Nothing in this order can do that.** The board's TP4054 charges
the pack, and only once the cells are already joined — which is after the moment
that matters.

It also breaks the maintenance plan. After HW-07 was descoped, the entire
validation-and-service story is *"watch VBAT, swap cells on alert."* **Every swap
needs the same equalisation step.** Without a charger, "swap" means "buy two new
cells," which is not a rechargeable node.

Secondary but real: a charger with a capacity-test mode is the only way to confirm
the cells are what the label claims **before** they are sealed in a box up a tank.

### SR-17 — the pattern, not the parts

Three parts were named as required in other rows' `Why it matters` columns and
appear in no line item: **self-amalgamating tape** (SR-06), **a 4.7 kΩ 1-Wire
pull-up** (SR-09), and **heat-shrink for the outdoor splices** (SR-02, SR-08,
SR-09). Individually they are ~$25 of hardware-store stock. Collectively they are
a pattern: **constraint columns accumulated requirements that never became rows.**

The tape is the sharp case. It is the part that keeps water out of the antenna
connector, it is named only inside SR-06 — and **SR-06 turns out not to be a real
line item**, so striking that row would have taken the tape with it.

### What this round did differently, and what it implies for the next one

Rounds 1 and 2 found their best material — the 20 cm blind zone, the 14 cm
temperature error, the 3 mA TXD leak — by **researching an unfamiliar part
deeply**. SR-14 is written to expect the same mechanism again.

**It did not repeat.** Every finding this round came from **cross-reading rows
against each other and against §4**: SR-06 against SR-05, SR-07 against §4's
already-bought list, SR-04 against SR-08, SR-16 against HW-05, SR-17 against four
constraint columns. No datasheet produced a surprise; the parts are all well
understood by now.

⚠ **That is a different check, and the ledger has no mechanism for it.** SR-14's
own note already concedes it caught SR-15 *"only because a reviewer looked, not
because the mechanism worked."* This round is the second consecutive instance.
**A consistency pass — do rows contradict each other, and does every named
constraint have a line item — is worth making an explicit step before an order
goes out**, rather than hoping a catch-all row fires.

### The order, assembled

Deletions applied. **Ship-together where possible; cells ship alone by law.**

| From | Items | Approx |
|---|---|---|
| **DigiKey or Mouser** | SR-10 A02YYUW ×2, SR-02 SparkFun PRT-12899 ×2 (or ×4) | ~$60 |
| **Battery specialist** (18650batterystore / Illumn) — ⚠ **separate ground shipment, UN3480** | SR-01 protected NCR18650B ×2 | ~$30 |
| **Amazon / general** | SR-05 pigtail ×2, SR-09 DS18B20 ×4, SR-03 enclosure, SR-04 gland assortment, SR-11 JST 1.25 pack, SR-16 XTAR VC2SL, SR-17 tape + heat-shrink | ~$130 |
| **Local hardware** | SR-12 4" PVC + hole saw, SR-13 consumables, SR-15 hedge (optional) | ~$90 |
| | | **~$310** |

⚠ **Not a purchase order.** Per the 🔒 rule, none of this is orderable until CC
promotes it into `HARDWARE_BUILD_PLAN.md` §4. Prices are approximate and, where
noted in the rows, unconfirmed — **SR-10's price in particular was not retrievable
from the distributor pages.**

**Order-sequencing note that saves a shipment:** SR-04's gland size depends on
SR-08's cable OD, and SR-05's thread length depends on SR-03's wall thickness.
Neither is knowable from the rows as written. **Pick the cable and the enclosure
first, read their published dimensions, then order the glands and the pigtail** —
or buy the assortment and the 14.5 mm thread and make both questions moot for
about $8.

---

## 3.10 Round 3 closeout — what was actually procured (2026-08-21)

**The round specced 15 line items. Twelve closed without a purchase.** Nine were
already on the shelf, two were struck as errors, and one purchase supersedes three
rows. **Four things get bought.**

⚠ **This section records procurement state only.** It is not authority to order —
per the 🔒 rule that still comes from `HARDWARE_BUILD_PLAN.md` §4 after CC
promotes. Owner intends to place the order **after** promotion, and to record
actual ordered parts in the BOM at that time.

### Procurement state

| ID | Line item | State | Note |
|----|-----------|-------|------|
| SR-01 | Protected 18650 cells ×2 | `promoted` — superseded | → **DEC-006 amendment 2026-08-21**, replaced by pack |
| SR-02 | 2-cell holder, parallel | `promoted` — superseded | → **DEC-006 amendment 2026-08-21**, replaced by pack |
| SR-03 | IP65 enclosure | `cart` | Zulkit 150×100×70 mm, grey, hinged (inner 130×81×63) |
| SR-04 | Cable glands | `on hand` | Owner has assorted sizes ⚠ verify size against SR-08 OD at build |
| SR-05 | U.FL → SMA-F bulkhead pigtail | `on hand` | Shipped with the Heltec boards — the "IPEX Ver.1-SMA Wire: Yes" option was taken. ~6 in stock |
| SR-06 | SMA bulkhead | `struck` | Duplicate of SR-05 — not a separate part |
| SR-07 | 915 MHz antenna | `on hand` | Already in the $67.20 Heltec order — 3 whips, 2 radios |
| SR-08 | Sensor cable | `on hand` | ⚠ **Substitution — not Cat5e.** See below |
| SR-09 | DS18B20 probes | **`ordered` 2026-08-21** | Amazon — ⚠ **5 × 5 m, not the specced 4 × ~3 m.** Promoted to `HARDWARE_BUILD_PLAN.md` §4 as ordered. The longer lead moves the splice out of the condensing zone; it does **not** license a second lid penetration — the shared-jacket decision from SR-04/SR-08 stands. |
| SR-10 | A02YYUW ×2 | `cart` | DFRobot direct ⚠ see lead-time note |
| SR-11 | JST 1.25 pigtails | `cart` | Amazon |
| SR-12 | PVC + hole saw | `on hand` | |
| SR-13 | Consumables | `on hand` | |
| SR-15 | Enclosure mounting | `on hand` | Owner reports included |
| SR-16 | Li-ion charger | `promoted` — superseded | → **DEC-006 amendment 2026-08-21**, three-pack rotation replaces it |
| SR-17 | Tape / 4.7 kΩ / heat-shrink | `on hand` | |

**Four purchases: SR-03, SR-09, SR-10, SR-11.** Roughly $110.

### SR-08 — accepted substitution, with one caveat

**Not the specced direct-burial Cat5e.** Owner has 25 ft of **22 AWG 6-conductor,
UL 2464, stranded tinned copper, PVC jacket** left over from tinkle.

**Electrically this is better than the spec, and the row should be updated rather
than waived.** 22 AWG stranded tinned copper beats 24 AWG solid Cat5e here on
three counts: lower resistance, corrosion resistance at the splices, and — the one
that matters — **stranded survives flexing at a tank lid, where solid conductors
work-harden and break.** The original row's "solid copper not CCA" warning was
aimed at CCA; stranded tinned copper is a different and better answer.

**Conductor count works exactly:** sensor takes V+ / GND / TX, DS18B20 shares V+
and GND and adds DQ. **Four used, two spare, one gland.** The "does the DS18B20
share the jacket" question from SR-04 is answered: yes.

⚠ **The one real gap: UL 2464 is an indoor appliance-wiring rating with no
sunlight-resistance requirement.** The listing describes the cable as outdoor;
that is marketing copy, not the standard. A genuinely sun-rated jacket carries a
*Sunlight Resistant* mark. **Plain PVC chalks and cracks in a couple of seasons of
direct sun and stiffens in Cleveland winters.**

**Mitigation, ~$4: sleeve the exposed run in split loom or ½" flex conduit.** The
enclosure is shaded by design; the run up to the tank lid is not. This is a build
step, not a re-order — a jacket failure is visible long before the conductors go.

⚠ **Verify 25 ft covers tank height plus slack before cutting.**

### The two errors this round caught, restated for the BOM

Both delete line items rather than filling them, and both were discoverable from
inside this file:

- **SR-06 is not a part.** A U.FL→SMA-female *bulkhead* pigtail is itself the
  bulkhead. Ordering both yields two pigtails and two unusable connectors.
- **SR-07 was already bought**, per §4's own *already bought* list. Three
  antennas, two radios.

### Still `blocked` — HW-13

> ⚠ **Superseded in part, 2026-08-21.** The argument below — that a datasheet
> cannot close F-8 — was made in this round and **the operator ruled against it**:
> the manufacturer's stated 3.3–5 V range beats unsourced reseller copy, so **F-8
> is closed** (see the F-8 row in §3.2 and `HARDWARE_BUILD_PLAN.md` §9, which
> records both positions). **HW-13 itself is unaffected and still open** — it is a
> question about this specific part on this specific rail, and only the bench
> answers it. Everything below about *why it matters* still stands.

⚠ **HW-13 is NOT closed by the datasheet, and this was raised and corrected during
the round.** DFRobot has stated 3.3–5 V since Round 1; **F-8 exists because
multiple resellers recommend 5 V "for best performance"**, which is a claim about
behaviour at the bottom of the range, not about the published spec. A datasheet
cannot settle it. **Only the part can.**

**Why it still matters after F-6 was retired:** Vext outputs **3.3 V**, and Vext is
now what gates the sensor rail. If 3.3 V proves marginal, the fix is a discrete
switch sourced from **VBAT (3.4–4.2 V)** — reintroducing the P-FET that HW-11 just
deleted. **That is the consequence hiding behind this row.**

**Test, at build step 5:** sensor on 3.3 V, count checksum-valid frames over
several minutes. Two sensors are on order specifically so a marginal result can be
distinguished from a bad unit.

### Lead time — the one scheduling risk

**SR-10 is the long pole.** DFRobot ships from Chengdu; $5 flat, but **2–4 weeks**.
HW-13 cannot be tested until it lands, and HW-13 is the last open question in the
build. **DigiKey and Mouser both stock SEN0311 and ship same day domestically** for
a few dollars more. ⚠ **Recommend switching SR-10 to a domestic distributor** —
this is a schedule decision for the owner, not a spec change.

---

## 3.11 Pack decision (drafted here as a new id) — PROMOTED as a DEC-006 amendment, 2026-08-21

> ✅ **Promoted. It got no new id, and the draft below is now history.** The real
> text lives in `docs/decisions/DEC-006-protected-cells-a-firmware-cutoff-not-a-hardware.md`
> under *Amendment, 2026-08-21*, carrying `amends_spec` for `SPEC.md` §4.
>
> ⚠ **Why it got no id of its own.** The draft names itself a new decision *and* declares
> `Amends: DEC-006`. Under **DEC-S036** those cannot both be true: *"there is no
> new decision that amends an old one — if it changes what an existing decision
> decided, it is that decision, later."* The protocol's test is which decision
> would be **wrong** if you shipped this. DEC-006 says two protected cells in a
> holder; ship the pack and DEC-006 is wrong. So this is DEC-006, later. The
> reasoning below was adopted almost entirely — only the container changed.
>
> The margin arithmetic was **re-derived rather than taken**: 5200 × 0.70 = 3,640
> mAh against ~2,460 mAh over two years = **1.48×**, confirming the draft's
> "1.9× → 1.5×". At a 10-minute cadence the re-derivation gives **~1.1×** where
> the draft said "roughly 1.0×" — slightly less dire, same conclusion, and the
> draft erred toward caution.

Drafted for CC in `DECISIONS.md` house style. **Numbering was provisional** — CC
assigned none.

**Status:** proposed, 2026-08-21
**Supersedes:** SR-01, SR-02, SR-16
**Amends:** DEC-006
**Touches:** `SPEC.md` §4, `HARDWARE_BUILD_PLAN.md` §4

### Decision

Power the field node from a **single pre-built 1S2P 18650 pack — 3.7 V, 5200 mAh,
with an integrated protection circuit module** — instead of two loose protected
cells in a holder. **Buy three.**

### Context

Rounds 1–2 settled on two protected 18650 cells wired in parallel (DEC-006),
which required a holder (SR-02) and a charger (SR-16). Round 3 sourcing surfaced
that **the holder could not be closed by research**: no vendor publishes internal
bay length, and protected cells run ~69–70 mm against the ~65 mm most holders are
moulded for. SR-02's recommendation — two single-cell holders wired in parallel —
reduced but did not remove the fit risk.

A pre-built pack removes the problem at its root.

### Consequences

**Simplifications:**

- **SR-02 disappears.** No holder, no fit risk, and the series/parallel silent
  failure mode is gone — the pack is factory-wired 1S2P.
- **SR-16 disappears.** HW-05 required cells be equalised before paralleling; a
  factory pack is matched at build and never separated. The board's TP4054
  charges the pack over USB.
- **Smaller.** ~68 × 37 × 19 mm versus two holders side by side. Helps SR-03.
- **Better swap.** With a JST 1.25 crimped on, replacement is unplug-old /
  plug-new. **Three packs give a rotation:** one in service, one charged, one
  spare. This is the maintenance story that replaced the descoped PPK2 (HW-07).

**Costs — both real:**

- ⚠ **Capacity drops from ~6700 mAh to 5200 mAh.** Against the same ~2,400 mAh
  two-year draw and the same 70 % cold derating, **margin falls from ~1.9× to
  ~1.5×.**
- ⚠ **Cell provenance is unverifiable.** The rationale for buying loose cells from
  a specialist was that they *test* them. A consumer pack cannot be verified —
  and the capacity-test charger that could have verified it is the thing this
  decision deletes.

### DEC-006 amendment

DEC-006 required **protected cells + a 3.2 V/cell firmware cutoff**, on the
finding that the Heltec TP4054 provides no discharge-side protection.

**Both halves survive.** The pack's PCM satisfies the protection requirement — and
arguably better, since a pack-level PCM is designed around the pack rather than
around an individual cell. **The firmware cutoff is unchanged and remains the more
important half**, for the reason DEC-006 already records: the risk is a
stuck-*awake* node, which firmware catches and no hardware protection catches
early.

### SPEC §4 amendment — required, not optional

⚠ **The 15-minute wake interval stops being a preference and becomes a
constraint.** At 5200 mAh the 10-minute cadence lands at roughly 1.0× margin —
no margin at all. **`SPEC.md` §4 must record 15 minutes as a floor, with the
reason**, or a future session will "improve" the cadence and silently break the
two-year target.

### Rejected alternatives

- **Two protected NCR18650GA cells + two single-cell holders + XTAR charger**
  (the SR-01/02/16 path). More capacity and verifiable cells, but an unclosable
  fit risk, three line items, and a manual equalisation step before every swap.
- **DFRobot DFR0969 2-way 18650 holder** ($9.90). Rejected outright: it is a
  power-bank shield, not a holder. ⚠ **Its NORMAL mode automatically shuts down
  when output current is too low** — a node sleeping at ~16 µA reads as no load,
  so the module would cut power to it. Its boost converter, controller, and four
  status LEDs also draw milliamps continuously against a 20 µA budget.
- **Dantona L37A52-2-1-2W** 1S2P 5200 mAh. Correct topology, but no published
  price, no cart, no stated PCM, and a probable sales enquiry and MOQ.
- **Primary lithium (ER18650 Li-SOCl₂).** Raised and rejected. ⚠ Three
  independent blockers: **(a)** an almost flat discharge curve, which destroys
  VBAT telemetry as a validation path — and per **F-5** that is the *only*
  remaining validation path after HW-07 was descoped; **(b)** the TP4054 sits
  permanently across VBAT and would attempt to charge a non-rechargeable cell
  whenever USB is connected, which happens routinely during firmware work;
  **(c)** high internal resistance handles the ~120 mA LoRa TX pulse poorly,
  typically requiring a hybrid layer capacitor.
- **"10-year lithium" AA (Energizer Ultimate, LiFeS₂).** 1.5 V per cell — two in
  series is 3.0 V, at or below LDO dropout. Non-starter.

### Verification

Per **F-5**, VBAT-in-packet remains the sole validation path and this decision
makes it more load-bearing, not less — it is now also how an over-stated capacity
claim gets caught. **A garbage pack shows up in the discharge slope within a
couple of months**, at which point it is a ~$20 replacement rather than a
redesign.

### Open item for CC

⚠ **The pack's product listing is demonstrably unreliable**, which does not make
the pack bad but does mean **no number from it should be promoted into `SPEC.md`
as fact.** It claims a built-in TP4054 "stabilizes voltage output" (the TP4054 is
a charger IC — and is already the charger on the Heltec board), −40 °C to 60 °C
operation (Li-ion does not usefully discharge at −40 °C), 2C fast charging while
describing 0–80 % in 1.5 h (≈0.5C), and UL 2056 certification (the power-bank
standard, not a cell-pack standard). **Record 5200 mAh as a nameplate figure
pending VBAT confirmation.**

---

## 3.12 Build-step additions from Round 3

Small items that belong in `HARDWARE_BUILD_PLAN.md` §5–§8 rather than the BOM.

| # | Step | Why |
|---|---|---|
| B-1 | **Meter JP1 polarity on each board before connecting any pack** | §3.7 recorded polarity from **one sample of one board**. Not a datasheet guarantee. |
| B-2 | **Meter the pack pigtail polarity before plugging in** | Wire colour on a consumer pack is no more trustworthy than the rest of its listing. Cut the shipped connector, crimp SR-11 JST 1.25. |
| B-3 | **Label the LoRa U.FL socket (E2) with a paint pen before the first antenna** | F-11. Two identical sockets; the wrong one transmits into an unmatched load with no error. |
| B-4 | **Sleeve the exposed SR-08 run in split loom or ½" flex conduit** | UL 2464 PVC is not sunlight-rated. ~$4. |
| B-5 | **Test-fit before drilling anything** | Board + pack in the SR-03 enclosure, and the U.FL bend radius in particular. Inner depth is 63 mm; the coax turn is what consumes it. |
| B-6 | **Check SR-04 gland size against the actual SR-08 cable OD** | Glands are on hand in assorted sizes; the right one has not been identified. |
| B-7 | **Verify 25 ft of SR-08 covers tank height plus slack before cutting** | Only one cut is free. |
| B-8 | **Check the SR-03 moulded knockout thread before drilling** | Determines which on-hand gland fits without a new hole. |
| B-9 | **Confirm SR-05 thread length against the SR-03 wall** | The on-hand pigtails shipped for Heltec's own thin plastic shell. If the nut will not reach, a 14.5 mm-thread pigtail is ~$12 — a part, not a redesign. |

---

## 4. Brief to paste into chat — **Round 3 (sourcing)**

> ⚠ **Round 3 breaks the "self-contained brief" pattern on purpose — hand over the
> whole file, not an excerpt.** Rounds 1 and 2 wrote every question out in full
> here, so §4 worked alone. This round's questions are **fourteen line items in the
> §3.8 table**, and reproducing them here would put the same fourteen rows in two
> places, where they would drift the first time one was answered. So the brief below
> is context *for* §3.8, not a replacement for it: an excerpt-only handover leaves
> six rows (SR-03, SR-04, SR-08, SR-09, SR-12, SR-13) unanswerable, which is the
> exact failure that opened this round.
>
> Round 2's brief is superseded and preserved verbatim in §4.1.

---

I'm sourcing the parts for a battery-powered wireless sensor node on a farm rain
tank. **The research is done** — two prior rounds settled every part *type*. I need
**specific products, vendor links, and current prices**, not design advice. Please
answer by ID from the **SR-01 … SR-15 table in §3.8 of this file**, and for each
give the product, the vendor link, the price, and the one spec that proves it meets
the constraint stated for that row. If you cannot see that table, you were handed an
excerpt rather than the whole file — say so and ask for the file, because six of the
rows appear nowhere else.

**The build, in one paragraph.** One field node in the lid of a rain tank measures
water level with an ultrasonic sensor and reports over LoRa every 15 minutes to a
second identical board tethered by USB to a Linux server. Battery only, no solar,
no mains anywhere near it, ~2 years between swaps. Read-only telemetry — it never
actuates anything.

**Already bought, do not re-source:** 3× Heltec Wireless Stick Lite V3 and 3×
915 MHz whip antennas ($67.20 delivered). The boards are in hand and working.

**Already settled — please do not re-derive, re-litigate, or improve on these:**

- The **A02YYUW** ultrasonic sensor: 60° beam cone, 3 cm blind zone, 100 ms
  response, median-of-5–7 read strategy.
- **LoRa, not WiFi.** WiFi was raised and rejected on the battery budget; there is
  no mains power at the tank.
- **Protected cells plus a 3.2 V/cell firmware cutoff** instead of a hardware
  low-voltage-cutoff board.
- **A DS18B20 in the tank headspace** — required, not optional; speed of sound
  moves 0.176 %/°C, which is ~14 cm of apparent level error across the seasonal
  range.
- **15-minute cadence.** All unit conversion happens on the server, not the node.
- **U.FL→SMA pigtail to a bulkhead**, antenna outside on the bulkhead.
- **3-conductor Cat5e for the sensor run, unshielded.**

**Three things that will otherwise get sourced wrong:**

1. **Protected cells are longer than bare ones.** The holder (SR-02) must seat the
   cells you pick for SR-01. Quote both lengths. This is the single most likely
   thing to arrive and not fit.
2. **Do not source 2×20 pin headers.** An earlier round said they were a gap. They
   are not — the sensor wires solder directly to the board pads, nothing is
   breadboarded, and the owner has header stock regardless.
3. **Do not source a P-FET load switch.** An earlier round had one in the BOM
   because the board's switched `Vext` rail was believed unusable. **It was
   measured on 2026-08-20 and it is fine** — 3.3 V on, 3.0 mV off. The sensor rail
   gates off Vext directly, so no discrete switch.

**One result you can rely on, and it should stop you over-speccing the antenna.**
A field range test was run on 2026-08-19: 20 packets sent, 19 received, across the
tank, the property corner and a tunnel. RSSI sat at −82 to −89 dBm everywhere with
about −3 dB SNR, against a LongFast decode floor near −20 dB. **The edge of the
link was never found — the property ran out first.** So a 3–5 dBi omni is amply
sufficient; nothing here needs a high-gain fiberglass stick, and a high-gain
antenna would actively hurt by flattening the vertical pattern between two ends at
different heights. (Recorded in this repo as `DEC-009` and issue #41, neither of
which you can see — that is why it is stated here.)

**Format that makes this mergeable:** one line item per ID, product + link + price
+ the proving spec. If something is out of stock or the price looks wrong, say so
and give the alternative rather than picking silently. If assembling the order
surfaces a part nobody listed, add an SR-ID for it and answer it — both prior
rounds found their most valuable items exactly that way.

---

## 4.1 Superseded — brief to paste into chat, **Round 2**

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

⚠ **But if chat has to ask for one of these, the brief is underwritten — fix the
brief.** Round 3 opened with chat correctly refusing to source anything, because
§4's brief was still Round 2's and named no line items. The instinct is to paste
`HARDWARE_BUILD_PLAN.md` §4 across; the right move was to write the round's brief
so the whole-file handover works as designed. **Pasting is for genuine one-offs, not
for patching a round that was opened without a brief.** The same rule kills the
temptation to point chat at a decision record: `DEC-009` is not readable from a
chat window, so anything a round needs from it gets **stated in the brief**, with
the DEC cited beside it for whoever reads this repo later.

---

## 7. Round log

| Round | Opened | Topic | Closed |
|-------|--------|-------|--------|
| 1 | 2026-08-07 | Tank node hardware — HW-01…HW-08, plus HW-09 (blind zone) and HW-10 (temperature) raised in chat | **2026-08-08 — all 10 `promoted`.** CC review in §3.3; math re-derived and confirmed; five corrections logged. Yielded DEC-006, DEC-007, and four new open items. |
| 2 | 2026-08-08 | **Board change to Stick Lite V3** (HW-15…HW-18) + loose ends (HW-11…HW-13). HW-14 is a meter check on receipt, not a chat question. Brief in §4. | **2026-08-20 — closed.** All nine research rows `promoted`. The bench session (§3.7) then measured HW-05, HW-14 (GPIO36 half) and HW-18: **Vext switches to 3.0 mV, so HW-11 is closed and the P-FET leaves the BOM.** CC review in §3.6. |
| 3 | 2026-08-21 | **Sourcing — SR-01…SR-17.** Rounds 1–2 answered *what kind of part*; this one answered *which SKU, from whom, at what price*. Ledger in §3.8, detail §3.9, procurement §3.10, brief in §4. | **2026-08-21 — closed, all `promoted`.** Yielded a **DEC-006 amendment** (1S2P pack replaces cells + holder + charger), a `SPEC.md` §4 amendment (15 min becomes a **floor**; three component rows corrected), nine build steps (B-1…B-9), and **two struck rows** — SR-06 was never a part, SR-07 was already bought. **Eleven of fifteen rows closed without a purchase; four things get bought.** ⚠ **Both errors were internal to this file and neither came from research** — see the note below. |

### ⚠ The check this ledger still has no mechanism for

Rounds 1 and 2 found their best material — the 20 cm blind zone, the 14 cm
temperature error, the 3 mA TXD leak — by **researching an unfamiliar part
deeply**. SR-14 is written expecting that mechanism to repeat.

**It did not.** Every finding in Round 3 came from **cross-reading rows against
each other**: SR-06 against SR-05, SR-07 against §4's own already-bought list,
SR-04 against SR-08, SR-16 against HW-05's equalisation requirement, SR-17 against
four different constraint columns. **No datasheet produced a surprise** — by the
third round the parts are well understood, and the errors had migrated into the
ledger itself.

That is a different check, and **nothing here performs it.** SR-14's own note
already conceded it caught SR-15 *"only because a reviewer looked, not because the
mechanism worked"*; Round 3 is the second consecutive instance, and this time the
things it missed were **two line items that would have been ordered.**

**Before the next order goes out, run a consistency pass explicitly:** do any two
rows contradict each other, and does every constraint named in a `Why it matters`
column have a line item of its own? Both of this round's struck rows and all three
of SR-17's missing parts were reachable by exactly that question, from information
already written down. Do not wait for a catch-all row to fire.
