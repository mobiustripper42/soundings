---
id: DEC-006
title: "Protected cells + a firmware cutoff, not a hardware LVC board"
topic: "Hardware — board, power & enclosure"
amends_spec:
  - section: "4"
    scope: "the per-node component list — the cell line, plus the enclosure size, gland size and SMA-bulkhead rows that Round 3 sourcing corrected — and the 15-minute cadence, which becomes a floor rather than a preference; the firmware-cutoff half of this decision is unchanged"
---

## DEC-006: Protected cells + a firmware cutoff, not a hardware LVC board

**Decision:** Field nodes use **protected 18650 cells** (~$2/cell premium) plus a **3.2 V/cell cutoff in firmware**. The ~$3 low-voltage-cutoff board in `SPEC.md` §4's BOM is **dropped**. The Nordic PPK2 (~$90–110) flagged in the original build plan is **not purchased**; a plain multimeter covers the check that matters.

**Why:**
- **The board offers no protection to remove.** The V3's charge IC is a **TP4054** — a linear charger with CHRG/GND/BAT/VCC/PROG and no discharge-side FET ([V3.1 schematic](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.1_Schematic_Diagram.pdf)). Heltec advertises "charge and discharge management"; the schematic supports charge only. So *something* is needed — the question is only which thing.
- **Firmware catches the failure mode that actually occurs, earlier.** At ~16 µA a node stuck *asleep* has decades of margin on 6000 mAh; over-discharge by sleeping is not a real risk. The risk is a node stuck *awake* at ~40 mA from a firmware bug. A 3.2 V/cell firmware cutoff sees that coming and reports it in telemetry; a hardware LVC cuts the rail hard, silently, after the fact.
- **Protected cells are the backstop for everything firmware can't see** — a genuinely hung MCU, a wiring fault. They cover the residual for less than the board they replace.
- **The measurement worth having is milliamp-scale, and a multimeter reads it.** The design errors that matter — SX1262 not slept, OLED left on, SPI pins floating — cost 1–40 mA. A PPK2's resolution only buys optimisation *within* the µA band, and the ~1.8× power-budget margin says there is nothing there to win. Long-run validation comes from the `battery_mv` already in every packet header (`contracts/packet-v1.md:45`) plus a server-side alert at 3.4 V/cell.

**Tradeoff:** Over-discharge protection now depends partly on code, which is the thing most likely to have a bug in it — the exact inversion of what a hardware LVC is for. Accepted because protected cells hold the hard floor regardless of what the firmware does, and because the alternative buys a slower, blinder version of the same guarantee. Second cost: without a µA-resolution instrument, the 16 µA figure is taken from a cited third-party measurement rather than confirmed on our own bench. The multimeter check confirms the *order of magnitude*, which is what the design turns on.

**Rejected:** the SPEC's LVC board (redundant against protected cells, and blind to the stuck-awake case); the PPK2 (precision the margin doesn't need); and doing neither (the TP4054 leaves a real gap).

**Confirmed 2026-08-08 against the board actually being bought (HW-16).** The Wireless Stick Lite V3 carries the same `U3 TP4054` with the same pins and the same absence of a discharge-side FET, and its product page repeats the same "charge and discharge management" claim the schematic does not support. **This decision stands unchanged.** Round 2 also supplied direct evidence for the multimeter-over-PPK2 half of it: HW-20 found a **3 mA** leak on a floating U0TXD — roughly 150× the sleep budget, and trivially visible on a DMM. Nothing found in either round needed µA resolution to catch.

**Revisit:** If Soundings grows to three or more distinct node designs, a PPK2 starts to amortise and the sleep-current question gets asked once per design instead of once ever. Also revisit if bench measurement shows sleep current an order of magnitude off the cited 16 µA — that would mean the budget, not just the instrument, needs rework.

---

## Amendment, 2026-08-21 (eric) — a pre-built 1S2P pack replaces the loose cells, the holder and the charger

**What this changes, and what still stands.** The **form** of the cell changes: a single pre-built **1S2P 18650 pack, 3.7 V, 5200 mAh nameplate, with an integrated protection circuit module**, replaces two loose protected cells in a holder. **Buy three.** Both of this decision's original legs survive — the pack's PCM satisfies the protection requirement, and **the 3.2 V/cell firmware cutoff is unchanged and remains the more important half**, for exactly the reason recorded above: the risk is a node stuck *awake*, which firmware catches and no hardware protection catches early. The LVC board stays dropped. The multimeter-over-PPK2 holding is untouched.

**Why it changed: the holder could not be closed by research.** Round 3 sourcing established that protected cells run **~68.9–69.5 mm** where bare cells are 65 mm — the protection PCB and button top add ~4–5 mm — and that **no holder vendor publishes an internal bay length.** The only evidence available is warnings: one parallel 2-cell holder states it fits ~65 mm cells and *"may not fit longer protected cells."* The intermediate fix (two single-cell holders wired in parallel) reduced the fit risk and did not remove it, because single holders do not publish a bay length either. A factory pack removes the problem at its root rather than managing it.

**What it also deletes.** The holder, and the separate Li-ion charger that had just been identified as a missing part. That charger existed to satisfy this decision's own requirement that paralleled cells be **equalised before joining** — two cells at different state of charge dump unlimited current into each other. A factory pack is matched at build and never separated, so the requirement is met by construction and the board's TP4054 charges the pack over USB. **The three-pack rotation** — one in service, one charged, one spare — is now the maintenance story that replaced the descoped PPK2.

**Two costs, both real, both accepted:**

- ⚠ **Margin falls from ~1.9× to ~1.48×.** Re-derived rather than taken: 5200 mAh × 0.70 cold derating = 3,640 mAh against the same ~2,460 mAh two-year draw. **This is what makes the 15-minute cadence a floor rather than a preference** — at 10 minutes the wake term grows from ~1,750 to ~2,625 mAh and the margin lands near **1.1×**, which is no margin at all. `SPEC.md` §4 now records that as a constraint, because otherwise a later session "improves" the cadence and silently breaks the two-year target.
- ⚠ **Cell provenance becomes unverifiable.** Buying loose cells from a specialist was justified partly because they *test* them. A consumer pack cannot be verified — and the capacity-test charger that could have verified it is the thing this amendment deletes. Accepted because **F-5's `battery_mv` telemetry is the check**: an over-stated capacity claim shows up in the discharge slope within a couple of months, at which point it is a ~$20 replacement rather than a redesign. This makes VBAT-in-packet more load-bearing, not less.

⚠ **No number from the pack's product listing may be promoted as fact.** That listing claims a TP4054 "stabilizes voltage output" (it is a charger IC, and it is already the charger on the Heltec board), −40 °C to 60 °C operation (Li-ion does not usefully discharge at −40 °C), 2C fast charging while describing 0–80 % in 1.5 h (≈0.5C), and UL 2056 certification (a power-bank standard, not a cell-pack one). **5200 mAh is recorded as a nameplate figure pending VBAT confirmation.**

**Rejected:**

- **Two protected NCR18650B/GA cells + two single-cell holders + an XTAR VC2SL charger.** More capacity (~6,800 mAh) and verifiable cells, but an unclosable fit risk, three line items instead of one, and a manual equalisation step before every swap.
- **DFRobot DFR0969 2-way 18650 holder.** Rejected outright — it is a power-bank shield, not a holder. ⚠ Its normal mode **automatically shuts down when output current is too low**, and a node sleeping at ~16 µA reads as no load, so the module would cut power to the thing it is powering. Its boost converter, controller and status LEDs also draw milliamps continuously against a 20 µA budget.
- **Dantona L37A52-2-1-2W** 1S2P 5200 mAh. Correct topology, but no published price, no cart, no stated PCM, and a probable sales enquiry and MOQ.
- **Primary lithium (ER18650 Li-SOCl₂).** Three independent blockers: its almost-flat discharge curve destroys VBAT telemetry, which per F-5 is the *only* remaining validation path; the TP4054 sits permanently across VBAT and would attempt to charge a non-rechargeable cell whenever USB is connected, which happens routinely during firmware work; and its high internal resistance handles the ~120 mA LoRa TX pulse poorly.
- **"10-year lithium" AA (LiFeS₂).** 1.5 V/cell, so two in series is 3.0 V — at or below LDO dropout. Non-starter.

**Revisit:** if VBAT telemetry shows the pack delivering materially under nameplate, or if a node fails to make it through a first winter. Either would mean going back to loose tested cells and solving the holder fit physically — which is a known, bounded problem, just a more tedious one.

---
