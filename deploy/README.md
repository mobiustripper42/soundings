# deploy/

**Soundings does not run a store, a broker, or a Grafana (DEC-004).** Poop Deck does —
TimescaleDB, mosquitto and the shared Grafana all live at `/home/eric/poop-deck`. In
production soundings' gateway connects *out* to that broker and publishes
`contracts/publish-v1.md` documents; the publish is the handoff, and a Poop Deck outage
is a dropped publish and nothing worse.

So there is nothing to deploy from this directory.

## `dev-sim/` — the local simulation stack

What used to be `deploy/` is now `deploy/dev-sim/`: mosquitto + VictoriaMetrics + Grafana,
stood up so the end-to-end sim spine has somewhere to draw a curve. It was always
provisional (Phase 1.5 landed it explicitly non-binding on the storage decision, and
DEC-004 then resolved that decision elsewhere).

**It is kept, not deleted, for one reason:** the milestone that closes this phase — 3.12,
first light — is *defined* as a real tank level on a chart, and until Poop Deck carries a
soundings dashboard this is the only thing that can draw one. Retiring the display before
the thing that needs a display would be tidy and wrong.

It is not production, it is not a fallback, and nothing should grow a dependency on it.
When Poop Deck is drawing the tank, `dev-sim/` and `gateway/soundings_gateway/ingest.py`
retire together.

```bash
docker compose -f deploy/dev-sim/docker-compose.yml up
```

Details in `dev-sim/README.md`.
