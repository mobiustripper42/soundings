# deploy/ — simulation server stack

The server-side trio the telemetry pipeline lands on, as a single
`docker compose` stack so the whole pipeline runs on a laptop with no hardware.

| Service | Image | Port | Role |
|---------|-------|------|------|
| Mosquitto | `eclipse-mosquitto:2` | 1883 (MQTT), 9001 (WS) | message bus between gateway and ingestion |
| VictoriaMetrics | `victoriametrics/victoria-metrics` | 8428 | time-series DB **(provisional — non-binding on D6)** |
| Grafana | `grafana/grafana-oss` | 3000 | dashboards (datasource pre-provisioned) |

## Run

```bash
docker compose -f deploy/docker-compose.yml up -d
# Grafana   http://localhost:3000   (anonymous viewer; admin/admin to edit)
# VM        http://localhost:8428
# MQTT      localhost:1883
docker compose -f deploy/docker-compose.yml down        # stop (keeps volumes)
docker compose -f deploy/docker-compose.yml down -v     # stop + wipe data
```

Knobs (env or a `.env` beside the compose file):

| Var | Default | Meaning |
|-----|---------|---------|
| `VM_RETENTION` | `12` | raw-sample retention, **months** (the retention knob) |
| `GRAFANA_USER` / `GRAFANA_PASSWORD` | `admin` / `admin` | Grafana admin login |
| `GRAFANA_PORT` / `VM_PORT` / `MQTT_PORT` / `MQTT_WS_PORT` | `3000` / `8428` / `1883` / `9001` | host ports (override if one is already taken) |

## Provisional DB — non-binding on D6

VictoriaMetrics is here **only because it is the fastest to stand up** (one
container, no schema, retention is a flag). This is **not** a resolution of D6
(TimescaleDB vs VictoriaMetrics, SPEC §12) — Phase 3 decides that against the
"do we need SQL JOINs to farm records?" question and may swap this service out.

What keeps the swap cheap: the datasource is provisioned under the stable uid
`soundings-tsdb`, so dashboards reference *the DB*, not the engine. Swapping to
Timescale means replacing one datasource file and the gateway's DB-writer (Phase
3) — dashboards keep working.

Storage-level **downsampling** is intentionally *not* configured here: it's
DB-specific (VM downsampling vs Timescale continuous aggregates) and belongs with
the D6 resolution. The sim downsamples at query time in Grafana. Retention (the
durable knob) is live now via `VM_RETENTION`.

## Holding a series and drawing it

VictoriaMetrics accepts writes on several line protocols. Quick smoke test
(Influx line protocol) — push a point and see it in the "Soundings sim spine"
dashboard:

```bash
curl -s -XPOST 'http://localhost:8428/write' \
  --data-binary 'soundings_soil_tension_raw,node=1 value=152'
# query it back
curl -s 'http://localhost:8428/api/v1/query?query=soundings_soil_tension_raw'
```

The real path (gateway: MQTT → decode → DB write) lands with the end-to-end spine
(Phase 1.6) and ingestion (Phase 3).
