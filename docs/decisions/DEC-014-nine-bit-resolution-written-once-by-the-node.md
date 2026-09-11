---
schema: 1
id: DEC-014
title: "The headspace probe runs at 9-bit, and the node masks the undefined bits"
topic: "Sensors & calibration"
status: "active"
date: "2026-09-07"
ruling: "The DS18B20 runs at 9-bit resolution. The driver masks the low three bits of the temperature register, which the datasheet declares undefined at that resolution."
claims:
  - kind: "file"
    target: "firmware/src/core/ds18b20.cpp"
  - kind: "test"
    target: "firmware/test/test_ds18b20/test_ds18b20.cpp"
revisit_if: "Headspace correction needs better than a 0.5 C step, or awake time stops being the binding power constraint."
---

## DEC-014: The headspace probe runs at 9-bit, and the node masks the undefined bits

See also DEC-007, which settles that this sensor exists, that the speed-of-sound
correction is applied gateway-side and that it rides channel 4. It says nothing about
on-node resolution.

12-bit conversion holds the node awake ~750 ms against 9-bit's ~94 ms, and
`HARDWARE_BUILD_PLAN.md:611-614` is explicit that total awake time is the term to
optimise, not sensor milliseconds. The cost is a 0.5 C step, which over a 2 m path is
1.76 mm of residual against the 141 mm of apparent level change being corrected.

At 9-bit the datasheet declares bits 2, 1 and 0 of the temperature register
**undefined** — not zero. This record exists partly because the opposite was written
into the issue first and would have shipped. `derive.py:147` divides the raw value by
16 and trusts it, so garbage left in those bits is up to 0.4375 C of invented
temperature riding the wire as though measured. The node masks them, and the gateway
needs no change — but only because the node now guarantees what the gateway already
assumed.

Masking follows the resolution the part is actually at, read from the config byte,
rather than the one requested. A sensor still at its 12-bit factory default has every
bit real, and masking it would discard precision to enforce a setting that has not
taken yet.

---
