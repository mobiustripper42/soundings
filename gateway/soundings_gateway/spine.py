"""End-to-end spine — the first time every piece touches every other piece.

    synthetic fleet  →  IPacketSource  →  gateway decode  →  MQTT  →  ingest  →  VictoriaMetrics  →  Grafana

⚠ This is the DEV SIM path (DEC-004). It stands the whole pipeline up locally, store and
all, so the spine has somewhere to draw a curve. In production soundings publishes
`contracts/publish-v1.md` documents to Poop Deck's broker and stops there — no local DB,
no local Grafana, no ingest side at all.

Run the sim stack first (deploy/dev-sim/docker-compose.yml), then:

    cd gateway && .venv/bin/python -m soundings_gateway.spine --realtime --minutes 720

and watch the drydown curve move on the "Soundings sim spine" dashboard. Omit
--realtime for a fast backfill (points carry their sim receipt time, so the curve
still lands spread across recent history). Zero hardware anywhere in the path.
"""
from __future__ import annotations

import argparse
import json
import logging
import threading
import time
from pathlib import Path

import paho.mqtt.client as mqtt

from . import config as gwconfig
from . import derive, emitter, gateway, ingest
from .publish import envelope

log = logging.getLogger("soundings_gateway.spine")


def build_fleet(n: int, **kw) -> emitter.FleetEmitter:
    """N bed nodes with slightly varied drydown so the fleet's curves differ and
    the ±jitter window is visible across distinct series.

    The wet-end spread is small in absolute terms because the Watermark curve is steep
    there: 0.6 kΩ and 1.0 kΩ are 7.0 cb and 10.7 cb, which is a visible gap on a chart.
    The pre-#20 spread (110 + 15·i, i.e. 11-15 kΩ) looked wider and was not — every one
    of those nodes started its drydown above the 50-60 cb stress threshold.
    """
    specs = [
        emitter.NodeSpec(
            node_id=i,
            wet_raw=6 + 2 * i,
            tau_min=2400.0 + 240.0 * i,
        )
        for i in range(1, n + 1)
    ]
    return emitter.FleetEmitter(specs=specs, **kw)


def run(args: argparse.Namespace) -> int:
    # Only the per-node reading branch is JSON. The derived branch carries bare scalars
    # for dashboards and alert rules, so a wildcard that swept both would hand
    # json.loads() a number and log a "bad payload" for every healthy publish.
    topic_wildcard = f"{derive.ROOT}/node/+/reading"

    cfg = gwconfig.load_config(args.config)

    # --- ingestion side: subscribe to MQTT, write decoded readings to the DB ---
    sink = ingest.Ingest(ingest.vm_writer(args.vm_url))
    connected = threading.Event()

    def on_connect(client, _userdata, _flags, reason_code, _props=None):
        log.info("ingest connected (%s); subscribing %s", reason_code, topic_wildcard)
        client.subscribe(topic_wildcard)
        connected.set()

    def on_message(_client, _userdata, message):
        try:
            sink.handle(json.loads(message.payload))
        except Exception:  # noqa: BLE001 — a poison message must not kill the loop
            log.exception("bad MQTT payload on %s", message.topic)

    def authenticate(client: mqtt.Client) -> None:
        """Apply broker credentials if the environment supplied them.

        Poop Deck's broker runs `allow_anonymous false`; the dev-sim mosquitto does not.
        Setting credentials only when they exist lets one code path serve both, and an
        absent credential against a real broker fails the connect loudly rather than
        publishing into a void.
        """
        if cfg.broker.username:
            client.username_pw_set(cfg.broker.username, cfg.broker.password)

    sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="soundings-ingest")
    authenticate(sub)
    sub.on_connect = on_connect
    sub.on_message = on_message
    sub.connect(args.broker_host, args.broker_port)
    sub.loop_start()
    if not connected.wait(timeout=10):
        log.error("ingest could not connect to broker %s:%d", args.broker_host, args.broker_port)
        sub.loop_stop()
        return 1

    # --- publisher side: the gateway publishes each decoded reading per node ---
    pub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="soundings-gateway")
    authenticate(pub)
    pub.connect(args.broker_host, args.broker_port)
    pub.loop_start()

    def publish(msg: dict) -> None:
        # The storable document (contracts/publish-v1.md) — versioned, ISO-8601 stamped,
        # raw AND derived in one place, keyed (node_id, seq) so a replay is a no-op.
        # This is what Poop Deck ingests; the raw reading dict never leaves the process.
        env = envelope(msg, cfg)
        if env is not None:
            pub.publish(derive.reading_topic(env["node_id"]), json.dumps(env), qos=1, retain=False)
        # Derived scalars, keyed by place rather than by hardware — the live view a
        # dashboard or alert rule subscribes to, not a record. Returns nothing for a node
        # the map doesn't cover or a reading that can't support its role's derivation.
        # The sim fleet is bed nodes; since issue #20 those derive tension in kPa, so a
        # sim run that used to publish nothing on this branch now publishes on it.
        for topic, payload in derive.derive_reading(msg, cfg):
            pub.publish(topic, payload)

    fleet = build_fleet(
        args.nodes,
        cadence_min=args.cadence_min,
        max_minutes=args.minutes,
        jitter_s=args.jitter_s,
        seed=args.seed,
        realtime=args.realtime,
        time_scale=args.time_scale,
        fw_version=args.fw_version,
    )
    gw = gateway.Gateway(fleet, publish)

    log.info("emitting %d node(s) over %.0f sim-min (realtime=%s)…",
             args.nodes, args.minutes, args.realtime)
    decoded = gw.run()

    # Let the broker drain the last messages into the DB before we tear down.
    deadline = time.time() + 10
    while sink.written < decoded and time.time() < deadline:
        time.sleep(0.2)

    pub.disconnect()
    sub.disconnect()
    pub.loop_stop()
    sub.loop_stop()
    log.info("spine done: %d decoded, %d dropped, %d written to DB",
             decoded, gw.dropped, sink.written)
    return 0 if sink.written >= decoded else 2


def main() -> int:
    p = argparse.ArgumentParser(description="Soundings end-to-end sim spine.")
    p.add_argument("--broker-host", default="localhost")
    p.add_argument("--broker-port", type=int, default=1883)
    p.add_argument("--vm-url", default="http://localhost:8428")
    p.add_argument("--config", default=str(Path(__file__).resolve().parents[1] / "config" / "soundings.toml"),
                   help="node→location map and tank geometry (D7)")
    p.add_argument("--nodes", type=int, default=3)
    p.add_argument("--cadence-min", type=float, default=12.0)
    p.add_argument("--minutes", type=float, default=1440.0, help="sim minutes to run")
    p.add_argument("--jitter-s", type=float, default=30.0)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--realtime", action="store_true",
                   help="sleep the (compressed) gap between transmits so the chart moves live")
    p.add_argument("--time-scale", type=float, default=720.0,
                   help="sim-seconds per real-second in realtime mode")
    p.add_argument("--fw-version", type=int, default=100)
    p.add_argument("--log-level", default="INFO")
    args = p.parse_args()
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(name)s %(message)s")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
