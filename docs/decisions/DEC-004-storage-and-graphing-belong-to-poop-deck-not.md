---
id: DEC-004
title: "Storage and graphing belong to Poop Deck, not soundings (resolves D1 + D6)"
topic: "Server, storage & display"
---

## DEC-004: Storage and graphing belong to Poop Deck, not soundings (resolves D1 + D6)

**Decision:** Soundings does not run its own time-series DB or Grafana. Storage and dashboarding move to **Poop Deck**, the farm's shared telemetry backend (TimescaleDB + Grafana, fed over MQTT). Soundings keeps everything upstream of the store: field-node firmware, the gateway/decoder, the packet contract (DEC-003, **unchanged**), the node→location map (D7, gateway-side enrichment before publish), and **its own alert + dashboard definitions as versioned config in this repo**, provisioned into the shared Grafana. The gateway derives kPa/VPD/gallons and publishes **raw + derived** JSON to Poop Deck under `farm/soundings/…`; Poop Deck stores both and never recomputes.

**Why:**
- **One store, farm-wide (resolves D6).** The JOIN-to-farm-records requirement D6 hinged on is real, and Poop Deck is the Postgres counterparty — tinkle already writes there. A single relational store lets soil tension, irrigation, and weather be correlated with a JOIN instead of an ETL, which the provisional VictoriaMetrics (a bare metrics TSDB) couldn't give.
- **Raw preserved, derivation re-revisable (resolves D1).** Raw stays on the wire (DEC-003) and Poop Deck stores raw **and** derived, so a re-fit calibration curve re-derives from stored raw without reflashing. Deriving gateway-side (not on-node) keeps the firmware from baking in coefficients and keeps the math re-revisable. **Do not adopt tinkle's semantic-only schema for soundings** — tinkle is an event producer (a valve-open *is* the fact); soundings is a sensor producer whose raw resistance/T-RH is the ground truth and kPa/VPD a lens.
- **Clear, one-way ownership.** Soundings owns sensor→packet→gateway→publish and its own dashboard/alert *definitions*; Poop Deck owns the store and the shared Grafana instance. Soundings publishes; Poop Deck remembers. A Poop Deck outage is a dropped publish, nothing worse — soundings stays autonomous.

**Tradeoff:** Soundings now depends on an external store for persistence and viewing, and its dashboards live as config provisioned into an instance it doesn't run. Accepted: farm-wide correlation and single-store simplicity outweigh local self-containment, and the gateway can buffer/replay to Poop Deck idempotently (natural key `(node_id, seq)`).

**Revisit:** If the farm-records JOIN requirement evaporates, or Poop Deck's ops burden proves heavier than a local store — neither expected. The DEC-003 wire contract is untouched by this decision.

---
