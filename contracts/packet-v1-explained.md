# Soundings packets for dummies

A plain-English companion to [`packet-v1.md`](packet-v1.md) (which is the precise
spec). Read this first; read that when you need the exact bytes. If the two ever
disagree, `packet-v1.md` wins.

## What problem is a "packet" even solving?

A field node wakes up every 10–15 minutes, reads its sensors, and needs to tell
the gateway what it measured — over a slow radio link with a tiny power budget.
It can't send a nice readable sentence like `soil tension is 15.2 kPa, battery
3.7 V`. That'd be huge, and radio airtime is the scarce resource (more bytes =
more battery, more chance of collisions).

So instead the node sends a **packet**: a short, dense run of bytes — about 16 to
30 of them — where *the position of each byte tells you what it means*. No labels,
no spaces. Both sides agree on the layout in advance. That agreement is "the
contract." This whole `contracts/` folder exists to make sure the node (which
writes the bytes, in C++) and the gateway (which reads them, in Python) agree
down to the last bit forever.

Think of it like a pre-printed form where everyone fills in the same boxes in the
same order, versus mailing a handwritten letter. The form is smaller and there's
no ambiguity about which number is which.

## The shape of a packet

Every packet has three parts, in order:

```
[ HEADER ] [ SENSOR READINGS ] [ CHECKSUM ]
  always       varies per          always
  14 bytes     node                2 bytes
```

1. **Header** — the same 14 bytes on every packet: who sent it, a counter, the
   battery level, and two "which sensors" maps (explained below).
2. **Sensor readings** — only the sensors this particular node actually has. A
   tank node sends one reading; a garden-bed node sends six. This is why packets
   vary in size.
3. **Checksum** — 2 bytes of math over everything before it, so the gateway can
   tell if the radio garbled the message in flight. If the math doesn't match,
   the gateway throws the packet away rather than trust bad data.

## The two clever bits

Almost everything here is boring and obvious (a node ID is just a number). Two
ideas are worth actually understanding, because they're the design decisions:

### 1. The "channel mask" — how one packet format fits every node

We don't want a different packet design for tank nodes vs. bed nodes. So the
header contains a **channel mask**: 16 on/off switches packed into 2 bytes. Each
switch corresponds to one possible sensor reading ("soil tension probe #1", "soil
temperature probe #2", "tank distance", and so on — the full list is the *channel
registry* in the spec).

- Switch **on** = "this node has that sensor, and its reading is in this packet."
- Switch **off** = "this node doesn't have that sensor; don't look for it."

So the node literally tells you its own sensor list in every message. A tank node
flips one switch on; a bed node flips six. The gateway reads the switches, then
knows exactly how many readings follow and what each one is. One format, every
node. (This is the project's "declared, not auto-detected" principle — DEC-002 —
made concrete.)

### 2. The "fault mask" — the difference between "no sensor" and "sensor broke"

Here's the subtle part. Suppose a bed node *has* a 12-inch soil-temperature probe,
but this cycle the probe failed to answer. We do **not** want to just leave it out
of the packet — because "left out" already means "this node doesn't have that
sensor." Those are completely different situations: one is normal, the other means
*go check the hardware*.

So there's a second set of 16 switches: the **fault mask**. For a sensor that's
present-but-broke-this-time, the node leaves its channel-mask switch **on** (so
the reading still has its spot in the packet) and *also* flips the matching
fault-mask switch on, meaning "ignore this number, the read failed." The gateway
sees that and can raise an alert: "node 1's 12-inch temp probe is missing."

Three possible states for any sensor, all unambiguous:

| Situation | channel switch | fault switch |
|---|:--:|:--:|
| Node doesn't have this sensor | off | off |
| Sensor read fine | on | off |
| Node has it, but it broke this cycle | on | **on** |

A missing reading is never a silent gap — it's always either "not my job" or "out
loud, this is broken."

## "Raw values" — why the numbers look weird

The sensor numbers in a packet are **raw**, not the friendly final values. The
soil-tension reading is electrical resistance, not kPa. The air-temp reading is a
"tick count" from the chip, not degrees. The tank reading is millimeters of empty
space, not gallons.

We do the conversion to human numbers later, on the server — not on the node. Why?
Because the conversion formulas (and the tank's gallons-per-inch curve) are things
we'll *refine over time*. If the node only ever sent "15.2 kPa," and we later
discovered our formula was 10% off, the original measurement would be gone. By
storing the raw resistance, we can re-run a better formula against years of old
data without touching a single node in the field. The raw reading is the durable
record; everything else is derived and re-derivable. (This is decision D1.)

## A real packet, byte by byte

Here's an actual tank-node packet from the test vectors (the first case in
`vectors/packet-v1.json`). 16 bytes, written in hex (each pair = one byte):

```
01 0a 6400 0100 740e 0001 0000 d204 9f96
```

Reading it left to right:

| Bytes | Field | Raw value | Means |
|---|---|---|---|
| `01` | proto_ver | 1 | "this is the v1 packet format" |
| `0a` | node_id | 10 | sent by node #10 |
| `6400` | fw_version | 100 | running firmware version 100 |
| `0100` | seq | 1 | this is message #1 from this node |
| `740e` | battery_mv | 3700 | battery at 3700 mV = 3.7 V |
| `0001` | channel_mask | (switch #8 on) | "I have the tank-distance sensor" |
| `0000` | fault_mask | (none) | nothing broke this cycle |
| `d204` | tank distance | 1234 | 1234 mm of empty space above the water |
| `9f96` | checksum | — | math over the 14 bytes before it |

> Why `6400` means 100 and not 6,400: the chip writes the *low* half of a
> two-byte number first (this is "little-endian," and it's just how the ESP32
> stores numbers). So `64 00` is read back-to-front as `00 64`, and hex `0064`
> = 100. You don't have to love it; you just have to know both sides do it the
> same way. The spec pins this so C++ and Python never disagree.

That's the whole thing. A garden-bed packet is the same idea with more switches
flipped on and more readings in the middle — 26 bytes instead of 16.

## What this task (#5) actually delivered

No firmware and no gateway code yet — just **the agreement and the proof**:

- **The spec** (`packet-v1.md`) — the exact rules, written down.
- **The test vectors** (`vectors/packet-v1.json`) — seven example packets with
  their exact correct bytes worked out ahead of time. When we write the C++ node
  code (#6) and the Python gateway code (#7), each one has to reproduce these
  exact bytes. That's how we guarantee the two sides can't drift apart: they're
  both graded against the same answer key.
- **The generator** (`tools/gen_vectors.py`) — the little script that produced
  that answer key, so we can regenerate it when we add a sensor later.

If you want to sanity-check it yourself: the bytes above should be the first
vector in `vectors/packet-v1.json`. Everything in this folder is just elaborating
on those two ideas — the channel mask and the fault mask — plus the boring-but-
necessary checksum and version bookkeeping.
