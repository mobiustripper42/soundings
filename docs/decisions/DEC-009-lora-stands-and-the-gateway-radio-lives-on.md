---
id: DEC-009
title: "LoRa stands, and the gateway radio lives on the server"
topic: "Radio, wire contract & gateway"
amends_spec:
  - section: "12"
    scope: "the D3 and D4 rows, both now resolved; the rest of the register is unchanged"
---

## DEC-009: LoRa stands, and the gateway radio lives on the server

**See also DEC-005**, which sequenced the tank node first and scheduled the range test whose result this decision records. DEC-005's `Revisit` condition — *"if the range test shows the link can't close from the tank cluster, the gateway architecture branches"* — **did not fire**. Its sequencing stands unchanged.

**Decision:** The node↔gateway link is **LoRa, SX1262 at both ends**, and the gateway radio is a Heltec Wireless Stick Lite V3 **permanently attached to the server (`bee-grace`) over USB serial**. **Resolves D3 and D4.** Three things follow:

- **D3 (node↔gateway PHY pairing) is dissolved, not chosen.** Both ends are the same board and the same silicon, so there is no pairing question, no SX127x/RFM95 driver, and one radio toolchain to learn. This was already the leading candidate in `SPEC.md:422`; the bench confirmed it.
- **D4 (gateway box) is radio-on-the-server.** No Pi Zero, and no Heltec acting as a LoRa→WiFi bridge. The bridge fallback at `HARDWARE_BUILD_PLAN.md` §3 is **not needed and not built** — it existed for the case where the link wouldn't close at the server, and the link closes at the server with margin.
- **Issue [#48](https://github.com/mobiustripper42/soundings/issues/48)'s shape is confirmed rather than chosen.** It was already specified as LoRa → USB serial + `SerialPacketSource`; that assumption is now load-bearing on evidence instead of expectation.

**The evidence.** Range test 2026-08-19 (issue [#41](https://github.com/mobiustripper42/soundings/issues/41), Phase 3.2), walking board vs stationary board on `bee-grace`, receive-side logged per packet:

| Location | Sent / received | RSSI | SNR |
|---|---|---|---|
| Desk (baseline) | 1 / 1 | −46 | +6.0 |
| Tank cluster | 5 / 5 | −82 to −88 | −0.75 to −5.75 |
| Property corner | 5 / 5 | −84 to −89 | −2.75 to −7.0 |
| Tunnel (moisture sensors) | 5 / 4 | −83 to −86 | −2.5 to −4.75 |

**20 sent, 19 received.** The result that matters is not the absolute numbers but their flatness: tank, property corner and tunnel are indistinguishable at −82 to −89 RSSI and about −3 dB SNR. The edge of the link was never found — the property ran out first.

**Why:**

- **The margin is large, and margin is what a deferred decision needs.** The modem preset in use decodes to roughly −20 dB SNR; the working locations sat around −3 dB. A decision made on a link that *barely* closed would have to be revisited the first wet winter. This one has roughly 15 dB of headroom at every location a node is going.
- **It removes an architecture rather than picking one.** D4's three candidates each carried their own work — a Pi to provision and maintain, or WiFi + MQTT + reconnect logic on an ESP32 (`HARDWARE_BUILD_PLAN.md` §3, costed there as "real firmware"). Radio-on-the-server is the only one that adds no new device and no new failure domain: the gateway is a USB peripheral of a machine that is already running, already on the LAN, and already the thing the Python gateway runs on.
- **The tunnel result is worth more than the tank result.** The tank was the question being asked; the tunnel is where the soil-moisture nodes go, and it is the anchor measurement this whole project exists for (`SPEC.md` §1). Getting both from one walk means Phase 2's link risk is retired too, without a second trip and without a second decision.

**WiFi instead of LoRa was raised and is rejected.** Recorded because it was a deliberate, reasonable audible and the reasoning should not have to be rebuilt from scratch if it resurfaces:

- **The appeal was real.** It would delete issue #48 outright, delete the LoRa driver work, and let a node publish MQTT straight to the broker `contracts/publish-v1.md` already targets — removing the gateway from the node's path entirely.
- **It fails on battery, and not narrowly.** `HARDWARE_BUILD_PLAN.md` §6 budgets ~2,460 mAh over two years against a ~4,500 mAh cold-derated pack, of which **~1,750 mAh is the wake term** (96 wakes/day, ~2 s, ~45 mA). *Estimate, not a measured figure:* a WiFi wake is association plus DHCP plus MQTT connect — call it 3–8 s at ~100–150 mA, roughly 6× the wake energy, which puts two years near 10,500 mAh against a 4,500 mAh pack. Two years becomes something like nine months. Tuned hard — static IP, stored BSSID and channel, ~1.5 s connect — it is still ~2.5×, sitting on the budget line with no cold margin. This is what `SPEC.md:153` means by "leaving it on destroys the battery budget": the wake term is the dominant term, not a footnote.
- **What actually settles it: there is no power anywhere the sensors are going.** WiFi is viable only if a node stops being battery-only. Mains or solar at the tank or in the tunnel would make it the simpler system — that option does not exist here, so the battery-only premise holds and LoRa is the answer.
- **The comparison to tinkle does not transfer.** tinkle runs on 24 V continuously, so WiFi was free there. Nothing else about that experience is evidence about this project.

**Tradeoff and what this decision does *not* establish:**

- **The test was run on Meshtastic, not on soundings firmware.** Both boards ran stock Meshtastic 2.7.26 at US / LONG_FAST, because that was the cheapest way to get two radios talking on the day the boards arrived. Path loss is path loss, so the RSSI figures characterize **the site**, which is what D3 and D4 turn on. They do **not** characterize the modem configuration soundings will actually ship, and the SNR margin quoted above is against LongFast's floor specifically. When issue #48 picks real radio parameters, that is a new question about spreading factor and airtime — not a re-run of this one.
- **The sample is small and hand-driven.** Five packets per location, sent by hand from a phone, because a beacon interval was set on the walking node and silently never took effect. The single tunnel miss is **one event, not a measured loss rate** — the signal there was identical to the open locations, so it was not weak signal, and it may equally have been a send that never left the phone. A real loss rate needs ten or more per location.
- **Only the receive path was measured.** Every number is receive-side at the gateway. The return path — gateway to node, which matters for any future downlink or ACK — was not measured at all.
- **Foliage is at its August maximum**, so this is close to the worst-case seasonal condition for the path, which is the right direction for the error to run.
- **Fixing the gateway to `bee-grace` couples the link to one machine.** Accepted: DEC-004 already makes the server the place soundings' data goes, and a USB radio moves to another machine in the time it takes to unplug it.

**Revisit:** If a later node location fails to close — a new sensor site further out, or a tunnel end not walked on 2026-08-19 — the fallback ladder in `HARDWARE_BUILD_PLAN.md` §3 is intact and unbuilt, and this decision is what should be reopened. Moving the gateway is the cheap first move; the WiFi bridge is the expensive one. Reopen the WiFi-vs-LoRa question only if a node site gains mains power, which is the single premise above that could change.

---
