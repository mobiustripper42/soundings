# OTA firmware update — operator runbook

Update a field node over WiFi without walking to the tank. First done successfully
**2026-08-25 at the bench**: v261 → v262, 923,872 bytes verified and flashed in 6.2 s.

Mechanics: `contracts/firmware-manifest-v1.md` (the format),
`contracts/downlink-v1.md` (bit 0, the trigger), DEC-011 (why the trigger is declarative).

**The node's WiFi is off at every other moment of its life.** It comes up only when the
gateway has told it there is something to fetch, and it goes down before the node sleeps.

---

## 1. Bump the version. This is the step that gets forgotten.

`firmware/src/esp32/main.cpp`:

```cpp
constexpr uint16_t kFwVersion  = 0x0106;   // ← bump it
```

⚠ **The daemon compares `fw_version` for inequality against the manifest.** If you publish
a new image under a version some node already reports, that node believes it is current
and will never fetch. Nothing errors; nothing happens. `FW_BUILD` (sha + timestamp) tells
two builds of the same version apart *afterwards*, but it is not on the wire and the
update mechanism cannot see it.

## 2. Build

```bash
cd ~/soundings/firmware
~/.platformio/penv/bin/pio run -e node          # build ONLY — no -t upload, no cable
```

The build prints its share of the OTA slot and **fails loudly** if the image outgrows it:

```
fw_build_id: firmware.bin 922896 bytes — 28% of the 3342336-byte OTA slot
fw_build_id: archived build -> build_archive/soundings-node-<sha>-<timestamp>.bin
```

## 3. Publish

```bash
cd ~/soundings/gateway
.venv/bin/python tools/publish_firmware.py \
    --bin ../firmware/.pio/build/node/firmware.bin \
    --version 262 \
    --dir /srv/soundings-firmware/firmware
```

It copies the image in first, then writes the manifest **last and atomically**. Until that
final rename the release is invisible: a node polling mid-copy sees the previous one, which
is correct rather than merely safe.

⚠ **`--version` must match the `kFwVersion` you just built.** Nothing checks this — the
tool cannot read a constant out of a compiled binary. Get it wrong and either nothing
updates, or nodes update and then immediately look stale again.

## 4. Serve it

A **static server, separate from the gateway daemon** — a process that both parses radio
traffic and serves firmware has two reasons to be restarted.

```bash
cd /srv/soundings-firmware && python3 -m http.server 8080 --bind 0.0.0.0
```

⚠ **Serve the PARENT of the firmware directory.** The node builds its URL from
`node_secret.ini` as `http://<host>:<port><path>manifest.txt`. With `path = /firmware/`
the document root must contain a `firmware/` directory. Getting this wrong produces a 404
the node reports and then sleeps through — it cost a bench cycle here.

Layout:

```
/srv/soundings-firmware/            ← the document root you serve
└── firmware/                       ← `path` in node_secret.ini
    ├── manifest.txt                ← written last; the trigger
    ├── soundings-node-262.bin
    └── soundings-node-261.bin      ← keep it; see Downgrading
```

## 5. Point the daemon at the same directory

The daemon derives the flag by comparing each node's reported `fw_version` against the
manifest, so it reads the **manifest directory**, not the document root:

```bash
cd ~/soundings/gateway
.venv/bin/python tools/bench_reply.py --port /dev/ttyUSB0 --ota-dir /srv/soundings-firmware/firmware
```

⚠ **The daemon validates before it flags.** If the named image is absent, the wrong
length, or fails its hash, **no flag is sent** and it logs why. A node is never woken onto
WiFi for an image the gateway could already see was broken.

## 6. Wait

Up to one cadence — 15 minutes in the field, 20 seconds on a bench build. Watch the node:

```
DOWNLINK heard at 621 ms: node=7 flags=0x0001
ota: update flagged; bringing WiFi up
ota: wifi up in 999 ms, ip 192.168.50.84, rssi -45 dBm
ota: flashed v262 (923872 bytes in 6199 ms) — rebooting
```

and then, on the next cycle, the daemon:

```
node 7 seq 9 fw 262 -> downlink flags=0x0000
```

**`flags=0x0000` is the confirmation.** The node reported the new version, the daemon
compared it to the manifest, and the flag cleared itself. There is no acknowledgement in
this protocol because that line *is* the acknowledgement.

---

## Measured, at the bench

| | |
|---|---|
| WiFi cold join | **999 ms** typical; **4150 ms** on the first join after a flash |
| Download + SHA-256 + flash, 923 KB | **6.2 s** |
| Total awake during an update | ~7.5 s, against a normal wake of ~2 s |

An update costs roughly four ordinary wakes' worth of awake time. Rare enough not to
matter; the 15 s join timeout exists because of that 4150 ms outlier.

## What the guardrails do, so a refusal is not a surprise

- **Nothing happens at all.** Either no manifest, or its version equals what the node
  reports, or the daemon found the image broken. Check the daemon log — it says which.
- **404 on the manifest.** The document root does not contain the `path` from
  `node_secret.ini`. See step 4.
- **`ota: flagged, but this build has no credentials`.** Built without
  firmware/node\_secret.ini. That compiles by design (empty strings), and this is the
  node refusing to act on it rather than fetching a nonsense URL.
- **`SHA-256 MISMATCH — image discarded, not flashed`.** What arrived over the air did not
  match the manifest. The image is discarded **before** `Update.end()` makes anything the
  boot target, so the running firmware is untouched. It retries next cycle.
- **WiFi join timeout.** Logged with the elapsed time, then the node sleeps normally. The
  flag is still set next cycle, so it self-retries — no retry code exists.

**Every failure ends the same way: WiFi off, sleep, try again in fifteen minutes.** The
flag persists because it is derived from a version mismatch that is still true.

## Downgrading

Republish the older manifest. `version` is compared for **inequality**, not ordering, so a
node will happily take an older image — which is deliberate, because **there is no
rollback** (operator call, 2026-08-23). Keep old `.bin` files on the server; republishing
one is the only downgrade path that exists.

## Recovery of last resort

A *valid but broken* image — one that boots and then misbehaves — reverts nothing. The
stock bootloader has no rollback and none was added: bricking-means-USB is an accepted
cost on the field node, and nothing in this design may be complicated to avoid it.

Recovery is the cable:

```bash
~/.platformio/penv/bin/pio run -e node -t upload --upload-port /dev/ttyUSB1
```

⚠ **This is a trip to the tank.** OTA is a convenience on top of USB, not a replacement
for it — which is the reason every check above is worth its cost.
