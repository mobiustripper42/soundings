# Tiller: Make soundings *forecast* water need, not just log it — and tell tinkle when *not* to water

*Overnight idea for soundings — Eric is the gate. Draft only.*

---

## The pitch

### The idea

soundings, as the README frames it, is a measure-and-store system: soil tension,
soil temp, tunnel air temp/humidity → LoRa → Pi → time-series DB. That's
**reactive** — you open a graph, notice Bed 2 is dry, and go water it.

The idea is to add one derived layer on the Pi gateway that flips soundings from a
logger into a **forecaster**: *"Red Bed 2 reaches 40 cb in ~16 h."* It needs **no
new hardware** — it runs on data you're already collecting:

1. **Observed drydown slope.** Each Watermark bed already produces a tension trace.
   The recent slope (cb/hour) linearly extrapolated to your irrigation threshold
   gives a baseline "hours to dry."
2. **VPD adjustment.** You're already logging tunnel air temp + RH per tunnel.
   Those two compute **vapor-pressure deficit (VPD)** — the dominant driver of
   transpiration/evaporation in an enclosed tunnel. Today's VPD scales the
   extrapolation: hot/dry day → the bed dries faster than the trailing slope says;
   cool/humid → slower. The forecast is **observed-slope-first, VPD-adjusted** —
   it degrades to plain linear extrapolation if the air node is down, so it never
   hard-depends on the model.

Then the payoff that makes it worth more than a nicer dashboard: soundings becomes
the **advisory seam to tinkle**. Not a control wire — tinkle keeps its fail-dry,
no-network local autonomy (DEC-012) untouched. soundings just publishes a hint
tinkle *may* read and *may* ignore: *"Bed 2 won't cross threshold before your next
scheduled run — you can skip it."* That unlocks **deficit irrigation**: mild,
controlled water stress on tomatoes concentrates sugars and improves flavor — a
real horticultural lever you **cannot pull with a dumb schedule**, because skipping
a watering safely requires knowing the bed isn't actually dry. soundings is the
only thing on the farm that can know that.

### Why it's worth it

- **Zero marginal hardware.** The air temp/RH channel reads like "tunnel comfort
  monitoring." It's actually the ET model's input. The capability is hiding in
  data you're already paying to collect and transmit.
- **It's the honest way to close tinkle's loop.** tinkle deliberately deferred
  closed-loop (`scheduled-now/closed-loop-later`) and is network-averse by design.
  A naive "sensors drive the valves" wire would violate fail-dry. An *advisory hint
  with tinkle holding veto* gives you 90% of the value without touching tinkle's
  safety model. This reframes "closed loop" from a V2 feature into a seam you design
  for now.
- **Why now:** it shapes the **schema, sample cadence, and sensor co-location
  before you build.** A pure-telemetry design might log at the wrong interval or not
  guarantee a paired air node per tunnel — and you'd discover that after deployment.
  And your own rollout plan hands you a free calibration window: the **Green Tunnel
  winter shakedown** (per the README) has no crop at stake, so it's the perfect
  season to log tinkle's irrigation events against tension response, learn each
  bed's drydown curve, and validate the VPD model — so it's *tuned* by the time Red
  goes live at the 2027 transplant. Spec it now; the shakedown does double duty.

### Why he hasn't already

The README says "reporting to a Raspberry Pi gateway and a local time-series
database." That's a **logger mental model** — measure, store, graph. The leap is
seeing that (a) the air-temp/humidity channel is an *evapotranspiration input*, not
just comfort data, and (b) soundings' real product is a *prediction*, with the
sibling actuator (tinkle) as its consumer. Nothing here was tried and rejected —
it just sits one reframe past where "sensor mesh → database" naturally stops. The
docs are still seed placeholders, so it's the cheapest possible moment to fold it in.

---

## Build handoff

**Scale:** this is a spec-and-schema shaping task plus one Pi-side derived layer —
not a firmware change to the battery nodes. Nodes stay dumb (measure + transmit);
all forecast math lives on the Pi where there's power and the TSDB already sits.

### Approach

- **Keep nodes dumb.** No ET math on battery/solar LoRa nodes — it would cost power
  budget for zero benefit. Nodes report raw tension (resistance/cb), soil temp, and
  (air nodes) temp + RH. The forecast is a *derived view* over the time series.
- **Forecast = observed-slope-first, VPD-adjusted, model-optional.** Order of
  fallback so it never hard-fails:
  1. Linear extrapolation of the trailing tension slope (e.g., last 6–12 h) →
     hours-to-threshold. Works with only the Watermark + soil-temp node alive.
  2. Scale that rate by a VPD factor from the tunnel air node when present.
  3. (Later) swap the linear model for a learned per-bed drydown curve from
     shakedown data.
- **Temperature-compensate the Watermark reading at ingest**, not in the forecast.
  Watermark resistance→centibar depends on soil temp; you have the soil-temp sensor
  co-located precisely so the cb value is trustworthy. Do this conversion once, on
  ingest, and store calibrated cb — everything downstream assumes it.
- **Advisory seam to tinkle is one local endpoint, codes/ids only.** A small local
  HTTP/MQTT topic on the Pi: `{bed_id, current_cb, hours_to_threshold,
  recommend_skip_next: bool, as_of, stale_after}`. tinkle *polls or ignores*; if the
  hint is stale (`as_of` too old) or absent, tinkle falls back to its schedule.
  tinkle never blocks on soundings. This is the whole fail-safe contract.

### File-by-file (spec stage — most of this is docs, by design)

- `README.md` — one line: soundings forecasts threshold-crossing and exposes an
  advisory irrigation hint; it does not control valves.
- `docs/SPEC.md` — fill the placeholder with the real project. Add **Core Concepts**:
  *calibrated tension*, *drydown slope*, *VPD*, *threshold forecast*, *advisory hint*.
  Under **V1 Scope**, add a phase for the derived forecast layer. Under **Not V1**,
  explicitly park: closed-loop *control* (soundings commanding valves), any
  ET model needing solar radiation / a pyranometer, learned ML curves (V2 — start
  linear).
- `docs/DECISIONS.md` — new DECs:
  - *Forecast runs on the Pi gateway, never on nodes* (power budget).
  - *Observed-slope-first, VPD-adjusted, model-optional* (graceful degradation).
  - *soundings↔tinkle seam is advisory, not control; tinkle holds fail-dry veto*
    (mirror/cite tinkle DEC-012). **Cross-link this in tinkle's DECISIONS too** so
    the two repos agree on the seam.
  - *Watermark temp-compensation happens at ingest; store calibrated cb.*
  - *Hint payload carries codes/ids + timestamps only — no prose, no commands.*
- `docs/PROJECT_PLAN.md` — add the forecast layer + the advisory endpoint as tasks;
  add a *winter-shakedown-as-calibration* task (log tinkle events vs. tension,
  derive per-bed drydown curves).
- Schema (whenever the TSDB lands): ensure a **paired air node per tunnel** is a
  first-class requirement, store **calibrated cb** alongside raw, and pick a sample
  cadence that survives slope estimation (don't under-sample the drydown).

### Gotchas / risks

- **Open-field ET models assume solar radiation** you won't have in a tunnel. Don't
  reach for full Penman-Monteith. VPD-scaling of an *observed* slope (or
  temp-only Hargreaves as a sanity bound) avoids the radiation dependency entirely.
  The observed slope is the anchor; VPD only bends it.
- **Watermark sensors are noisy and slow to respond** near saturation and at very
  high tension. Smooth before differentiating the slope; don't forecast off two raw
  points. Define a valid tension band for the linear model.
- **Don't let the advisory become a dependency.** The entire safety story is that
  tinkle ignores a missing/stale hint and falls back to schedule. Build and test the
  stale/absent path *first*, before the happy path.
- **VPD needs RH you can trust** — tunnel RH sensors drift and saturate. If RH is
  suspect, the forecast must fall back to pure slope, not emit garbage.
- **Two repos, one contract.** The seam payload is a shared interface across
  soundings and tinkle. Version it; write it down in both DECISIONS files.

### Done when

- SPEC/DECISIONS/PLAN describe a forecaster with an advisory tinkle seam, and the
  "Not V1" list parks control + radiation-ET + ML curves.
- There's a written, versioned hint-payload contract referenced from *both*
  soundings and tinkle DECISIONS.
- A worked example in the spec: given a tension trace + a VPD series, the doc shows
  the hours-to-threshold the forecast would produce, including the slope-only
  fallback when the air node is absent.
- (When code exists) the stale/absent-hint path is the first tested behavior, and
  tinkle provably falls back to schedule when soundings is dark.

### Kickoff

> Read `tiller-proposals/2026-06-11-vpd-irrigation-forecast.md`. We're adopting the
> forecaster framing for soundings. Start by filling `docs/SPEC.md` from the README
> vision plus this proposal — real Overview, Core Concepts (calibrated tension,
> drydown slope, VPD, threshold forecast, advisory hint), V1 phases, and a "Not V1"
> that parks valve control, radiation-based ET, and ML drydown curves. Then draft the
> new DECISIONS entries, including the advisory soundings↔tinkle seam (cite tinkle
> DEC-012). Keep nodes dumb; all forecast math is Pi-side. Plan it and wait for my go
> before writing the DECISIONS.
