# Serial framing v1 — packet-v1 over the gateway's USB link

**Status:** v1, Phase 3.9a
**Scope:** the USB serial link between the gateway radio board and the Python
daemon on `bee-grace`. Nothing else uses this. The LoRa air interface is not
framed by this document — the radio delivers whole packets, and this exists only
because a serial port delivers a byte stream.

## Why there is a frame at all

`contracts/packet-v1.md` packets are **binary and variable-length** — 14 bytes at
minimum (header + CRC, no channels), 46 at maximum (`packet.h:23`). A serial port
hands the daemon an undelimited stream of bytes, so without an envelope there is
no way to know where one packet ends and the next begins.

The stream also carries bytes that are not packets at all. The ESP32 prints boot
chatter on U0TXD at every reset, and a reset happens whenever the port is opened
with DTR asserted. **Anything reading this link starts mid-garbage and must
expect to.**

## Frame

```
+------+------+------+---------------------+
| 0xA5 | 0x5A | LEN  | PAYLOAD (LEN bytes) |
+------+------+------+---------------------+
   0      1      2      3 .. 3+LEN-1
```

| Field | Width | Value |
|---|---|---|
| Sync | 2 B | `0xA5 0x5A`, in that order |
| `LEN` | 1 B (u8) | Payload length. **Valid range 14–46 inclusive** |
| Payload | `LEN` B | One packet-v1 frame, exactly as the radio delivered it |

Overhead is 3 bytes. Frames are emitted back to back with nothing between them.

## No frame checksum, deliberately

The payload already carries **CRC-16/CCITT-FALSE over its own bytes**
(`contracts/packet-v1.md` § CRC, DEC-003), and USB's link layer is itself
CRC-protected with retry. A mis-framed payload therefore fails the packet CRC and
is dropped by `decode()` — which is the same path a radio-corrupted packet takes,
already built and already tested.

A second checksum here would be a second thing to get wrong, and a second
algorithm to keep straight. **If a future revision decides the `LEN` byte needs
protecting, reuse CRC-16/CCITT-FALSE — do not introduce a second algorithm into
this repo.**

## Reading the stream

A reader holds a buffer and repeats:

1. **Find the sync.** Discard everything before the first `0xA5 0x5A`. If no sync
   is present, discard everything except a possible trailing `0xA5` (which may be
   the first half of a sync whose second byte hasn't arrived).
2. **Wait for `LEN`.** If fewer than 3 bytes are held, wait for more.
3. **Range-check `LEN`.** If it is outside 14–46, this was not a frame header —
   a payload byte, or boot text that happened to contain the sync pattern.
   **Discard exactly one byte** (the first `0xA5`) and return to step 1. Dropping
   the whole sync pair instead would miss a real frame in `A5 A5 5A …`.
4. **Wait for the payload.** If fewer than `3 + LEN` bytes are held, wait.
5. **Emit** the `LEN` payload bytes and consume `3 + LEN`. Return to step 1.

A reader that follows this holds at most 48 bytes of unresolved input, so the
buffer is bounded without needing an explicit cap.

**Emitting is not validating.** The reader yields *candidate* payloads; whether a
candidate is a real packet is settled by the packet CRC downstream. This is why
the reader has no error state to get stuck in: every failure is either "discard a
byte" or "wait for more bytes."

### The three resynchronisation cases this must survive

| Case | What the reader sees | Outcome |
|---|---|---|
| Board reset | Boot chatter, then frames | Chatter discarded at step 1 or 3 |
| Reset mid-frame | A truncated payload, then chatter, then frames | The truncated frame **absorbs** what follows — see below |
| Daemon restart | Stream opened mid-frame | Partial frame discarded at step 1 or 3; the next whole frame is read normally |

**A truncated frame absorbs the bytes behind it, and that is accepted.** If the
board dies after writing a valid `LEN` but before writing the payload, the reader
is committed: it waits for `LEN` bytes and takes whatever arrives next. It emits
one garbage candidate, which the payload CRC rejects downstream.

**The bound is in bytes: at most 46 bytes of the following stream may be misread
as payload.** That is what the code guarantees — `LEN` cannot exceed the largest
legal packet, so the reader cannot be committed to more than that.

Expressed in *frames*, the worst case is **two**, not one. The smallest complete
frame is 3 + 14 = 17 bytes, and ⌊46 ÷ 17⌋ = 2 — so a reset landing immediately
after a `LEN` of 46 can swallow two whole minimum-sized frames before the window
closes. Two lost readings is 30 minutes of tank level at the 15-minute cadence.

Accepted, because the alternatives don't help: a frame checksum would detect the
absorption one frame earlier but not prevent it, and the event requires a board
reset landing inside a frame, which is not something that happens in steady
operation. The property that matters is that the reader never wedges — it is
consuming bytes the whole time and is back in sync within one window.

**A false sync inside a payload is expected, not exceptional.** `0xA5 0x5A` can
occur in packet data. It is harmless: it is only looked for when the reader is
*between* frames, and when it does cause a bad lock, the range check or the
payload CRC rejects the result within one frame.

## Implementations

| End | File | Direction |
|---|---|---|
| Node/gateway firmware | `firmware/src/core/serial_framing.{h,cpp}` | Encode |
| Python daemon | `gateway/soundings_gateway/framing.py` | Decode |

The encoder lives in `src/core` rather than in the gateway sketch so that it
compiles and is tested under `pio test -e native`. It is not behind an adapter
seam — it is a pure function over bytes, not a thing with a hardware
implementation and a fake.

## No shared vector file — a deliberate departure from packet-v1

`contracts/vectors/packet-v1.json` exists because two independent implementations
each *derive* a layout that is a function of `channel_mask`, and get it silently
wrong in ways that produce plausible readings. **None of that applies here.**

- The framing is **one-way**: firmware only encodes, Python only decodes. There
  is no round trip for a vector to pin.
- The layout is **fixed**, not computed. Three constant-width fields.
- The failure is **loud and immediate**. A framing bug means nothing decodes, at
  the bench, on the first try — not a wrong number that looks fine for a season.

Framing test cases are literal byte strings in each end's own tests. If this ever
grows a computed field, revisit that.
