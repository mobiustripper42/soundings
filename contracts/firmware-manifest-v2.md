# Firmware manifest v2 — what the node fetches before it fetches firmware

**Status:** v2, Phase 3.9e. Supersedes `firmware-manifest-v1.md`.
**Direction:** static HTTP server → node, over WiFi. The only part of soundings that
is not LoRa, and the only time a node's WiFi radio is ever on.

## What changed from v1, and why it is a version rather than an amendment

v2 adds one required field, `sig`, and requires the node to verify it before it
fetches an image (DEC-013).

**A new required field is exactly the change v1's own rule says needs a version
bump.** v1 rule 3: unknown keys are ignored, which is the format's only forward
compatibility, and it is one-way — a later key carrying a *requirement* is silently
skipped by an older parser. Adding `sig` to v1 in place would have left "v1" meaning
one thing to anyone who read the document last month and another to anyone reading it
today, with nothing to tell them apart. So v1 keeps its promise and this is a second
document.

⚠ **Nothing on the wire distinguishes v1 from v2.** There is no `format:` field and
deliberately so: a version marker would be a value the node must trust *before* it has
verified anything, and the signature already answers the only question that matters.
Which parser runs is decided at compile time by which firmware is on the chip.

**That asymmetry is what makes the fleet transition free.** A node still running v1
firmware ignores `sig` as an unknown key and takes the first signed image over the
air — no cable, no trip to the tank — and the image it lands on enforces from then on.
This works exactly once, in one direction, and it works *because of* the hazard rule 3
warns about. It is not a mechanism to lean on again.

## Why there is a manifest at all

**The manifest is the trigger; the `.bin` is not.** Writing a firmware image to the
server is not atomic — a `scp` takes seconds and a node that polls mid-copy would
fetch a truncated image. So the image is written first, under its own name, and the
manifest naming it is written **last and atomically** (`os.replace`, which is a rename
on the same filesystem). Until that rename lands, the new firmware is invisible: the
manifest still names the old one, and nothing about the half-written file is reachable.

This is also why the manifest carries a size and a hash rather than just a version.
The node has no way to know it received the whole image — HTTP over WiFi on a device
with a marginal signal can truncate — so it checks what it got against what it was
promised, before it writes a byte to the OTA slot.

⚠ **And it is why `sig` exists.** Size and hash defend against *corruption*, not
against an *adversary*: all three of v1's checks compared the image to a `sha256` that
arrived over the same unauthenticated channel as the image. Anyone answering at
`SOUNDINGS_OTA_HOST` served a manifest whose hash was the hash of their own binary and
every check passed. The signature is what makes the `sha256` field mean something.

## Where it lives

**The manifest is always `manifest.txt`, at the base URL.** The node builds one fixed
address from `firmware/node_secret.ini` and never discovers or follows anything:

```
http://<ota.host>:<ota.port><ota.path>manifest.txt
```

`ota.path` carries its trailing slash; `file` from the manifest resolves against the
same base. **A fixed name is what lets the manifest be the trigger** — the node asks
for one known address every time, and the only thing that ever changes is what is
behind it. Discovery would mean a node deciding at runtime which file to trust.

⚠ **`file` is a bare filename, resolved relative to that base — never a URL or a path.**
No slashes, no `..`, no scheme. A manifest that could name an arbitrary address is a
manifest that can point a field node at anything, and the node has no rollback.

## Format

A UTF-8 text file, one `key: value` per line. **Not JSON**, deliberately: the node's
parser lives in `src/core` and must compile for the ESP32 and the native test runner,
and five fixed fields do not justify adding ArduinoJson to the node build
(`firmware/platformio.ini` pins it as a native-test dependency only). If this ever
grows nested or repeated structure, JSON is the obvious answer and this decision is
the thing to revisit.

```
version: 261
size: 323521
sha256: <64 lowercase hex chars>
file: soundings-node-261.bin
sig: <128 lowercase hex chars>
```

⚠ The hash and signature above are written as **placeholders rather than
plausible-looking values**, on purpose. A realistic hex string in an illustrative
example is indistinguishable from a real one, and someone will eventually copy it.
The reproducible vectors are in `contracts/vectors/manifest-sig-v1.json`.

| Key | Type | Meaning |
|---|---|---|
| `version` | u16, decimal | The `fw_version` the image reports once running. Compared against the value on the wire (`contracts/packet-v1.md`). |
| `size` | u32, decimal | Image length in bytes. Checked against what was actually received. |
| `sha256` | 64 lowercase hex chars | SHA-256 of the whole image. **Lowercase only** — a parser that accepts both is a parser two implementations can disagree about. |
| `file` | filename | Resolved **relative to the server's base URL**. No slashes, no `..`; a manifest is not a path oracle. |
| `sig` | 128 lowercase hex chars | Detached Ed25519 signature over the canonical message below. Lowercase only, same rule and same reason as `sha256`. |

### Parsing rules

1. **Separator is a colon followed by one space** (`": "`). Not a bare colon: a
   Windows path or a URL in a future value contains colons, and a split-on-first-colon
   parser silently mangles them.
2. **Order does not matter.** Keys may appear in any order.
3. **Unknown keys are ignored**, so v3 can add a field without breaking a v2 node.
   ⚠ This is the *only* forward-compatibility this format has, and it is one-way: a v2
   node ignores what it does not know, which means a v3 field carrying a *requirement*
   would be silently skipped. Anything mandatory needs a version bump, not a new key —
   and `sig` is the worked example of that rule being followed.
4. **A missing required key rejects the whole manifest.** All five are required.
5. **Blank lines and lines beginning with `#` are ignored.**
6. **Bounded at 512 bytes and 32 lines.** The node reads this over HTTP before it can
   trust anything about it; an unbounded read is a remote party choosing how much of a
   battery device's RAM to consume. **The cap did not move for `sig`**: every field at
   its maximum width is 297 bytes and 5 lines, asserted in both test suites rather than
   stated here.

### Rejection is silent and total

A manifest that fails any rule above — including signature verification — is **not**
partially applied. The node logs nothing to the network, updates nothing, and sleeps:
the same shape as `contracts/downlink-v1.md`'s rejection rule, and for the same reason.
A node that cannot read a manifest is indistinguishable, from the field, from a node
with no update waiting, and inventing a distinction between them would create a failure
mode out of the ordinary case.

## What gets signed

⚠ **A canonical reconstruction, not the file's bytes.** Every implementation rebuilds
this message from the four values it parsed, in this fixed order, and never echoes the
bytes it received:

```
version: <version>\n
size: <size>\n
sha256: <sha256>\n
file: <file>\n
```

One line per field, `": "` separated, each terminated by a single `\n`, no trailing
content. `sig` itself is **not** part of the message — signing it would be circular.

**This is what lets three independent parsers agree.** Line order, comment lines, blank
lines, trailing spaces and CRLF endings all vanish before the signature is computed, so
there is no "which bytes exactly" question left for them to answer differently. A
byte-range rule over the file as received would have had to answer it, three times,
identically, forever.

**The cost, stated plainly: this signs the meaning, not the file.** A future unknown
key would not be covered by the signature. That is already true of the format — rule 3
means unknown keys cannot carry a requirement — but it is the first thing to check if
v3 ever adds a field.

**Ed25519, not ECDSA-P256.** Three reasons, all pointing the same way: the verify is
small enough to vendor as one file that compiles for both the ESP32 and the host test
runner, which neither libsodium nor mbedtls can claim; it is deterministic, so the
signer cannot leak the private key through a bad random nonce; and its signature is a
fixed 64 bytes, encoding as a flat 128 hex chars, where ECDSA's variable-length ASN.1
would give three parsers one more thing to disagree about. Recorded in DEC-013.

## The shared vectors

v1 graded both ends against a literal quoted in this document. v2 cannot: a signature
is not something a reader can eyeball, and a wrong one in prose would look exactly like
a right one — the failure this document already had once with a hash.

So the fixtures live in **`contracts/vectors/manifest-sig-v1.json`**, generated by
`contracts/tools/gen_sig_vectors.py`, and both suites read that file. It carries twelve
cases including the three that matter most: a tampered field, a signature over a
*different* manifest, and a manifest whose layout differs while its values do not.

⚠ **The private key in that file is published on purpose.** It is a test key, derived
from a fixed ASCII phrase so the whole file can be regenerated rather than trusted:

```
printf 'soundings manifest-sig-v1 TEST KEY -- not for production' | sha256sum
```

The base payload is still the four ASCII bytes `test`, whose SHA-256 is reproducible
with `printf 'test' | sha256sum` — the same regenerate-rather-than-trust principle v1
established after its first draft shipped an invented hash.

## Keys

**The public key is tracked.** It lives in `firmware/platformio.ini` as
`SOUNDINGS_OTA_PUBKEY`, 64 lowercase hex chars, compiled into every node build. It is
not a secret, and keeping it in git is the record of which key the fleet trusts —
changing it is a reviewable diff rather than something that happened on a laptop.

It is deliberately **not** a `[wifi]`/`[ota]` section value: those are merged over by
the untracked `node_secret.ini`, and a key a local file can blank is a key that gets
blanked by accident, on the one build where nobody notices.

⚠ **An empty or malformed key means REFUSE EVERY UPDATE.** There is no
skip-verification path anywhere in the node. An unkeyed build is a node that cannot be
updated over the air, which is the safe direction to fail.

**The private key is offline and never in this repo.** `.gitignore` carries
`*ota-signing-key*` and states why. Generate one with:

```
gateway/.venv/bin/python gateway/tools/publish_firmware.py --gen-key ~/.soundings/ota-signing-key
```

⚠ **There is no way to give a deployed node a new key except over USB.** Losing or
clobbering the private key strands the fleet — `--gen-key` refuses to overwrite for
exactly that reason. Back it up somewhere that survives the machine.

## How the three ends use it

**Publish tool** (`gateway/tools/publish_firmware.py`) — writes the image first, then
signs, then swaps the manifest in atomically. `--key` is required and there is no flag
to publish unsigned: a tool that could do it by accident would produce a fleet that
quietly stopped updating.

**Daemon** (`gateway/`) — reads the manifest off disk, compares `version` against the
`fw_version` a node just reported, and sets **bit 0** of the downlink flags if they
differ (`contracts/downlink-v1.md`).

⚠ **The daemon validates before it flags.** If the signature does not verify, the named
`file` is absent, its length does not match `size`, or its SHA-256 does not match, **no
flag is set.** A node must never be woken onto WiFi for an image the gateway could
already tell was broken — that spends the one expensive thing the node has for a result
that was knowable for free. The signature check here is *not* load-bearing — the node
verifies independently and would refuse anyway — it follows the same
check-it-before-the-radio principle as everything else.

**Node** (`firmware/src/core/`) — parses the same format after fetching it over HTTP,
**verifies the signature before requesting the image at all**, and then re-verifies the
hash itself while streaming. The signature check comes first because everything
downstream compares the image against a `sha256` that arrives in this same manifest:
until the manifest is authentic, those checks answer a question an attacker wrote.

The order, in full: **signature** → image GET → size agreement → `Update.begin()` →
streaming SHA-256 → `Update.end()`. Each is earlier than the thing it protects.

## Versioning (of the firmware, not the format)

`version` is compared for **inequality**, not ordering. A manifest naming an older
version is a deliberate downgrade and the node will take it — there is no rollback
mechanism (operator call, 2026-08-23: bricking-means-USB is accepted, and nothing may
complicate this design to avoid it), so publishing the previous manifest is the only
downgrade path there is, and it should work.

**A `fw_version` collision is the failure mode this cannot detect.** Two different
images sharing a version are indistinguishable to the node, and it will believe it is
already current. `fw_version` is bumped by hand in `firmware/src/esp32/main.cpp`;
`FW_BUILD` (sha + timestamp, from `tools/fw_build_id.py`) is what distinguishes two
builds of the same version, and it is not on the wire.

## Implementations

| End | File | Role |
|---|---|---|
| Node firmware | `firmware/src/core/fw_manifest.{h,cpp}` | Parse + canonical message |
| Node firmware | `firmware/src/core/ed25519.{h,cpp}` | Verify (vendored TweetNaCl) |
| Publish tool | `gateway/tools/publish_firmware.py` | Sign + write (atomically, last) |
| Daemon | `gateway/soundings_gateway/ota.py` | Parse + verify + validate |

⚠ **Three parsers, two verifiers, one signer, and none of them grades another.** Each
is checked against `contracts/vectors/manifest-sig-v1.json`, which is generated by a
fourth piece of code that ships in none of them. That is the same arrangement packet-v1
uses, and it is why a disagreement surfaces as two failing suites rather than as a node
that flashed something nobody checked.
