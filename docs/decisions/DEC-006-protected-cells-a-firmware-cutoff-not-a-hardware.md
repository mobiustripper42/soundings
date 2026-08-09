---
id: DEC-006
title: "Protected cells + a firmware cutoff, not a hardware LVC board"
topic: "Hardware — board, power & enclosure"
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
