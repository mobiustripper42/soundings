---
schema: 1
id: DEC-015
title: "The node provisions its own sensor configuration, once, and verifies it"
topic: "Firmware architecture"
status: "active"
date: "2026-09-07"
ruling: "The node reads the DS18B20's config byte every wake and writes 9-bit to its EEPROM once, verified by a rail cycle and bounded to one attempt per boot. No bench step."
claims:
  - kind: "file"
    target: "firmware/src/core/ihealattemptflag.h"
  - kind: "file"
    target: "firmware/src/esp32/main.cpp"
  - kind: "test"
    target: "firmware/test/test_ds18b20/test_ds18b20.cpp"
revisit_if: "A sensor needs bench setup anyway, or a second sensor type wants this shape."
---

## DEC-015: The node provisions its own sensor configuration, once, and verifies it

See also DEC-014, which chooses 9-bit. This is how 9-bit gets onto the part.

Two alternatives lost. Writing the config every read costs ~3.5 ms of awake time
forever to avoid one write. A bench step per sensor needs the firmware to keep the
write path anyway, for a replacement probe, so it buys a ritual and a
flash-run-reflash dance on top of code that exists regardless.

Detection is stateless. The driver never asks whether a sensor is new; it reads the
config byte — free, it rides the scratchpad read already done — and asks whether the
part is at 9-bit. A reading at the wrong resolution still publishes un-faulted: a
12-bit value is finer data, and the problem it signals is awake time, not measurement.

Verification cycles the rail rather than reading back. A refusing part accepts Copy
Scratchpad in silence; the scratchpad reloads from EEPROM on power-up, so the cycle is
what separates "we sent it" from "it kept it".

The bound is one attempt per boot, in RTC memory. Confirming success removes any need
to count failures; the config register's 50,000 writes would otherwise be gone in
seventeen months.

A failed write is invisible in the field — not a channel fault, and no status field
exists. That gap is issue #98.

---
