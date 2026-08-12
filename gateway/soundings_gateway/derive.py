"""Derived tank values onto the MQTT topic hierarchy.

**D9 resolved.** Everything soundings publishes lives under `farm/soundings/…`
(DEC-005 constrained the root, resolving a conflict with the bare `farm/water/…`
namespace an earlier draft of docs/tank-level-sensor.md used). Below it, two branches:

    farm/soundings/node/<node_id>/reading      the decoded packet, JSON, one per wake
    farm/soundings/<location>/<metric>         derived scalars, one topic per metric

They are separate because they answer to different readers. The node branch is keyed by
*hardware* and carries everything a packet had, including faults — it is the audit trail,
and it stays correct when a sensor moves. The location branch is keyed by *place* and
carries one number per topic, which is what a dashboard, an alert rule and a pump
controller each want to subscribe to without parsing anything.

The gating lives here rather than in tank.py: whether a reading is trustworthy enough
to derive a volume from is a policy question, and the answer is conservative. A derived
value that is merely plausible is worse than an absent one, because nobody re-checks a
number that looks fine.
"""
from __future__ import annotations

import logging

from . import tank

log = logging.getLogger(__name__)

__all__ = ["ROOT", "reading_topic", "derive_tank"]

ROOT = "farm/soundings"

CH_TANK_DISTANCE = "TANK_DISTANCE"
# DEC-007 puts the headspace DS18B20 on bit 4 (SOIL_TEMP_0) rather than spending one of
# the three remaining reserved bits on a naming preference. Only the SOIL prefix is
# wrong; the encoding — DS18B20 raw, i16, 1/16 °C — is exactly right.
CH_HEADSPACE_TEMP = "SOIL_TEMP_0"
TEMP_COUNTS_PER_C = 16.0


def reading_topic(node_id: int) -> str:
    """Where a node's full decoded packet is published."""
    return f"{ROOT}/node/{node_id}/reading"


def _channel(msg: dict, name: str):
    """The named channel if present AND not faulted, else None.

    A faulted channel is a declared sensor that did not answer (DEC-002) — its raw value
    is meaningless, and treating it as a number is how a fault becomes a reading.
    """
    for c in msg.get("channels", []):
        if c.get("name") == name:
            return None if c.get("fault") else c
    return None


def _fmt(value: float) -> str:
    return f"{value:.2f}"


def derive_tank(msg: dict, cfg) -> list[tuple[str, str]]:
    """Derived (topic, payload) pairs for one reading. Never raises.

    Returns [] rather than throwing on anything unexpected: this runs inside the ingest
    loop, and packet.py's contract is that malformed input is logged and dropped, never
    fatal. A derivation that raises takes the daemon down with it.
    """
    try:
        return _derive_tank(msg, cfg)
    except Exception:  # noqa: BLE001 — the daemon outlives one bad reading
        log.exception("tank derivation failed (node %s)", msg.get("node_id"))
        return []


def _derive_tank(msg: dict, cfg) -> list[tuple[str, str]]:
    node_id = msg.get("node_id")
    node = cfg.nodes.get(node_id)
    if node is None or node.role != "tank":
        # An unmapped node is a provisioning gap, not a reason to guess a location.
        return []

    geom = cfg.tanks.get(node.location)
    if geom is None:
        # Mapped to a place with no geometry: a config error. Inventing a default
        # capacity would put a fabricated number on a chart under a real location's name.
        log.warning("node %s maps to %s, which has no tank geometry", node_id, node.location)
        return []

    dist = _channel(msg, CH_TANK_DISTANCE)
    if dist is None:
        return []

    raw_mm = dist["raw"]
    if not (tank.SENSOR_MIN_MM <= raw_mm <= tank.SENSOR_MAX_MM):
        # Outside the A02YYUW's range the value is not a distance at all — it is the
        # sensor saying it got no usable return. Publishing it as a distance would put a
        # spike in the history that a later curve re-fit has to be told to ignore.
        log.info("node %s: distance %s mm outside sensor range, dropped", node_id, raw_mm)
        return []

    base = f"{ROOT}/{node.location}"
    out: list[tuple[str, str]] = [(f"{base}/distance_mm", _fmt(float(raw_mm)))]

    temp = _channel(msg, CH_HEADSPACE_TEMP)
    if temp is None:
        # No headspace temperature means no correction, and an uncorrected volume
        # silently carries the ~14 cm seasonal error DEC-007 exists to remove. Raw
        # distance still goes out — it is the durable record and a re-fit can use it.
        return out

    temp_c = temp["raw"] / TEMP_COUNTS_PER_C
    out.append((f"{base}/headspace_temp_c", _fmt(temp_c)))

    if not geom.measured:
        # The tape measure has not happened (HARDWARE_BUILD_PLAN.md §8 step 8). Raw and
        # temperature are real measurements and go out; gallons would be arithmetic on
        # placeholder geometry wearing the same units as the truth.
        return out

    corrected = tank.correct_distance_mm(raw_mm, temp_c, cfg.ref_temp_c)
    height = tank.water_height_mm(corrected, geom.sensor_zero_mm, geom.max_height_mm)
    gallons = tank.gallons_from_height(height, geom)

    out.append((f"{base}/level_gal", _fmt(gallons)))
    out.append((f"{base}/percent", _fmt(tank.percent_full(gallons, geom.capacity_gal))))
    return out
