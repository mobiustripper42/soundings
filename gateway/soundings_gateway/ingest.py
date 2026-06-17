"""Ingestion — decoded readings (off MQTT) into the time-series DB.

Turns a reading dict into Influx line protocol and writes it to VictoriaMetrics.
Measurement `soundings` + one field per channel + a `node` tag yields clean
metric names `soundings_<channel>{node="N"}` for Grafana. Faulted channels are
NOT written (a failed read is a fault, not a bogus value — DEC-002); the fault is
still visible in the MQTT message for alerting (Phase 4).

The DB writer is injected so this is unit-testable without a live VM, and so the
D6 swap (VictoriaMetrics → TimescaleDB) touches only the writer, not this logic.
A write failure is logged, never fatal — the daemon keeps ingesting.
"""
from __future__ import annotations

import logging
import urllib.request
from collections.abc import Callable

log = logging.getLogger(__name__)

# Takes one line-protocol string and persists it.
Writer = Callable[[str], None]


def reading_to_line(msg: dict) -> str:
    """Influx line protocol for one reading. Faulted channels are dropped (a failed
    read is a fault, not a value — DEC-002), but `battery_mv` is always written: even
    a node whose every sensor faulted is worth recording as alive."""
    fields: dict[str, int] = {}
    for c in msg["channels"]:
        if c["fault"]:
            continue
        fields[c["name"].lower()] = c["raw"]
    fields["battery_mv"] = msg["battery_mv"]
    field_str = ",".join(f"{k}={v}" for k, v in fields.items())
    line = f"soundings,node={msg['node_id']} {field_str}"
    ts = msg.get("received_at")
    if ts is not None:
        line += f" {int(ts * 1e9)}"  # explicit nanosecond timestamp (gateway receipt time)
    return line


def vm_writer(vm_url: str) -> Writer:
    """A Writer that POSTs line protocol to VictoriaMetrics' /write endpoint."""
    write_url = vm_url.rstrip("/") + "/write"

    def _write(line: str) -> None:
        req = urllib.request.Request(write_url, data=line.encode(), method="POST")
        with urllib.request.urlopen(req, timeout=5) as resp:  # noqa: S310 (trusted local URL)
            resp.read()

    return _write


class Ingest:
    def __init__(self, write: Writer):
        self.write = write
        self.written = 0

    def handle(self, msg: dict) -> None:
        """Write one reading. Never raises — a bad write must not stop ingestion."""
        line = reading_to_line(msg)
        try:
            self.write(line)
            self.written += 1
        except Exception:  # noqa: BLE001 — keep the daemon alive on a write hiccup
            log.exception("failed to write reading to DB (node %s)", msg.get("node_id"))
