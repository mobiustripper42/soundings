---
session: 9
dev: eric
slug: sync-shell-and-its-alive
branch: task/sync-shell-and-its-alive
started: 2026-08-24T15:51:14Z
ended:
points:
pr_numbers: [80]
status: open
transcript: /home/estoffer/.claude/projects/-home-estoffer-soundings/a98a09ef-75f3-50fa-9fd7-e50cad2a053a.jsonl
---

# Session 9 — sync-shell-and-its-alive

<!-- Task blocks appended by /kill-this, one per task. -->

## Housekeeping — a WiFi password reached a public repo

⚠ **`firmware/wifi_secret.ini` was committed to `origin/main` through the GitHub web UI**
(commit `eb98539`, 12:54 EDT) on a repo `gh repo view` confirms is PUBLIC. Real credentials,
not the template placeholders. **The web UI honours no `.gitignore`** — the ignore rule
added minutes earlier on the task branch could not have stopped it.

- Removed from the tip, then the two commits (create + delete) were dropped entirely.
  They cancelled exactly — `git diff b83b502 origin/main` was empty — so no history
  surgery was needed beyond resetting `main` to `b83b502`. 0 forks, 0 watchers.
- ⚠ **A force-push does not revoke anything.** The password was public for ~20 minutes on
  a repo in GitHub's public event firehose. **Rotation is the fix and is still outstanding**
  (operator doing it on returning home). The orphaned SHA also still serves by direct URL
  until GitHub Support purges it.
- ⚠ **A bare `git push --force-with-lease` was attempted and git refused it** — the branch
  name did not match its upstream. It was safe only by accident: nothing was committed on
  the task branch yet, so `HEAD` still *was* `b83b502`. With work committed it would have
  pushed the feature branch onto `main`. The explicit refspec is the only safe form.

## Housekeeping 2 — issue #76 split, and the estimate doubled

- **#76 rewritten as 3.9c** (the downlink link, `points:5` → `points:8`); **#79 is new** —
  3.9d, OTA over WiFi, `points:8`. Same shape as the 3.8a/3.8b and 3.9a/3.9b splits.
  Phase 3 now stands at **35 points across seven open issues**, up from 24.
- ⚠ **The re-estimate is not bookkeeping, and its cause is worth keeping.** 3.9c was priced
  at 5 as "OTA over WiFi". Building it revealed the transport OTA rides on had never worked
  gateway→node and was losing 78% of uplink packets. **A task priced before anyone knew the
  layer beneath it was broken.** That is the second such split this phase.
- **Operator design calls recorded:** WiFi credentials come from a build-time
  `firmware/wifi_secret.ini` via `extra_configs`, **superseding #76's NVS provisioning** —
  simpler, and a password change costs a USB trip either way. Manifest is line-oriented
  `key: value`, JSON as fallback. The listen-every-Nth-wake switch belongs in 3.9d, not
  later, because it lives in the same function OTA modifies.

## Task 1: Phase 3.9c — the downlink link, measured

**The first task in this project whose deliverable is a measurement.**

**Completed:**
- **DEC-011** (gateway listens continuously; the re-arm invariant belongs to the driver;
  downlinks are declarative) and a **DEC-010 amendment** (the airtime table governs the
  *receiver* too; the receive window measured, and 250 ms confirmed).
- **`IRadio` gained a second receive mode** — `startReceive()`/`poll()` for the gateway.
  The node keeps its bounded `receive(timeoutMs)`, which is DEC-006's safety property.
- **`src/core/gateway_bridge.{h,cpp}`** — the reply window, in core so it is host-tested.
- **`src/core/idownlinkhandler.h`** + RunCycle wiring — the OTA seam, called before the
  sleep, because on hardware there is no after.
- **The daemon can send at all now**: `framing.encode()`, `SerialDownlinkSink`,
  `Gateway(respond=)`. It could decode this envelope but never write one.
- **`gateway/tools/bench_reply.py`** — stdlib-only bench instrument, no pyserial.
- Suites: **143 native** (was 128), **165 pytest** (was 146), five envs build, gates green.

**⚠ The bug, and it is the lesson: it was duty cycle, not RF.** The gateway ran the NODE's
`receive(200 ms)` in a loop against a packet that is 165 ms on air (DEC-010's own table), so
a packet decoded only if it began in the first ~35 ms of a slice. **Predicted 17.5%, measured
2 of 9, twice — at RSSI −60 dBm and SNR +6 dB, twenty dB above the floor.** The failures
presented as `CRC_MISMATCH`, which reads as interference and is what a *truncated* packet
looks like. **A packet cut off by its own receiver is indistinguishable from a corrupted one
unless you know the airtime.** After: uplink 9 of 9, round trip 8 of 8 at 613 ms with a
**1 ms spread**.

**The airtime table is now load-bearing in a place it was never written for.** It answered
"does a packet fit the dwell limit"; it also answers "how long must a receiver listen
continuously". Anything setting a listening interval anywhere must be checked against it.

⚠ **Three diagnoses were wrong before one was right, and each cost time:** saturation (RSSI
said no), the node failing to transmit (9 of 9 accepted), and the chip's hardware timeout
(it was RadioLib's *software* timeout, `SX126x.cpp:300`, then a forced standby at `:307`;
`STOP_TIMER_ON_PREAMBLE` is defined and never issued in 7.7.1). **The prediction matched for
a mechanism I had guessed wrong** — only reading the pinned library settled it.

⚠ **The gateway sketch printed nothing on boot**, so a dead radio, a crashed sketch and an
unplugged cable were one symptom. Adding one line is what exposed the next bug.

⚠ **Opening a tty from Python asserted DTR+RTS and parked the ESP32 in its ROM bootloader.**
Termios settings came back byte-identical to a working `stty raw`; reads returned on
schedule, 385 of them, all empty. `cat` was unaffected because the shell leaves the modem
lines alone. **The tell was the missing boot banner** — which only existed because it had
been added an hour earlier.

⚠ **A fake under-modelled the hardware and its test asserted nothing.** `FakeRadio` re-armed
but never modelled the standby drop `transmit()` performs, so deleting the re-arm changed
nothing and a test named for the invariant pinned none of it. **Fifth consecutive task with a
false green of this family, and the first where the fake — not the test — was the liar.**
Mutation caught it; reading would not have.

**Code review:** 3 findings, all real. **The one with teeth: I stated a bench simplification
as a property.** The reply window does not poll the radio for up to 400 ms, and
`RX_TIMEOUT_INF` auto-restarts into the *same buffer*, so a second node's packet overwrites
an unread one. My comment said "nothing else will transmit for fifteen minutes" — true of the
node holding its window, false of the 2–3 nodes `SPEC.md` §13 schedules **next**. It is a
*software* gap that bites earlier than the RF collision DEC-011 flagged. Comment and Revisit
now say so; the fix wants its own tests and a multi-node bench. Also dropped a premature
`.gitignore` entry, kept `*secret*.ini` with its reason stated, and documented the single
`delay()` as a narrow exemption. `/security-review` not run — no blast-radius trigger, and
`.claude/CLAUDE-context.md` still carries no `## Blast-Radius Triggers` section.

**PR:** [PR #80](https://github.com/mobiustripper42/soundings/pull/80) — `closes #76`
verified via GraphQL to resolve to issue #76 only; issue #79 stays open.
**Points:** 8
**Branch:** task/3.9c-ota-over-wifi
**Opened at:** 2026-08-24T23:14:00Z

**Next Steps:**

**Context:**
