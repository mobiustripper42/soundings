"""End-to-end spine — the first time every piece touches every other piece.

    synthetic fleet  →  IPacketSource  →  gateway decode  →  MQTT  →  ingest  →  VictoriaMetrics  →  Grafana

Run the sim stack first (deploy/docker-compose.yml), then:

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

import paho.mqtt.client as mqtt

from . import emitter, gateway, ingest

log = logging.getLogger("soundings_gateway.spine")


def build_fleet(n: int, **kw) -> emitter.FleetEmitter:
    """N bed nodes with slightly varied drydown so the fleet's curves differ and
    the ±jitter window is visible across distinct series."""
    specs = [
        emitter.NodeSpec(
            node_id=i,
            wet_raw=110 + 15 * i,
            tau_min=2400.0 + 240.0 * i,
        )
        for i in range(1, n + 1)
    ]
    return emitter.FleetEmitter(specs=specs, **kw)


def run(args: argparse.Namespace) -> int:
    topic_wildcard = f"{args.topic_prefix}/+"

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

    sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="soundings-ingest")
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
    pub.connect(args.broker_host, args.broker_port)
    pub.loop_start()

    def publish(msg: dict) -> None:
        pub.publish(f"{args.topic_prefix}/{msg['node_id']}", json.dumps(msg))

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
    p.add_argument("--topic-prefix", default="soundings/readings")
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
