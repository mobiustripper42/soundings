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

## Amendment, 2026-08-25 (eric) — the security posture, which the original text did not examine

**What this changes, and what still stands.** Every mechanism above is unchanged and the OTA path works as described. What this adds is the analysis the original decision skipped: it weighed *corruption* — a half-finished `scp`, a lossy WiFi link — and never weighed an **adversary**. `/security-review` raised it on the 3.9d PR, and it is right that the word "attacker" appears nowhere in the original.

### The three checks defend against accident, not against malice

The daemon's validation, the node's streaming hash, and `Update.end()` are described above as three lines of defence. Against a hostile party they are **one line, and it is not load-bearing**: all three compare the image against a hash that arrives over the same unauthenticated channel as the image. Anyone answering at `SOUNDINGS_OTA_HOST` serves a manifest whose `sha256` is the hash of their own binary, and every check passes.

**Firmware is fetched over plaintext HTTP with no signature.** There is no TLS, no image signing, and no pinned key anywhere in 3.9d.

### Assigning bit 0 changed the meaning of an existing channel

⚠ **This is the part worth remembering, because it is not visible in this file's diff.** `contracts/downlink-v1.md` accepts a downlink on **CRC-16 alone** — no MAC, no nonce, no shared secret. That was harmless while `flags` carried no assignments: a forged downlink made a node do nothing.

Bit 0 attaches *fetch and execute code* to that frame. **The downlink did not become less authenticated; the consequence of forging one became arbitrary code execution.** Any SX1262 running DEC-010's published modem parameters can transmit one, and the node opens a window every wake.

### What this costs, given no rollback

DEC-006's "bricking-means-USB is accepted" was an **availability** cost — a bad flash we published ourselves. Under this analysis it is also **attacker-triggerable**, and a malicious image can refuse all future OTA, closing the only remote path to fixing it. Recovery is a trip to the tank with a cable, per node.

### The fix, when it is worth doing

**Sign the manifest.** Embed an Ed25519 or ECDSA-P256 public key in the firmware — mbedtls is already linked for SHA-256, so this costs no new dependency — have `publish_firmware.py` sign the four manifest lines with an offline key, add a `sig:` field, and verify before `Update.begin()`. That makes the `sha256` field mean something and leaves the transport in plaintext, which is fine.

**TLS alone is the weaker fix**: it authenticates the server rather than the image, needs certificate management on a battery node, and does nothing about the forgeable trigger. Signing addresses both, because an attacker who forges a downlink then cannot produce an image the node will accept.

### Accepted for now, and the condition for revisiting

**This is a private farm LAN behind a residential router, with one node, in a field.** The attacker must be in RF range of the property *and* on the WiFi, and the payoff is a tank-level sensor. That is a real risk and a small one, and shipping unsigned is a deliberate choice rather than an oversight — now that it is written down, which it was not before.

**Revisit when any of these becomes true:** a node moves somewhere the LAN is not physically controlled; the fleet grows enough that a trip to every node is a real cost; anything on this network stops being sensors; or DEC-012's credential-in-image exposure is closed, since a published `.bin` currently hands out the PSK that is the first step of the chain. **Signing before the fleet grows is much cheaper than signing after** — every deployed node needs the key, and a node that ships without one can only be given it by USB.

⚠ **One thing was fixed rather than accepted.** `configured()` checked the SSID and host but not the password, so a `node_secret.ini` missing its `pass` line compiled to `WiFi.begin(ssid, "")` — associating to an **open** network of that SSID, and turning "attacker needs the PSK" into "attacker needs to broadcast an SSID". Silent from a green build. Now checked.
