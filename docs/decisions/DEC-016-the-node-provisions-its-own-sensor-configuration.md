---
schema: 1
id: DEC-016
title: "The node provisions its own sensor configuration, once, and does not verify it"
topic: "Firmware architecture"
status: "active"
date: "2026-09-10"
ruling: "The node reads the DS18B20's config byte every wake and writes 9-bit to its EEPROM once, bounded to one attempt per boot by a flag marked before the write. No verify, no bench step."
claims:
  - kind: "file"
    target: "firmware/src/core/ds18b20.cpp"
  - kind: "test"
    target: "firmware/test/test_ds18b20/test_ds18b20.cpp"
supersedes:
  - DEC-015
revisit_if: "A part appears that reports EEPROM status, or a status channel makes a failed write visible in the field."
---

## DEC-016: The node provisions its own configuration, and does not verify it

Supersedes DEC-015, which kept all of this plus a verify. The verify is gone; it never
worked.

It cycled the rail and re-read the config, since the scratchpad reloads from EEPROM on
power-up. That rested on Ve falling below the part's reset threshold within 50 ms — a
seed nobody had measured. Measured 2026-09-09, by parking a marker in the scratchpad and
power-cycling: Ve does not collapse until between 50 and 100 ms. So the part never
restarted, handed back the scratchpad just written, and reported success unconditionally.
A check that cannot fail is not a check, and this one also cost a second rail cycle on
every heal wake.

Nothing depended on it. DEC-015 had already moved the endurance bound onto marking the
flag **before** the write, precisely because the verify could not be trusted. That bound
is now the whole mechanism: a refusing part is written at most once per boot.

The rest of DEC-015 stands — stateless detection off the config byte, a wrong-resolution
reading still publishing un-faulted, no bench ritual.

A failed write stays invisible in the field. Issue #98.

---
