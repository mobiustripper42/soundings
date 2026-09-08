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
revisit_if: "A sensor needs bench setup anyway, or another sensor wants this shape."
---

## DEC-015: The node provisions its own sensor configuration, once, and verifies it

See also DEC-014, which chooses 9-bit. This is how 9-bit gets onto the part.

Two alternatives lost. Writing the config every read costs ~3.5 ms of awake time
forever to avoid one write. A bench step needs the write path in firmware anyway, for
a replacement probe, so it buys a ritual and a reflash dance on top of code that
exists regardless.

Detection is stateless. The driver reads the config byte — free, it rides the
scratchpad read already done — and asks whether the part is at 9-bit. A reading at the
wrong resolution still publishes un-faulted: it is finer data, and what it signals is
awake time, not measurement.

Verification cycles the rail rather than reading back, since a refusing part accepts
Copy Scratchpad in silence. It is **diagnostic only** — whether the rail truly falls
below the part's reset threshold depends on capacitance the firmware cannot see.

So the bound is one attempt per boot, marked **before** the write. Code review caught
the reverse: a lying verify left the flag clear and every real wake rewrote the part —
the 50,000-write budget gone in seventeen months, through the mechanism meant to
protect it.

A failed write is invisible in the field — not a channel fault, and no status field
exists. Issue #98.

---
