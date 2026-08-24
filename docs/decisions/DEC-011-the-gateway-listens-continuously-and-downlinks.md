---
id: DEC-011
title: "The gateway listens continuously, and downlinks describe state rather than command it"
topic: "Radio, wire contract & gateway"
---

## DEC-011: The gateway listens continuously, and downlinks describe state rather than command it

**See also DEC-010**, whose airtime table this is a consequence of and which carries the measured receive window; its 2026-08-24 amendment is the evidence behind the first half of this. **See also DEC-009**, which put the gateway radio on the server. Nothing here changes a modem parameter.

**Decision:** the gateway runs **continuous receive** — `startReceive()` once, then non-blocking `poll()` — while the node keeps **bounded receive with a timeout**. `IRadio` carries both modes. The half-duplex **re-arm invariant belongs to the driver, not the caller**. And every downlink bit must describe **a state the node reports back**, never an action it performs.

### Two receive modes, because the two ends want opposite things

`IRadio::receive(buf, cap, timeoutMs)` was added in 3.9b **for the node**, and its own comment says so: *"The node has nothing else to do inside its window."* The timeout is not incidental — it is the safety property. A node that listens forever is a node stuck awake, which DEC-006 identifies as the actual battery killer.

The gateway then used that call because it was the only one there. **That is a seam that had not yet grown a second mode, not a misuse of the first.** It cost 78% of uplink packets (DEC-010's amendment) and the cost was invisible, because a self-truncated packet fails CRC exactly like a corrupted one.

So the seam now carries both:

| | node | gateway |
|---|---|---|
| Call | `receive(buf, cap, timeoutMs)` | `startReceive()` + `poll()` |
| Deadline | yes — bounded, terminal, must sleep | **none** |
| Why | battery; DEC-006 | mains (DEC-009); should never stop listening |

**Continuous receive is the ordinary way to build a LoRa gateway.** It is not an optimisation and the previous arrangement was not a conservative default. RadioLib's no-arg `startReceive()` uses `RX_TIMEOUT_INF`, so there is no deadline in hardware or software and nothing for an in-flight packet to be truncated against.

⚠ **The mechanism was one layer up from where it looked.** RadioLib enforces `receive()`'s timeout in **software** — a wall-clock check in its polling loop (`SX126x.cpp:300`, whose own comment reads *"safety check, the timeout should be done by the radio"*) — and then forces standby (`:307`), aborting whatever is arriving. The chip would have finished the packet: `STOP_TIMER_ON_PREAMBLE` is defined at `SX126x_commands.h:16` and **never issued anywhere in RadioLib 7.7.1**, leaving the hardware timer stopping on header detection. The first diagnosis blamed the chip and was right about the effect for the wrong reason, which is only knowable by reading the library.

### The re-arm invariant is the driver's, and that is the whole point

The radio is half-duplex. RadioLib's `transmit()` forces standby on entry (`SX126x.cpp:214`) and leaves it there (`:271`), so **every downlink silently disarms a continuous receive.**

A gateway that fails to re-arm is deaf from its first downlink onward — permanently, silently, and looking exactly like a node that stopped transmitting. Re-arming is routine in any half-duplex radio program; **owning it inside `Sx1262Radio::send()` rather than in the caller** is what stops it being forgotten, because the only caller is `src/gateway/main.cpp` and **no test env compiles that file**. That is not hypothetical either: 3.9b shipped a broken frame reader from exactly there.

It re-arms after a **failed** send too. A gateway that stops listening because one downlink failed is a worse outcome than the failed downlink.

⚠ **The host test for this was green against a broken implementation until it was mutated.** `FakeRadio` re-armed but never modelled the disarm, so deleting the re-arm changed nothing and a test named for the invariant asserted none of it. The fake now models the standby drop. **A fake that under-models the hardware makes every test above it a green light for nothing** — the same reason `test_adapters` grades fakes at all.

### Downlinks are declarative, and that is what makes delivery reliable

The OTA trigger does not send *"do an update."* The daemon compares the `fw_version` the node just reported against its manifest and sets a bit meaning *"you are not running what I have."*

If that downlink is lost, **nothing is lost.** The node reports the same stale version fifteen minutes later, the mismatch is still true, and the bit goes out again. It retries with no retry code and clears with no acknowledgement, because **the node's next packet is the acknowledgement.** Even at a pessimistic 80% link, non-convergence within three cycles is 0.8%; within five, 0.03%. It costs no extra airtime and no extra awake time.

**So: every future downlink bit describes a state the node reports back, never an action it performs.**

- ✅ *"your config should be revision 7"* — the node reports its revision; the gateway asserts until they agree.
- ❌ *"reboot now"*, *"increment your counter"* — one-shot and unobservable, genuinely needing exactly-once delivery, acks and dedup. On a node awake 2 s in 900 that protocol would be expensive, fragile, and a new way to strand a node.

If something must be one-shot, **make it observable** — put a boot counter or config revision in the packet — and it becomes declarative again.

`flags` has 15 unassigned bits (`contracts/downlink-v1.md`), and that document already says they are deliberately empty. **The discipline to spend them only on states is the thing that will be easy to lose** the first time someone wants a quick "restart" bit.

**Why:**

- **Measured, not argued.** Uplink went from 2 of 9 to 9 of 9; the full round trip ran 8 of 8 with a 1 ms spread in arrival time. Before this the gateway→node direction had never worked once.
- **It removes a failure mode rather than making it rare.** A 2000 ms slice measured just as well at this sample size (10 of 10) and was the intermediate fix, but it leaves ~8% of arrivals exposed to a boundary. Continuous receive has no boundary.
- **The declarative rule means the link's reliability stops being load-bearing.** A lost downlink costs fifteen minutes, never correctness, and no code exists to maintain that property.

**Tradeoff, and what this does not establish:**

- ⚠ **Continuous receive introduces the deafness failure mode** that the old slice loop structurally could not have, since it re-armed every iteration. It is designed out by putting the invariant in the driver, but it is a real class of bug that did not exist before and it is silent when it happens.
- ⚠ **`poll()` is only exercised on hardware.** `FakeRadio` implements it and `test_adapters` grades the fake, but the real driver's IRQ handling is bench-verified only — the same gap that hid 3.9b's microseconds-for-milliseconds bug.
- **One node, six feet, one desk.** 100% is a bench result. The duty-cycle arithmetic generalises; the number does not.
- **Nothing here is about OTA.** The manifest, the version compare, and the node's fetch-and-flash path are issue #76's remainder and will need their own decision if they turn out to have one.

**Rejected:** keeping the 2000 ms slice (a workaround for using the node's API, and it leaves a boundary); putting the re-arm in the gateway sketch (the one file nothing compiles); a single receive mode for both ends (the node's deadline is a safety property and the gateway's absence of one is the point); an ack/retry protocol for downlinks (the declarative design makes it unnecessary, and it would spend wake time to reinvent what telemetry already provides).

**Revisit:** if a second node joins, the gateway's single continuous receive becomes a shared resource and collisions become possible — nothing here addresses two nodes transmitting at once. If a downlink is ever needed that cannot be expressed as a state, this decision is what it has to argue against.
