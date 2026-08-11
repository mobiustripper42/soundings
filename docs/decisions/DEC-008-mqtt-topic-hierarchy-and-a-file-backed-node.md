---
id: DEC-008
title: "MQTT topic hierarchy, and a file-backed node→location map"
topic: "Radio, wire contract & gateway"
amends:
  - id: DEC-005
    relation: extends
    scope: "the topic hierarchy below the farm/soundings/ root, which DEC-005 constrained but left open; the namespace choice itself stands"
---

## DEC-008: MQTT topic hierarchy, and a file-backed node→location map

**Decision:** Two branches under the `farm/soundings/…` root DEC-005 fixed, and the
node→location map is a **config file**, not a database table. **Resolves D7 and D9.**

```
farm/soundings/node/<node_id>/reading      the decoded packet, JSON, one per wake
farm/soundings/<location>/<metric>         derived scalars, one topic per metric
```

For the tank cluster the second branch is exactly the four topics
`docs/tank-level-sensor.md` already names: `distance_mm`, `headspace_temp_c`,
`level_gal`, `percent`. The map lives at `gateway/config/soundings.toml`, keyed by node
id, giving each node a `location` (its sub-path) and a `role` (which derivation runs).

**Why:**

- **The two branches answer to different readers, and one branch cannot serve both.**
  The node branch is keyed by *hardware*: it carries everything the packet carried,
  faults included, and it is the audit trail — it stays correct when a sensor is moved
  to another tank, because it never claimed to be about a place. The location branch is
  keyed by *place* and carries one number per topic, which is what a dashboard panel, an
  alert rule, and tinkle's future pump lockout each want to subscribe to without parsing
  a payload or knowing which node id happens to be fitted this season.
- **A file, because the map is what tells ingestion where a reading belongs.** It has to
  be readable *before* anything is written to the database, so putting it in the database
  is a bootstrap the gateway does not need — and it would couple provisioning to whichever
  store D6 eventually picks, which is still open. A file is also diffable: adding a node
  shows up in review rather than in somebody's `psql` history.
- **One topic per metric, scalar payloads, no JSON on the derived branch.** A subscriber
  that has to parse a document to read one number is a subscriber that breaks when the
  document gains a field. It also keeps the ingest wildcard honest: the reading branch is
  the only JSON, so `farm/soundings/node/+/reading` sweeps exactly what `json.loads()`
  can handle.

**Tradeoff:** A node that moves between locations needs a config edit and a gateway
restart; auto-discovery would not. Accepted — it is the same declared-not-detected
posture as DEC-002, and for the same reason: a node that silently re-homes itself is a
node whose history cannot be trusted. Second cost: derived topics carry no timestamp, so
a stale retained value looks current to a naive subscriber. The reading branch carries
`received_at` and is the record; the derived branch is a view.

**Rejected:** a single `farm/soundings/<node>/<metric>` tree (welds place to hardware —
moving a sensor rewrites history); JSON on the derived branch (see above); a database
table for the map (bootstrap ordering, and it presumes D6).

**Revisit:** if a second sensor ever feeds one location, `role` becomes a list rather
than a scalar. If retained-value staleness bites, publish a `*/updated_at` alongside.

---
