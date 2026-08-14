"""Gateway configuration — the node→location map (SPEC §12 D7) and tank geometry.

**D7 resolved: a file, not a database table.** The map is what tells ingestion where a
reading belongs, so it must be readable *before* anything is written to the database —
a table that has to exist before the first write is a bootstrap the gateway does not
need, and it would tie provisioning to whichever database D6 eventually picks. A file is
also diffable, so adding a node shows up in review rather than in someone's psql
history. If a UI ever needs to edit the map, it edits this file.

stdlib only (`tomllib`, Python 3.11+), per the project's stdlib-first convention.
"""
from __future__ import annotations

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

__all__ = ["BrokerConfig", "TankGeometry", "NodeLocation", "GatewayConfig", "load_config"]


@dataclass(frozen=True)
class BrokerConfig:
    """Where to publish. Poop Deck owns the broker (DEC-004), so soundings connects out.

    Host and port are config; **credentials are environment only** and never touch this
    repo. Poop Deck's mosquitto runs `allow_anonymous false` with a per-producer ACL, so
    an unauthenticated connect is refused rather than silently dropping messages — which
    is the good failure, and the reason there is no anonymous fallback here.
    """

    host: str = ""
    port: int = 1883
    username: str = ""
    password: str = ""

    @property
    def configured(self) -> bool:
        return bool(self.host)


@dataclass(frozen=True)
class TankGeometry:
    """Two-segment volume curve for one tank cluster.

    `measured` gates every derived volume. It defaults False and every absent-key path
    below leaves it False, because an absent flag must never read as "go ahead": the
    numbers need a tape measure (HARDWARE_BUILD_PLAN.md §8 step 8), and a plausible
    wrong number on a chart is worse than a gap — nobody re-checks a reading that looks
    fine.
    """

    measured: bool = False
    capacity_gal: float = 0.0
    sensor_zero_mm: int = 0
    max_height_mm: int = 0
    breakpoint_mm: int = 0
    gal_per_mm_below: float = 0.0
    gal_per_mm_above: float = 0.0


@dataclass(frozen=True)
class NodeLocation:
    node_id: int
    location: str       # the MQTT sub-path under farm/soundings/ this node publishes to
    role: str           # selects the derivation; "tank" is the only one so far
    label: str = ""     # human-facing name, for dashboards


@dataclass(frozen=True)
class GatewayConfig:
    ref_temp_c: float = 20.0
    broker: BrokerConfig = field(default_factory=BrokerConfig)
    nodes: dict[int, NodeLocation] = field(default_factory=dict)
    tanks: dict[str, TankGeometry] = field(default_factory=dict)


def load_config(path) -> GatewayConfig:
    """Read the config file. Raises FileNotFoundError if it is absent.

    Deliberately NOT tolerant of a missing file: an empty config looks exactly like "no
    nodes are mapped yet" and would derive nothing, forever, without saying so.
    """
    p = Path(path)
    with p.open("rb") as fh:          # raises FileNotFoundError, which is the point
        raw = tomllib.load(fh)

    gw = raw.get("gateway", {})

    nodes: dict[int, NodeLocation] = {}
    for key, entry in raw.get("nodes", {}).items():
        # TOML table keys are strings; a node id left as one never matches the integer
        # node_id on a decoded reading, and every derivation silently returns nothing.
        node_id = int(key)
        nodes[node_id] = NodeLocation(
            node_id=node_id,
            location=entry.get("location", ""),
            role=entry.get("role", ""),
            label=entry.get("label", ""),
        )

    tanks: dict[str, TankGeometry] = {}
    for location, entry in raw.get("tanks", {}).items():
        tanks[location] = TankGeometry(
            measured=bool(entry.get("measured", False)),
            capacity_gal=float(entry.get("capacity_gal", 0.0)),
            sensor_zero_mm=int(entry.get("sensor_zero_mm", 0)),
            max_height_mm=int(entry.get("max_height_mm", 0)),
            breakpoint_mm=int(entry.get("breakpoint_mm", 0)),
            gal_per_mm_below=float(entry.get("gal_per_mm_below", 0.0)),
            gal_per_mm_above=float(entry.get("gal_per_mm_above", 0.0)),
        )

    br = raw.get("broker", {})
    broker = BrokerConfig(
        host=str(br.get("host", "")),
        port=int(br.get("port", 1883)),
        # Credentials come from the environment, never the file. The names match Poop
        # Deck's own convention (their deploy/.env.example uses MQTT_<PRODUCER>_USER).
        username=os.environ.get("MQTT_SOUNDINGS_USER", ""),
        password=os.environ.get("MQTT_SOUNDINGS_PASSWORD", ""),
    )

    return GatewayConfig(
        ref_temp_c=float(gw.get("ref_temp_c", 20.0)),
        broker=broker,
        nodes=nodes,
        tanks=tanks,
    )
