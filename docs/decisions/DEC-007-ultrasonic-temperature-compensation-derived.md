---
id: DEC-007
title: "Ultrasonic temperature compensation, derived gateway-side"
topic: "Sensors & calibration"
---

## DEC-007: Ultrasonic temperature compensation, derived gateway-side

**Decision:** The tank node carries a **DS18B20 in the tank headspace** as a required sensor, and transmits **raw distance and raw temperature counts**. The **gateway** applies `d_corrected = d_raw × c(T) / c(T_ref)` (where `c = 331.3 + 0.606·T` m/s) before the volume curve. Raw distance, raw headspace temperature, and derived gallons are all published. The headspace DS18B20 uses existing channel bit 4 (`SOIL_TEMP_0`); **no new channel bit is spent.**

**Why:**
- **It is the dominant error term, by an order of magnitude.** The speed of sound moves ~0.176 %/°C. Over a 2 m headspace, 0 °C→40 °C is **14.1 cm of apparent level change with no water moving** — against a sensor specified to ±1 cm, and larger than the blind-zone correction (HW-09) that had been masking it. In a sun-exposed tank headspace that range is the year, not an extreme.
- **The sensor does not do it for us.** DFRobot's spec table lists no temperature compensation and their own FAQ says to implement it yourself. One reseller claims otherwise; the manufacturer wins.
- **Gateway-side keeps it re-revisable, which is the whole architecture.** Same argument as DEC-004 and the volume curve: the correction can be refined later — humidity has a smaller but real effect — and **re-derived against years of stored raw without touching a node sealed in a tank lid.** On-node derivation would bake a coefficient into firmware in the least accessible place on the farm.
- **Bit 4 is the right home despite the name.** `SOIL_TEMP_0` encodes "DS18B20 raw, i16, 1/16 °C" — exactly what this sensor produces. Only the `SOIL` prefix is wrong, and the gateway's node→location map (D7) labels it correctly downstream. Spending one of the three remaining reserved bits (`contracts/packet-v1.md:89`) on a naming preference would be poor economy against the 16-channel ceiling.

**Tradeoff:** A single sensor measures one point in an air column that stratifies, so the correction is approximate — worst when the tank is near-empty and the headspace is tall. Accepted: it converts a ~14 cm error into a low-single-digit-cm one, and a second headspace sensor is a cheap refinement if the residual ever appears in the data. Second cost: a channel whose registry name doesn't match its physical use, which is a readability tax on anyone reading the contract cold. Mitigated by documenting it in `docs/tank-level-sensor.md`; a documentation-only rename to `DS_TEMP_0` is a cheap future tidy-up not worth doing on its own.

**Rejected:** no compensation (a 14 cm error dwarfs every other term); on-node correction (forfeits re-revisability, contra DEC-003/DEC-004); a new channel bit (spends scarce registry space to fix a name).

**Amended 2026-08-08 (HW-19).** The tradeoff above called the stratification residual "approximate." That undersold it: the bias is **signed and seasonal, not noise** — a sun-loaded lid sits 15–20 °C above the air over the water, so a lid-mounted probe reads the top of the gradient and **over-corrects**, and a median across samples cannot remove it. It is worst precisely when the headspace is tallest, which is the empty-tank case this sensor exists to protect. Mitigation is placement (thermally isolate from the lid; hang the probe partway down) plus an empirically fitted gradient model — which, because derivation is gateway-side, can be calibrated against manual dip readings and re-derived over stored history **without opening a tank**. That promotes the gateway-side choice from tidy to load-bearing. Probe *accuracy* is nearly irrelevant by comparison: 1 °C of probe error is 3.5 mm at a 2 m path.

**Revisit:** If the fitted gradient model still leaves visible bias, add a second headspace sensor and interpolate. If humidity turns out to matter at this precision, it re-derives from stored raw — no hardware change.

---

*Settled choices recorded as `[settled]` in SPEC (read-only V1, LoRa
point-to-point not LoRaWAN, no solar, USB-flash not OTA, software-first build)
may graduate to their own DEC entries here if their reasoning needs preserving.
For now they live in SPEC §2–§3.*
