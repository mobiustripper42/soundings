# Chat ↔ Claude Code Handoff

*The sync point between open-ended research in Claude chat and the repo that
Claude Code edits. Hand-maintained — paste questions out, paste answers back in.*

---

## Why this exists

Claude Code and Claude chat are good at different halves of a hardware build,
and neither can see the other's context.

| | Claude Code (this repo) | Claude chat |
|---|---|---|
| Sees | The repo, the docs, the code, the git history | Nothing from this repo unless pasted |
| Good at | Editing files, writing and running code, keeping SPEC/DECISIONS/PLAN coherent | Live web search, datasheets, current prices and availability, vendor links, open-ended design conversation |
| Bad at | Knowing what a part costs today or whether it's in stock | Remembering what the project already decided |

This file is the interchange. **It is the only channel** — anything that matters
and lives only in a chat window is lost.

---

## The ritual

1. **Out.** Copy the brief in §4 into a chat window. It carries enough project
   context that chat can answer without seeing the repo.
2. **Back.** Paste chat's answers into the ledger in §3 — fill `Answer`, set
   `Status` to `answered`, stamp `Date`. Raw and unedited is fine; don't tidy.
3. **Promote.** Tell Claude Code *"handoff updated"*. CC reads this file, moves
   each answer to where it actually belongs (§5), sets `Status` to `promoted`,
   and updates the affected docs in one commit.

Answers live here only in transit. **A `promoted` row's real home is elsewhere**
— this file is a log, not a source of truth.

### Rules

- **Never let CC guess a part spec.** If a number isn't in the ledger as
  `answered` or `promoted`, it stays `[verify]` in the build plan. Cf. `CLAUDE.md`
  — don't write against a guess.
- **One ID per question.** Chat answers by ID so a partial reply still merges.
- **Cite the source.** A datasheet URL or vendor link in the `Answer` cell beats
  a bare number; it's what makes the answer re-checkable in a year.
- **Contradictions are findings.** If chat's answer conflicts with something in
  `SPEC.md` or `docs/tank-level-sensor.md`, say so in the answer rather than
  quietly overwriting. CC will surface it as a decision.

---

## 2. Status vocabulary

| Status | Meaning |
|---|---|
| `open` | Asked, no answer yet |
| `answered` | Answer pasted into the ledger, not yet moved into the real docs |
| `promoted` | Moved to its home doc by CC; the ledger row is now history |
| `blocked` | Needs a physical measurement or a purchase before it can be answered |

---

## 3. Question ledger

### Round 1 — tank node hardware (opened 2026-08-07)

Sourced from `docs/HARDWARE_BUILD_PLAN.md`. All eight block either an order or a
cut.

| ID | Question | Why it matters | Status | Answer | Date |
|----|----------|----------------|--------|--------|------|
| HW-01 | What antenna connector does the Heltec WiFi LoRa 32 V3 ship with (u.FL/IPEX vs SMA), and what 900 MHz antenna suits a fixed outdoor node? Does it need a pigtail to an SMA bulkhead? | Can't order antennas or the enclosure bulkhead without it | `open` | | |
| HW-02 | What deep-sleep current is actually achievable on a Heltec V3, and what has to be disabled to get there (OLED, Vext, LDO, onboard LED)? Real measured figures, not vendor claims. | `SPEC.md` §4 asserts 20–30 µA and a 2-year life. Unvalidated. | `open` | | |
| HW-03 | What is the A02YYUW's beam angle / cone half-angle? | Sets the minimum standoff tube ID for a given length — and therefore the hole-saw size. Nothing gets cut until this lands. | `open` | | |
| HW-04 | How long is the A02YYUW's stock cable, and can its UART be extended to ~tank height (several metres) at 9600 baud? Shielding needed? | The enclosure must sit low and shaded while the sensor sits in the lid of the tallest tank | `open` | | |
| HW-05 | What 2-cell parallel 18650 holder matches the Heltec V3's battery connector (type and polarity)? | Wrong connector is a soldering job or a returned part | `open` | | |
| HW-06 | Does the Heltec V3's onboard battery circuitry include adequate over-discharge protection, or is a separate LVC board required? | `SPEC.md` §4 lists a ~$3 LVC board; may be redundant | `open` | | |
| HW-07 | What's a reasonable option for measuring µA-range sleep current on a hobby budget? | Without it, HW-02 can't be validated and the 2-year claim stays a guess | `open` | | |
| HW-08 | What is the A02YYUW's power-up settling time, and what's the right read strategy (how many samples, median vs mean)? Does it free-run when powered? | Sets `t_active`, which lands directly in the battery-life equation | `open` | | |

**Also worth raising in chat, not blocking:** `docs/tank-level-sensor.md:45-49`
states a ~20–25 cm dead zone. Some A02YYUW specs suggest a much smaller blind
zone. If ours is wrong in the optimistic direction, the dead-zone clamp shrinks
and we get usable range higher in the tank. Worth a sanity check while HW-03 is
being looked up.

---

## 4. Brief to paste into chat

> Copy everything between the rules. It's self-contained.

---

I'm building a battery-powered wireless sensor node for a farm rain-catchment
tank and need help sourcing parts and reading datasheets. Please answer by ID,
cite datasheets or vendor pages where you can, and flag anything where the
common wisdom disagrees with the vendor's claims.

**The build:** one field node measuring water level in a cluster of three
plumbed-together rain tanks (2× 1100-gal vertical cylinders + 1× 330-gal IBC
tote, sharing one water level). It reports over LoRa to a gateway radio tethered
to a Linux server on the farm LAN. Read-only telemetry — it measures and
reports, it never actuates anything.

**Hardware:**
- MCU + radio: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262), one board. WiFi
  disabled, deep sleep between readings.
- Sensor: A02YYUW ultrasonic distance sensor (UART), mounted in the tank lid
  pointing down at the water, recessed in a short PVC standoff tube to fight
  condensation.
- Power: 2× 18650 in parallel (~6000 mAh), no solar. Target ~2 years between
  battery swaps, waking every 10–15 minutes.
- Enclosure: IP65, light-colored, mounted low and shaded (the cells degrade
  above ~45 °C), PG7 glands pointing down.
- Same Heltec V3 board as the gateway radio, USB-serial to the server.

**Questions:**

- **HW-01** — What antenna connector does the Heltec V3 ship with (u.FL/IPEX or
  SMA)? What 900 MHz antenna suits a fixed outdoor node, and do I need a pigtail
  to an SMA bulkhead through the enclosure wall?
- **HW-02** — What deep-sleep current is realistically achievable on a Heltec V3,
  and what has to be disabled to get there (OLED, Vext rail, LDO, onboard LED)?
  I've seen 20–30 µA claimed; I want real measured numbers and the gotchas.
- **HW-03** — What is the A02YYUW's beam angle (cone half-angle)? I need it to
  size the PVC standoff tube — too narrow or too long and the cone hits the tube
  wall instead of the water.
- **HW-04** — How long is the A02YYUW's stock cable? Can its UART be extended to
  several metres at 9600 baud (the enclosure sits low and shaded while the sensor
  is in the lid of a tall tank)? Does it need shielded cable?
- **HW-05** — What 2-cell parallel 18650 holder matches the Heltec V3's onboard
  battery connector? I need the connector type and the polarity convention — the
  V3 is reportedly easy to get backwards.
- **HW-06** — Does the Heltec V3's onboard battery circuitry include adequate
  over-discharge protection for unattended 18650s, or should I add a separate
  low-voltage-cutoff board?
- **HW-07** — What's a reasonable way to measure µA-range sleep current on a
  hobby budget? A regular multimeter's burden voltage makes this unreliable and I
  can't validate a 2-year battery claim without it.
- **HW-08** — What is the A02YYUW's power-up settling time, and what's the right
  read strategy — how many samples, median or mean? Does it free-run
  continuously when powered? I plan to switch its power rail so it only draws
  during the wake window, so settling time goes straight into my battery budget.

**Bonus sanity check:** my notes say the A02YYUW has a ~20–25 cm blind zone near
the transducer. Is that right, or is the real figure smaller? It changes how much
of the top of the tank I lose.

---

## 5. Where answers go when promoted

CC moves each `answered` row to its real home and marks it `promoted`.

| Answer type | Home |
|---|---|
| A part choice, quantity, or spec | `docs/HARDWARE_BUILD_PLAN.md` §4 BOM — the `[verify]` tag comes off |
| A dimension or physical constraint | `docs/HARDWARE_BUILD_PLAN.md` §5 mounting |
| A current, timing, or battery figure | `docs/HARDWARE_BUILD_PLAN.md` §6, and `SPEC.md` §4 if it contradicts the estimate there |
| A sensor behaviour that changes the math | `docs/tank-level-sensor.md` |
| A choice with a real tradeoff and a rejected alternative | A new `DEC-nnn` in `docs/DECISIONS.md` |
| Something that resolves a deferred decision | The `SPEC.md` §12 register, struck through with a pointer |

**Contradictions get surfaced, never silently applied.** If an answer conflicts
with an existing `[settled]` item, CC raises it rather than editing over it.

---

## 6. Going the other way — CC to chat

Occasionally chat needs repo state to answer well. Paste it in directly; there's
no ceremony. The three that carry the most context per line:

- `docs/tank-level-sensor.md` — the whole sensor design, ~120 lines
- `docs/HARDWARE_BUILD_PLAN.md` §4 and §9 — the BOM and what's still open
- `SPEC.md` §4 — the node hardware spec and the power claim

---

## 7. Round log

| Round | Opened | Topic | Closed |
|-------|--------|-------|--------|
| 1 | 2026-08-07 | Tank node hardware — HW-01…HW-08 | — |
