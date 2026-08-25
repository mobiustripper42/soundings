---
id: DEC-012
title: "OTA is a pull, and the manifest is the trigger"
topic: "Firmware architecture"
amends_spec:
  - section: "3"
    scope: "the Not-V1 row forbidding OTA; the rest of scope is unchanged"
  - section: "8"
    scope: "the updates line only; the one-firmware and host-testable-core points stand"
---

## DEC-012: OTA is a pull, and the manifest is the trigger

**See also DEC-011**, which decided that downlinks describe state rather than command action — this is the first thing built on that rule and the reason it exists. **See also DEC-010**, whose receive window is the only moment a node can be told anything, and DEC-006, whose power findings bound everything below.

**Decision:** field nodes update over WiFi by **pulling** — the daemon sets a flag, the node fetches when it chooses. The **manifest**, not the `.bin`, is what makes a release visible, and it is written last and atomically. WiFi comes up only when there is something to fetch. **There is no rollback.**

### Why a pull, when tinkle is a push

tinkle's OTA runs an HTTP server on the device and you `curl -F` an image to it (`/home/estoffer/tinkle/docs/OTA.md:37`). It has worked since 2026-07-10 and it is the obvious thing to copy.

**It cannot transfer.** tinkle is mains-powered and always associated; a soundings node is awake about 2 s in every 900 and its radio is off the rest of the time. There is no moment at which a person can reach it, so the only workable shape is one the node initiates. The node reports `fw_version` in every packet already (`contracts/packet-v1.md`), so staleness is detectable with nothing new on the wire — and DEC-010's receive window is the one moment it can be told.

### The manifest is the trigger, and the ordering is the decision

Writing a ~900 KB image to a server is not atomic. A node that polled mid-copy would fetch a truncated file, and a truncated file that happens to flash is a node in a tank that no longer boots.

So: the image is written first under its own name, then a manifest naming it is moved into place with `os.replace`. **Until that rename lands the release does not exist** — the manifest still names the previous image, and nothing about the half-written copy is reachable.

`contracts/firmware-manifest-v1.md` carries `version`, `size`, `sha256` and `file`. It exists as a contract rather than an implementation detail because three independent parsers read it: the node's (C++, `src/core/fw_manifest.cpp`), the daemon's, and the publish tool that writes it.

### Verification happens three times, on purpose

1. **The daemon, before it flags.** Absent file, wrong size, or hash mismatch ⇒ **no bit set**. A node must never be woken onto WiFi for an image the gateway could already see was broken — that spends the most expensive thing the node owns for a result knowable for free.
2. **The node, while streaming.** Not redundant with (1): the daemon checked a file *on disk*, the node checks *what arrived over the air*. Different failures, both real. The check completes **before `Update.end()`**, so a bad image never becomes the boot target.
3. **`Update.end()` itself**, which is the third line of defence rather than the first.

### No rollback, and nothing may be complicated to avoid it

Operator call, 2026-08-23: **bricking-means-USB is an accepted cost.** Two app slots come free with the ESP32 partition table and are used, but automatic fallback is a bootloader option that was not pursued.

Consequently `version` is compared for **inequality, not ordering** — republishing an older manifest is the only downgrade path there is, so it has to work.

### Credentials are build-time

`firmware/node_secret.ini` (untracked) supplies SSID, password and the server address; `platformio.ini` carries empty defaults and merges the secret file over them via `extra_configs`. **Verified by three builds** that PlatformIO merges a redefined section key-by-key rather than replacing it (`docs/DEV_REFERENCE.md`), so a partial or absent file compiles rather than failing.

⚠ **Which makes empty values the thing to guard, not build errors.** An absent secret file produces an empty host and a URL like `http://:manifest.txt` from a perfectly green build. The node treats an empty SSID or host as *"not configured, never attempt an update"* and refuses.

This supersedes issue #76's original NVS-provisioning plan. A password change costs a USB trip either way, so the axis that decided it was that a build-time credential is compiled into every image — **including the ones served over the LAN by the OTA server.** That exposure is accepted for a farm LAN and is the thing to revisit first if it ever stops being.

**Why, in short:**

- **Measured end to end** on 2026-08-25: v261 → v262, 923,872 bytes, SHA-256 verified, flashed in 6.2 s, WiFi cold join 999 ms. The flag then cleared itself and stayed clear for eleven consecutive cycles.
- **Delivery reliability stops being load-bearing.** Because the flag is derived from a version mismatch rather than delivered as a command, a lost downlink costs fifteen minutes and no correctness — and there is no retry or acknowledgement code anywhere to maintain.
- **The expensive resource is guarded at the cheapest point.** Every check the daemon can perform, it performs before the node's radio comes on.

**Tradeoffs, and what this does not establish:**

- ⚠ **An update costs ~7.5 s awake against a normal ~2 s wake** — roughly four ordinary wakes. Rare enough not to threaten DEC-006's budget, but it is not free, and a node that failed repeatedly would retry every cycle forever. Nothing currently bounds the number of attempts.
- ⚠ **The first WiFi join after a flash took 4150 ms**, four times the typical 999 ms. The 15 s timeout accommodates it; the cause was not chased.
- ⚠ **Measured on a bench six feet from an AP, at RSSI −43 to −90 dBm.** The tank is served by a different AP entirely and nothing here has been tested there. tinkle sits at −72 dBm on that AP and is reliable, but it moves small MQTT messages, not 900 KB.
- ⚠ **A `fw_version` collision is undetectable.** Two images sharing a version are indistinguishable to the node, which will believe it is current. `FW_BUILD` distinguishes them for a human but is not on the wire.
- **The publish tool cannot verify that `--version` matches the built binary.** It is a constant in a compiled image; nothing reads it back.

**Rejected:** a push model (no moment exists at which to push); the `.bin` as trigger (a partial copy is fetchable); JSON for the manifest (four fixed fields do not justify ArduinoJson in the node build); NVS-provisioned credentials (a password change costs a USB trip either way, and this is simpler); rollback (explicitly, and nothing may be complicated to add it later); an ack for the flag (the node's next packet already is one).

**Revisit:** if a node ever fails an update repeatedly, bound the attempts. If the credential-in-image exposure stops being acceptable, NVS provisioning is a swap rather than a redesign. If an image ever approaches the 3,342,336-byte slot, the build gate fails first and the partition table is the next question.
