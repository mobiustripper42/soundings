---
id: DEC-010
title: "Modem parameters, and the downlink that carries them back"
topic: "Radio, wire contract & gateway"
---

## DEC-010: Modem parameters, and the downlink that carries them back

**See also DEC-009**, which settled that the link is LoRa with SX1262 at both ends and the gateway radio hangs off the server, and which explicitly left this open: *"When issue #48 picks real radio parameters, that is a new question about spreading factor and airtime — not a re-run of this one."* **See also DEC-003**, the packet contract this rides inside; nothing here changes it.

**Decision:** the node↔gateway modem runs **SF10, 250 kHz bandwidth, coding rate 4/5**, explicit header, CRC on, 8-symbol preamble, private sync word. **TX power is configuration, not a constant** — 17 dBm in the field, 2 dBm on the bench. The link is **bidirectional**: the node holds a short receive window after each transmit, and that window is what every future downlink rides on.

### The airtime table, which is the actual content of this decision

Computed from the Semtech LoRa time-on-air formula for a 16-byte tank packet and a 46-byte worst case (`packet.h:23`), CR 4/5, 8-symbol preamble, explicit header, CRC on:

| SF | BW | 16 B (tank) | 46 B (max) | |
|---|---|---|---|---|
| 7 | 250 kHz | 26 ms | 46 ms | |
| 8 | 250 kHz | 46 ms | 82 ms | |
| 9 | 250 kHz | 82 ms | 154 ms | |
| **10** | **250 kHz** | **165 ms** | **288 ms** | **chosen** |
| 11 | 250 kHz | 289 ms | 535 ms | 46 B exceeds 400 ms |
| 12 | 250 kHz | 659 ms | 1151 ms | both exceed 400 ms |

**Why:**

- **Airtime is not the binding constraint, and discovering that inverted the choice.** The wake is budgeted at ~2 s (`HARDWARE_BUILD_PLAN.md:474`) and is dominated by MCU-on time, not by the radio. The difference between the fastest and slowest usable setting in that table is about 10 % of one wake. There is nothing worth buying with speed — so the budget goes to margin instead. The task was specced expecting the opposite, with `t_active > 2 s` (`:812`) as the risk to design around; the arithmetic said it was not in play.
- **SF10 is the highest spreading factor whose *worst-case* packet still fits 400 ms.** SF11 fits a tank packet at 289 ms and blows the limit at 535 ms for a full one. Choosing on the typical packet rather than the largest one is how a limit gets discovered by the one node that declares every channel.
- **It leaves roughly 8 dB in the worst measured location.** DEC-009 logged SNR between −0.75 and −7.0 dB at every location a node is going, on LongFast. SF10's demodulation floor is about −15 dB, so the worst of those readings retains ~8 dB and the typical retains ~14. LongFast held ~14.5 dB worst-case; this spends about 6 of it and keeps a real reserve for winter, wet foliage, and a receiver that will not be a phone in someone's hand.
- **17 dBm rather than the PA's full 22.** With 8 dB of margin there is no reason to run flat out, and TX energy is a small term beside the wake regardless. It is configuration precisely so this can be revisited from a measurement rather than a rebuild.
- **2 dBm on the bench, and that is not a nicety.** Both boards sit ~6 ft apart on `bee-grace` during bring-up. At full power that close the receiver is saturated — decodes fail, and the symptom is indistinguishable from a framing bug or a modem mismatch. A hardware problem wearing a software costume is the expensive kind.

**The receive window is part of this decision, not a later addition.** The node is deep asleep 99 % of the time, so the gateway can never reach it on demand; the only moment it can be told anything is one it offers itself. Holding a short window immediately after each transmit costs roughly 200 ms of receive current per wake and makes the link bidirectional for everything downstream — starting with the OTA trigger in issue #76, which cannot exist without it. Building the window now and finding a use for it later is cheaper than retrofitting it into a sealed node.

**Tradeoff, and what this decision does *not* establish:**

- **⚠ The 400 ms figure is FCC 15.247's maximum dwell per channel, and whether it binds us is a question this decision does not settle.** It is the number the LoRa world designs to. A single-channel, non-hopping 250 kHz node does not meet the ≥500 kHz digital-modulation clause that would exempt it from hopping rules, and reconciling those two is a regulatory judgment nobody on this project is qualified to make. It is adopted as a **conservative design constraint**, not as a compliance claim. In practice this is one node transmitting 288 ms every 15 minutes — a duty cycle near 0.03 % — on private land. If that reading is ever shown to be wrong in either direction, SF11 becomes available and the table above is what to re-read.
- **⚠ The gateway→node direction has never been measured.** Every number in DEC-009 is receive-side at the gateway. The downlink is asserted to work because the path is reciprocal and both ends are the same silicon, which is a good argument and not a measurement. The first thing bench step 6 should establish after a packet decodes is that a reply reaches the node.
- **The margin figures are inferred across a modem change.** The SNR readings were taken on LongFast (SF11/250k); path loss does not care about spreading factor, so they characterise the site rather than the configuration. Comparing them against SF10's floor is sound, but it is arithmetic on someone else's measurement, not a measurement of ours.
- **Sync word is private, which means no interoperability with anything.** Deliberate — there is no fleet to join and a public sync word invites decoding other people's traffic and having them decode ours.

**Rejected:** SF11 or SF12, which buy sensitivity nobody needs at a max-packet airtime past the limit; SF7–SF9, which buy speed against a budget where speed is worth nothing; 125 kHz bandwidth, which gains ~3 dB and doubles every figure in the table for no reason once the margin is already sufficient; a fixed TX power, which would have meant either a saturated receiver on the bench or a rebuild to test.

**Revisit:** if a node site fails to close, move up the table — SF11 is one step and costs the dwell margin. If the downlink proves unreliable while the uplink is fine, that is an asymmetry worth measuring rather than designing around, and DEC-009's fallback ladder is intact. If the dwell question is ever answered authoritatively, re-read the table.
