# Publish contract v1 — soundings → Poop Deck

The JSON document soundings publishes for every node wake. This is the second of
soundings' two contracts: `packet-v1.md` is what crosses the *radio* (DEC-003, byte-exact,
pinned by golden vectors), and this is what crosses the *broker* (DEC-004, DEC-008).

They are deliberately different. The packet is byte-starved — 46 bytes, a 16-channel
ceiling, channel names that are warts. The published document is not, so it carries names,
a location, derived values, and a timestamp the node cannot produce.

**Consumer:** Poop Deck (`/home/eric/poop-deck`), the farm's shared telemetry backend.
Soundings publishes; Poop Deck remembers (DEC-004). A Poop Deck outage is a dropped
publish and nothing worse.

---

## Topic

```
farm/soundings/node/<node_id>/reading
```

One message per node wake, QoS 1, **not retained** — a retained reading would look current
forever to a subscriber that connected an hour later. The derived-scalar branch
(`farm/soundings/<location>/<metric>`, DEC-008) is a separate live view for dashboards and
alert rules; it is **not** this contract and is not storable — it carries no timestamp.

## Payload

```json
{
  "v": 1,
  "node_id": 7,
  "seq": 42,
  "ts": "2026-08-13T04:12:00Z",
  "fw_version": 259,
  "battery_mv": 3810,
  "location": "water/cluster",
  "channels": [
    {"name": "TANK_DISTANCE", "bit": 8, "raw": 1347, "fault": false},
    {"name": "SOIL_TEMP_0",   "bit": 4, "raw": 320,  "fault": false}
  ],
  "derived": {
    "distance_mm": 1347.0,
    "headspace_temp_c": 20.0
  }
}
```

| Field | Type | Required | Notes |
|---|---|---|---|
| `v` | int | yes | Schema version. A consumer that doesn't recognise it must **drop and log**, never guess. |
| `node_id` | int | yes | From the packet header. Identifies *hardware*. |
| `seq` | int | yes | From the packet header, 0–65535, wraps. Half the natural key, so a reading without one is **not published at all** rather than published unkeyable. Survives deep sleep in RTC memory but **not a power cycle** — a backwards jump is a node restart, not packet loss. `0` is a real sequence number (the first packet after a power cycle), never an absent one. |
| `ts` | string | yes | **ISO-8601 UTC, `…Z`.** Gateway receipt time — field nodes have no RTC. |
| `fw_version` | int | no | Which binary produced this. Informational; **never a drop gate, on either side** — present in every document a healthy node produces, but its absence is not a reason to refuse the reading. The measurement is worth more than the provenance. |
| `battery_mv` | int | yes | Always present, even when every sensor faulted: a node whose sensors all failed is still worth recording as alive. `0` means the battery read itself failed. |
| `location` | string | no | Gateway-side enrichment from the node→location map (D7, DEC-008). Absent for an unmapped node. |
| `channels` | array | yes | Raw per-channel values **and their fault flags**. The durable record (D1). |
| `derived` | object | yes | Present but possibly `{}`. |

### `channels[]`

| Field | Type | Notes |
|---|---|---|
| `name` | string | From the packet-v1 channel registry. `SOIL_TEMP_0` on the tank node is the headspace DS18B20 — the encoding is right, the name is a wart (DEC-007). |
| `bit` | int | Registry index 0–15. The stable identifier; prefer it over `name` if the two ever disagree. |
| `raw` | int | The sensor's own units, uninterpreted. **This is the ground truth** — every derived value re-derives from it. |
| `fault` | bool | `true` = declared but did not answer (DEC-002). The `raw` alongside it is meaningless; do not store it as a value. |

### `derived`

Keys are **omitted when not derivable**, never nulled or zeroed. An absent key means "this
could not be computed this cycle"; a present one is a real number. The refusals are listed
in DEC-008 and enforced in `gateway/soundings_gateway/derive.py`.

Which keys appear depends on the node's `role` in the node→location map (D7):

| role | keys | refuses when |
|---|---|---|
| `tank` | `distance_mm`, `headspace_temp_c`, `level_gal`, `percent` | tank geometry unmeasured; no headspace temperature; distance outside the sensor's range |
| `bed` | `soil_temp_<bit>_c`, `tension_<bit>_kpa`, `tension_<bit>_wet_end` | no soil-temp probe at that Watermark's depth (SPEC §5.1 makes the compensation mandatory); resistance past the curve's temperature-dependent pole |

`tension_<bit>_wet_end` is present and `1` only below 10 kPa, where the Watermark stops
being quantitative. The tension is published alongside it rather than withheld: it is a
true "far wetter than you would irrigate at", and a dropped reading is indistinguishable
downstream from a dead sensor.

Nothing here is recomputed downstream. Poop Deck stores raw **and** derived (DEC-004) and
never derives; if a curve is re-fitted, it re-derives from the stored `channels[].raw`.

## Idempotency

The natural key is **`(node_id, seq)`** — the same key Poop Deck's own
`db/README.md:26` specifies for a `soundings_readings` table. `seq` wraps at 65535 and
restarts at 0 on a power cycle, so the key is not unique over all time; it is unique over
any window shorter than 65535 wakes, which at a 15-minute cadence is about 22 months. That
is enough for its actual job: making a gateway buffer-and-replay a no-op rather than a
duplicate row.

## Versioning

`v` increments when a field changes meaning or a required field is added or removed.
Adding an **optional** field does not bump it — a consumer that ignores unknown keys keeps
working, which is why `v` is a gate on the consumer rather than a negotiation.

## What Poop Deck still needs on its side

Recorded here because it is the reason an end-to-end test cannot run yet, and because a
producer contract that quietly assumes the consumer is ready is how a "working" integration
turns out never to have stored anything:

1. **A `soundings` MQTT user and ACL stanza.** Their `deploy/mosquitto/aclfile` grants
   `ingest` read on `farm/#` and `tinkle` write on `farm/irrigation/#` only. There is no
   soundings writer.
2. **A `soundings_readings` migration.** None exists in their `db/migrations/`.
3. **A soundings ingest daemon.** Their `ingest/` holds one daemon and its Dockerfile
   copies one file; a second producer needs a second image or a parameterized one.

None of the three is soundings' to write.
