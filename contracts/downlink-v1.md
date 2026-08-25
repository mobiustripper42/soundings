# Downlink v1 — the reply a node hears in its receive window

**Status:** v1, Phase 3.9b
**Direction:** gateway → node, over LoRa. The opposite direction from
`contracts/packet-v1.md`, and a much smaller message.

## When it happens, and why it is this shape

The node is deep asleep ~99 % of the time (`HARDWARE_BUILD_PLAN.md` §6), so the
gateway can never reach it on demand. The only moment it can be told anything is
one the node offers: **a short receive window held immediately after each
transmit** (DEC-010). Everything about this format follows from that.

- **It is tiny**, because the window is airtime the node pays for on every wake
  whether or not anything is said.
- **It is addressed**, because a window is open on every node in earshot at once
  and a node must ignore traffic meant for someone else.
- **Silence is a legal answer.** No downlink means nothing is pending. The
  gateway is not obliged to reply, and the node treats a quiet window as normal —
  not as a fault, and not as a link failure.

## Frame

```
+----+----+----+----+----+----+
| PV | ID |   FLAGS |   CRC16 |
+----+----+----+----+----+----+
  0    1    2    3     4    5
```

| Offset | Field | Width | Value |
|---|---|---|---|
| 0 | `proto_ver` | u8 | `0x01` |
| 1 | `node_id` | u8 | The node this is addressed to. A node drops anything else. |
| 2 | `flags` | u16 | Little-endian. **No bits are assigned in v1.** |
| 4 | `crc16` | u16 | **CRC-16/CCITT-FALSE** over bytes 0–3, appended little-endian — the same algorithm and the same framing as packet-v1 (DEC-003). |

Six bytes. All multi-byte fields little-endian, matching packet-v1.

**`flags` is deliberately defined and deliberately empty.** Reserving a bit for a
purpose that does not exist yet is how a contract acquires fields nobody can
explain. v1 carries no bit assignments; a downlink with `flags == 0` means *"you
are heard, and there is nothing for you."* Issue #76 assigns the first bit when
it has something to say with it.

## Reading one

1. **Reject anything not exactly 6 bytes.**
2. **Reject `proto_ver != 0x01`.** A node from a future protocol is not a node to
   guess at.
3. **Reject a CRC mismatch.**
4. **Reject a `node_id` that is not ours** — silently, and this is the common
   case rather than an error. Two nodes in earshot both hear both replies.
5. Otherwise the flags are valid and current.

**Every rejection is silent and identical in effect: the window closes and the
node sleeps.** There is no error path, no retry and no fault bit, because a
downlink the node did not understand is indistinguishable from a downlink that
was never sent — and a quiet window is already the ordinary case. Making a bad
downlink louder than an absent one would invent a failure mode out of the
sensor's normal state.

## What it is *not*

- **Not an acknowledgement.** V1 telemetry is fire-and-forget (`SPEC.md` §2); the
  node does not retransmit and does not care whether a packet arrived.
- **Not a command channel.** Soundings never actuates (`SPEC.md` § Not V1).
  Everything this can ever carry is about the node's own housekeeping.
- **Not measured.** ⚠ DEC-010 records this: every figure in DEC-009 is
  receive-side at the gateway, so the gateway→node direction has never been
  range-tested. The path is reciprocal and both ends are the same silicon, which
  is a good argument and not a measurement.

## Over the wire between the daemon and the gateway board

The daemon decides what to send; the gateway board only relays. So a downlink
travels **daemon → USB serial → gateway board → LoRa**, and the serial leg uses
`contracts/serial-framing-v1.md` in the reverse direction — same envelope, same
rules, same reader algorithm.

That is why serial framing v1's minimum payload length is 6 rather than 14: it
now carries two payload types, and the smaller one sets the floor. See the
amendment in that document.

## Bit 0 assigned — firmware update waiting (amended 2026-08-25, Phase 3.9d)

**What this changes, and what still stands.** The frame, the field widths, the CRC and
every rejection rule above are unchanged. What changes is one sentence: `flags` is no
longer empty. **Bit 0 (`0x0001`) means "you are not running what I have."**

| Bit | Mask | Meaning |
|---|---|---|
| 0 | `0x0001` | **Firmware update waiting.** The manifest's `version` differs from the `fw_version` this node reported. |
| 1–15 | — | Unassigned. |

`flags == 0` still means *"you are heard, and there is nothing for you"*, and it is still
what almost every window carries.

### It describes a state; it does not command an action

This is the distinction DEC-011 fixes, and the reason the bit is worded the way it is.
The daemon does not send *"do an update"* — it compares the `fw_version` on the wire
against `contracts/firmware-manifest-v1.md` and asserts the difference.

**That is what makes a lost downlink cheap.** If this frame never arrives, the node
reports the same stale version fifteen minutes later, the mismatch is still true, and the
bit goes out again. It retries with no retry code and clears with no acknowledgement,
because the node's next packet *is* the acknowledgement. At a pessimistic 80% link,
failing to converge within three cycles is 0.8%.

⚠ **Spend the remaining 15 bits only on states the node reports back.** An action bit —
*"reboot now"*, *"increment your counter"* — is one-shot and unobservable, and would need
exactly-once delivery that this link deliberately does not provide. If something must be
one-shot, make it observable instead (a boot counter, a config revision in the packet) and
it becomes a state again.

### What the node does with it

Nothing, unless an `IDownlinkHandler` is bound (`firmware/src/core/idownlinkhandler.h`).
A node with no OTA client decodes the flag and ignores it, which is a legitimate
configuration and the one every node ran before 3.9d.

⚠ **The daemon validates before it sets this bit.** If the image the manifest names is
absent, the wrong length, or fails its hash, **no flag is sent** — a node must never be
woken onto WiFi for an image the gateway could already tell was broken. That check is in
`gateway/soundings_gateway/ota.py`, and it is the first of three: the node re-verifies the
hash while streaming (a different failure — this end checked the file on disk, the node
checks what arrived over the air), and `Update.end()` is the third.
