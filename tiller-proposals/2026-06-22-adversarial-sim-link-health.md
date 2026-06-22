# Tiller: Teach the gateway to tell a dead node from a healthy one — by making the simulator lie to it first

*Overnight idea from Tiller for **soundings**. Draft only — you're the gate.*

---

## The pitch

### The idea

The Phase 1 spine works: a drydown curve moves on the dashboard with zero hardware. But the synthetic fleet (`FleetEmitter`) only ever emits **clean, monotonic, valid frames** — every `seq` increments by one, nothing is lost, duplicated, reordered, or reset. And the gateway, given that perfect stream, does the matching minimum: `decode → stamp received_at → publish → count`. The packet header carries a per-node `seq` field the contract added *explicitly* "for dedup + loss detection" (`packet-v1.md` line 44) — and **no code anywhere consumes it.** It's parsed onto `Reading`, serialized in `to_dict()`, and thrown away.

The idea is two halves of one organism. First, an **`ImpairingSource`** — a deterministic, seed-driven decorator over `IPacketSource` that injects what a real LoRa link does: packet loss, duplication, reordering, and node-reset (seq restarts at 0 after a brownout). Second, a **`SeqTracker`** — an observe-only state machine the gateway runs between decode and publish that finally *consumes* `seq`: per-node dedup, wrap-safe gap/loss counting, reset detection. Then the payoff that makes it more than a test fixture: **publish per-node link-health as a first-class reading** on the same MQTT path as soil moisture — loss rate, gaps, resets, last-seen. The gateway stops forwarding telemetry and starts *interpreting* it. "Read-only telemetry" becomes "telemetry that tells you node 3 is dying before it goes dark."

### Why it's worth it

The whole project thesis is *"prove the stack in simulation now, so the winter bench is tuning, not authoring."* Right now the simulator only proves the **happy path** — it can only ever tell you good news. The two V1 requirements that *depend on a degraded stream* are currently unbuildable and untestable: SPEC §7's **"node silent > 45 min" alert** is loss detection (you can't test an alert that can never fire against a source that never loses anything), and SPEC §10's **±30 s wake jitter** is a number you chose but never measured. The bench will be the **first time the gateway ever meets a corrupted or missing frame** — which is exactly the "authoring at the bench" failure software-first exists to prevent. And the link-health layer is nearly free: once `SeqTracker` exists for Phase 2/3, you're not building new machinery, you're *refusing to throw away the byproduct*. On a real farm that byproduct is the difference between "the back-forty sensor died three weeks ago and nobody noticed" and "node 3's loss has climbed for four days — go check the enclosure gasket."

### Why he hasn't already

Because the clean emitter is the *correct* thing to build first — you need a happy path before you can have an unhappy one. And once it worked (drydown curve moving, Phase 1 "done when" met), the simulator **looked finished.** The unused `seq` field sits quietly in the header doing nothing; resilience is filed as "Phase 3's problem" (the plan promises the gateway *handler* for lost packets, but schedules no adversarial *generator* to drive it — you can't write a regression test for graceful loss handling against a source that never loses anything). Nothing was tried and rejected. It sits one reframe past where "the spine works" naturally stops: a simulator that only emits good news cannot prove the one thing it exists to prove.

---

## Build handoff

**Scale:** a contained two-component feature on the gateway (`ImpairingSource` + `SeqTracker`), plus a thin link-health publish path. First slice below is one coherent subsystem; a second slice and the explicitly-deferred facets are sketched so they don't get lost. No firmware changes, no contract change, no new dependency.

### Approach — the shape and the key decisions

- **`ImpairingSource(inner: IPacketSource, profile, seed)` is a sibling adapter in the shipping package** (`source.py` neighborhood), not test-only code. It depends only on the public `IPacketSource` contract and the wire format. It adds **zero daemon hot-path cost** unless explicitly constructed — prod wiring composes `FleetEmitter` (later the real radio adapter) directly and never references it. This is what lets it double as a permanent, opt-in spine **"chaos mode"** (a `--chaos <profile>` flag on `spine.py`) without bloating the daemon. Frame-level impairments (loss/dup/reorder/reset) operate on whole frames; byte-level ones (truncation/bit-flip) are a *second slice* — they exercise the parser's `PacketError` taxonomy, a separable concern from seq-state.
- **`SeqTracker` is an injected, observe-only collaborator** — **not** inlined into `Gateway`, **not** a stream filter. `Gateway.run()` stays one line longer: `decode → stamp → tracker.observe(reading) → publish(...) → tally`. The tracker is a pure state machine over `(node_id, seq, received_at)`, unit-testable in isolation with hand-fed sequences, touching neither network nor publisher. **It never mutates or suppresses telemetry** — raw readings stay the durable record; link stats are a sidecar, never a filter.
- **Link-health rides the existing pipeline as just another record.** The tracker's per-node counters get published (periodically, or alongside each reading) through the same `publish(dict)` → MQTT → `ingest` → VictoriaMetrics path, yielding `soundings_link_*{node="N"}` series Grafana already knows how to draw. **Inference only:** the gateway computes "node 3 looks unhealthy" *solely from the seq stream it can actually see* — the same information it will have on the farm. The moment it reads the simulator's internal "this node is dying" flag, you've built a demo, not a diagnosis.

### File-by-file (gateway; first slice)

- **`gateway/soundings_gateway/source.py`** — add `ImpairingSource(IPacketSource)`. Constructor takes the wrapped source, a seeded `random.Random`, and an impairment profile (loss probability, dup probability, reorder window/probability, reset schedule per node). `__iter__` pulls from `inner` and applies impairments deterministically from the seed. Keep profiles as small dataclasses; ship 2–3 named ones (see fixtures). Frame-level only this slice.
- **`gateway/soundings_gateway/seqtrack.py`** (new) — `SeqTracker`. Per-node state: `last_seq`, a small dedup window (recently-seen seqs), and counters `received / duplicates / gaps_lost / resets / last_received_at`. `observe(reading)` updates state and returns a small verdict (`new` / `duplicate` / `reset`) the gateway can use. A `health(node_id)` / `snapshot()` accessor produces the publishable per-node dict.
- **`gateway/soundings_gateway/gateway.py`** — inject an optional `SeqTracker`; call `tracker.observe()` after stamping. On a `duplicate` verdict, still publish the *reading* (dedup is a stat, not a drop, in V1 — don't silently lose data) but mark it. Tally unchanged.
- **`gateway/soundings_gateway/ingest.py`** — teach `reading_to_line` (or a sibling `health_to_line`) to emit the link-health record as line protocol: `soundings_link,node=N loss=…,dups=…,resets=…,gap=… <ts>`. Keep faulted-channel and battery rules intact.
- **`gateway/tests/test_seqtrack.py`** (new) — the crown jewel. Hand-feed sequences and assert exact counts: clean run → zero loss/dups; dropped frames → exact gap count; duplicated frame → counted, not double-published; **seq wrap at 0xFFFF → counted as small loss, never a 65 k spike**; **reset (low seq after high, outside the wrap window) → one `reset` event, loss counter UNTOUCHED.**
- **`gateway/tests/test_spine.py`** — add an end-to-end case: `ImpairingSource(FleetEmitter(...), profile=lossy, seed=…)` → gateway+tracker → assert decoded < emitted, gaps detected, and a link-health line lands.

### Gotchas / risks

- **Wrap vs. reset is the load-bearing detail — get it wrong and you manufacture phantom 65 k-loss spikes.** Compute gap as a modular delta `(seq - last_seq) & 0xFFFF`; a *small* positive delta (≤ a sane window, e.g. 256) is normal loss. A low absolute seq arriving after a high `last_seq`, *outside* that window, is a **reset** (re-baseline, emit a reset event, do **not** add to loss). The `node-reset` impairment exists precisely to drive this test.
- **No RTC on field nodes** — `received_at` is the gateway's only clock. Dedup and ordering are **seq-keyed, never time-keyed**; time is only for the (deferred) collision facet and for the silent-node alert's wall-clock gap.
- **Keep the collision/airtime facet OUT of the live source.** Quantifying the ±30 s jitter needs LoRa time-on-air (payload length + SF/BW), and D3 (radio params) is deferred. If you build it, build it as a **standalone offline analysis module** over fixtures with SF/BW as *labeled, swept inputs* — never a collision channel wired into the runtime decorator, never a hardcoded radio constant. That keeps DEC-001 honest: build the machinery, defer the decision.
- **Don't let `SeqTracker` start *correcting* the stream** (reordering, suppressing). Observe and count only.
- **Cut the CRC-pass bit-flip theater.** A bit-flip that survives CRC (~1/65536) yields a silently-wrong reading you can't detect *or* act on in a read-only V1 — it exercises a branch no alert or dashboard consumes. CRC-drop is already covered by `test_packet.py`. If you can't bear to lose it, park it as a one-line V2 hardening note in SPEC §12, not code.

### Done when

- `SeqTracker` consumes `seq`: dedup, wrap-safe gap/loss, and reset detection all unit-tested, with the **reset-is-not-65k-loss** case green.
- `ImpairingSource` deterministically injects loss/dup/reorder/reset from a seed; an end-to-end `test_spine` case proves the gateway survives a hostile stream (decoded < emitted, gaps counted, daemon never crashes).
- Per-node link-health lands in the DB as its own series and is visible in Grafana.
- The daemon's default/prod wiring is unchanged — chaos is opt-in.
- **Deferred, on purpose (write these into SPEC §12 / the Phase-4 issue, don't build now):** byte-level impairments (truncation/bit-flip → second slice, tests the parser taxonomy); the ±30 s-jitter collision-rate study (offline module, PHY fenced); named "field-day" fixtures (dead-battery, colliding pair, wet-enclosure) wired to the Phase-4 silent-node + low-battery alerts when those dashboards exist.

### Kickoff

> We're making the soundings simulator adversarial so the gateway can be hardened before the bench. Read `tiller-proposals/2026-06-22-adversarial-sim-link-health.md`. Start with the first slice: a `SeqTracker` (observe-only: per-node dedup, wrap-safe gap/loss counting, reset detection) plus an `ImpairingSource` decorator over `IPacketSource` (deterministic, seeded; loss/dup/reorder/reset only). Wire `tracker.observe()` into `Gateway.run()` without making it a stream filter, and add the unit test that proves a seq reset reads as one reset event with the loss counter untouched (the 65k-spike trap). Hold the collision/airtime facet and byte-level corruption for a second slice. Plan it and wait for my go before writing code.

---

🤖 Tiller · rotation pick 8/11 · 2026-06-22
