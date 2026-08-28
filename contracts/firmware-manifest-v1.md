# Firmware manifest v1 — what the node fetches before it fetches firmware

**Status:** SUPERSEDED by `firmware-manifest-v2.md` (Phase 3.9e, DEC-013).
**Direction:** static HTTP server → node, over WiFi. The only part of soundings that
is not LoRa, and the only time a node's WiFi radio is ever on.

> ⚠ **This document is kept as written, and its promise still holds: a v1 manifest is
> these four fields.** v2 adds a required `sig` field and requires the node to verify
> it. That is a new *required* field, which is precisely the change rule 3 below says
> needs a version bump rather than a new key — so v1 was not amended, and "v1" still
> means today what it meant when this was written.
>
> Nothing on the wire distinguishes the two. Which parser runs is decided at compile
> time by which firmware is on the chip, and a node still running v1 firmware ignores
> `sig` as an unknown key. That is what made the fleet transition free, and it is the
> hazard rule 3 warns about being useful exactly once.
>
> **Nothing implements this document any more.** Read v2.

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
and four fixed fields do not justify adding ArduinoJson to the node build
(`firmware/platformio.ini` pins it as a native-test dependency only). If this ever
grows nested or repeated structure, JSON is the obvious answer and this decision is
the thing to revisit.

```
version: 261
size: 323521
sha256: <64 lowercase hex chars>
file: soundings-node-261.bin
```

⚠ The hash above is written as a **placeholder rather than a plausible-looking value**,
on purpose. A realistic 64-hex string in an illustrative example is indistinguishable
from a real one, and someone will eventually copy it. The reproducible vector is below.

| Key | Type | Meaning |
|---|---|---|
| `version` | u16, decimal | The `fw_version` the image reports once running. Compared against the value on the wire (`contracts/packet-v1.md`). |
| `size` | u32, decimal | Image length in bytes. Checked against what was actually received. |
| `sha256` | 64 lowercase hex chars | SHA-256 of the whole image. **Lowercase only** — a parser that accepts both is a parser two implementations can disagree about. |
| `file` | filename | Resolved **relative to the server's base URL**. No slashes, no `..`; a manifest is not a path oracle. |

### Parsing rules

1. **Separator is a colon followed by one space** (`": "`). Not a bare colon: a
   Windows path or a URL in a future value contains colons, and a split-on-first-colon
   parser silently mangles them.
2. **Order does not matter.** Keys may appear in any order.
3. **Unknown keys are ignored**, so v2 can add a field without breaking a v1 node.
   ⚠ This is the *only* forward-compatibility this format has, and it is one-way: a v1
   node ignores what it does not know, which means a v2 field carrying a *requirement*
   would be silently skipped. Anything mandatory needs a version bump, not a new key.
4. **A missing required key rejects the whole manifest.** All four are required.
5. **Blank lines and lines beginning with `#` are ignored.**
6. **Bounded at 512 bytes and 32 lines.** The node reads this over HTTP before it can
   trust anything about it; an unbounded read is a remote party choosing how much of a
   battery device's RAM to consume.

### Rejection is silent and total

A manifest that fails any rule above is **not** partially applied. The node logs
nothing, updates nothing, and sleeps — the same shape as `contracts/downlink-v1.md`'s
rejection rule, and for the same reason: a node that cannot read a manifest is
indistinguishable, from the field, from a node with no update waiting, and inventing a
distinction between them would create a failure mode out of the ordinary case.

## The shared literal

Both ends are graded against these exact bytes rather than a vector file — the same
arrangement `downlink-v1` uses, and for the same reasons: fixed shape, no computed
layout, and a mistake means *nothing* updates rather than a plausible wrong result.

```
version: 261
size: 4
sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08
file: t.bin
```

That hash is SHA-256 of the four ASCII bytes `test`. A test at either end can compute
it rather than trusting this document, which is the point of choosing a value someone
can reproduce in one command:

```
printf 'test' | sha256sum
```

⚠ **The first draft of this document had a different hash here, and it was invented.**
It looked exactly as plausible as the correct one, and nothing but running the command
above could have told them apart. That is the entire argument for a vector a reader can
regenerate instead of one they must trust — and the third time in this repo a number has
been wrong in prose while the code around it was fine.

## How the two ends use it

**Daemon** (`gateway/`) — reads the manifest off disk, compares `version` against the
`fw_version` a node just reported, and sets **bit 0** of the downlink flags if they
differ (`contracts/downlink-v1.md`).

⚠ **The daemon validates before it flags.** If the named `file` is absent, its length
does not match `size`, or its SHA-256 does not match, **no flag is set.** A node must
never be woken onto WiFi for an image the gateway could already tell was broken —
that spends the one expensive thing the node has for a result that was knowable for
free.

**Node** (`firmware/src/core/`) — parses the same format after fetching it over HTTP,
and **re-verifies the hash itself** while streaming the image. That is not redundant
with the daemon's check: the daemon validated the file *on the server*, and the node
is validating what *arrived over WiFi*. Different failures, both real.

`Update.end()` validating the image is the **third** line of defence, not the first.

## Versioning

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
| Node firmware | `firmware/src/core/fw_manifest.{h,cpp}` | Parse |
| Publish tool | `gateway/tools/publish_firmware.py` | Write (atomically, last) |
| Daemon | `gateway/soundings_gateway/ota.py` | Parse + validate |

All three exist as of Phase 3.9d. The contract was written **before** any of them, as
the brief all three were built against — which is why three independent parsers agree
without a shared vector file, and why the shared literal below is one a reader can
regenerate rather than trust.
