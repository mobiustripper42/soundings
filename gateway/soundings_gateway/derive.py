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

from . import tank, tension, vpd

log = logging.getLogger(__name__)

__all__ = [
    "ROOT",
    "reading_topic",
    "metric_name",
    "derive_tank",
    "derive_bed",
    "derive_reading",
]

ROOT = "farm/soundings"

CH_TANK_DISTANCE = "TANK_DISTANCE"
# DEC-007 puts the headspace DS18B20 on bit 4 (SOIL_TEMP_0) rather than spending one of
# the three remaining reserved bits on a naming preference. Only the SOIL prefix is
# wrong; the encoding — DS18B20 raw, i16, 1/16 °C — is exactly right.
CH_HEADSPACE_TEMP = "SOIL_TEMP_0"
TEMP_COUNTS_PER_C = 16.0

# Which soil-temp channel compensates which Watermark. This is **pinned in
# contracts/packet-v1.md § Channel registry**, not chosen here: SPEC §5.1 stacks the
# sensors at two depths with a DS18B20 co-located at each, and deploys the commercial and
# homemade sensors as matched pairs SIDE BY SIDE, so one probe serves both sensors at its
# depth.
#
# ⚠ A manifest that breaks that co-location produces no fault and no dropped reading — it
# produces a confidently wrong number, compensated from the wrong depth. Every field on
# the wire is individually valid, so nothing downstream can detect it. That is why the
# pairing lives in the wire contract rather than in this comment.
TENSION_TEMP_PAIR = {
    0: "SOIL_TEMP_0",   # shallow, commercial
    1: "SOIL_TEMP_1",   # deep,    commercial
    2: "SOIL_TEMP_0",   # shallow, homemade — beside bit 0
    3: "SOIL_TEMP_1",   # deep,    homemade — beside bit 1
}


def reading_topic(node_id: int) -> str:
    """Where a node's full decoded packet is published."""
    return f"{ROOT}/node/{node_id}/reading"


def metric_name(topic: str) -> str:
    """The metric segment of a derived topic — `…/water/cluster/level_gal` → `level_gal`.

    Lives here rather than in the caller because this module owns the topic shape. A
    consumer that string-split the topic itself would silently produce wrong keys the day
    that shape changed; keeping the parse next to the construction means the two move
    together or not at all.
    """
    return topic.rsplit("/", 1)[-1]


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


def derive_bed(msg: dict, cfg) -> list[tuple[str, str]]:
    """Derived (topic, payload) pairs for one bed-node reading. Never raises.

    Same contract as derive_tank, and the same reason: this runs inside the ingest loop
    and a derivation that raises takes the daemon down with it.
    """
    try:
        return _derive_bed(msg, cfg)
    except Exception:  # noqa: BLE001 — the daemon outlives one bad reading
        log.exception("bed derivation failed (node %s)", msg.get("node_id"))
        return []


def _derive_bed(msg: dict, cfg) -> list[tuple[str, str]]:
    node_id = msg.get("node_id")
    node = cfg.nodes.get(node_id)
    if node is None or node.role != "bed":
        return []

    base = f"{ROOT}/{node.location}"
    out: list[tuple[str, str]] = []

    # Soil temperatures first: they are measurements in their own right, and publishing
    # them is what lets someone check a suspicious tension against the temperature that
    # produced it.
    temps_c: dict[str, float] = {}
    for bit, name in ((4, "SOIL_TEMP_0"), (5, "SOIL_TEMP_1"), (9, "SOIL_TEMP_2")):
        ch = _channel(msg, name)
        if ch is None:
            continue
        temp_c = ch["raw"] / TEMP_COUNTS_PER_C
        temps_c[name] = temp_c
        out.append((f"{base}/soil_temp_{bit}_c", _fmt(temp_c)))

    for bit in (0, 1, 2, 3):
        ch = _channel(msg, f"SOIL_TENSION_{bit}")
        if ch is None:
            continue

        # ⚠ No temperature, no tension. SPEC §5.1 calls the compensation **mandatory**,
        # and the reason is in the numbers: at 5 kΩ the same sensor reads 24.3 cb at
        # 10 °C and 34.2 cb at 30 °C, which straddles the 25-30 cb irrigation trigger.
        # An uncompensated value is not a rougher answer, it is a different one — so it
        # is withheld, exactly as tank.py withholds gallons with no headspace temp. The
        # raw resistance still goes out on the node branch and a re-fit can use it.
        paired = TENSION_TEMP_PAIR[bit]
        temp_c = temps_c.get(paired)
        if temp_c is None:
            log.info("node %s: SOIL_TENSION_%d has no %s, tension not derived",
                     node_id, bit, paired)
            continue

        kohm = tension.resistance_kohm(ch["raw"])
        kpa = tension.tension_kpa(kohm, temp_c)
        if kpa is None:
            # Out of the curve's domain — past the denominator's pole, or a value that
            # is not a resistance. Not a very dry soil; a reading with no meaning.
            log.info("node %s: SOIL_TENSION_%d raw %s at %.1f C is outside the curve",
                     node_id, bit, ch["raw"], temp_c)
            continue

        out.append((f"{base}/tension_{bit}_kpa", _fmt(kpa)))
        if tension.is_wet_end(kpa):
            # Published, not dropped — it is a true "wetter than you would irrigate at".
            # The flag says the number is directional rather than quantitative.
            out.append((f"{base}/tension_{bit}_wet_end", "1"))

    out.extend(_derive_air(msg, base, node_id))
    return out


def _derive_air(msg: dict, base: str, node_id) -> list[tuple[str, str]]:
    """Canopy air: temperature, humidity, and the VPD computed from them (SPEC §5.3).

    Each measurement publishes on its own merits; only the DERIVED value needs both.
    That is the same split the soil temperatures above take — published first, in their
    own right, so someone can check a suspicious tension against the temperature that
    produced it — and the same one _derive_tank takes with distance and headspace temp.
    A canopy temperature is real information whether or not its RH arrived.

    ⚠ Each channel is band-checked SEPARATELY. The registry's tick conversions span
    -45..130 °C and -6..119 %RH, which is the raw grid and not what an SHT45 can
    physically be: zero ticks decodes to a perfectly well-formed -45 °C at -6 %RH, and
    full scale to 130 °C at 119 %RH — a VPD of -52 kPa. Those are a dead or disconnected
    part, and a plausible-looking number published from one is worse than a gap because
    nobody re-checks a reading that looks fine. Quantisation is handled inside the
    predicates: saturated air lands a tick above 100 % and is not a fault.
    """
    out: list[tuple[str, str]] = []

    t_ch = _channel(msg, "AIR_TEMP")
    t_c = None
    if t_ch is not None:
        t_c = vpd.air_temp_c(t_ch["raw"])
        if vpd.temp_out_of_band(t_c):
            log.info("node %s: air temp %.3f C is outside the SHT45's range, dropped",
                     node_id, t_c)
            t_c = None
        else:
            out.append((f"{base}/air_temp_c", _fmt(t_c)))

    rh_ch = _channel(msg, "AIR_RH")
    rh_pct = None
    if rh_ch is not None:
        rh_pct = vpd.air_rh_pct(rh_ch["raw"])
        if vpd.rh_out_of_band(rh_pct):
            log.info("node %s: air RH %.3f %% is outside the SHT45's range, dropped",
                     node_id, rh_pct)
            rh_pct = None
        else:
            out.append((f"{base}/air_rh_pct", _fmt(rh_pct)))

    # ⚠ No humidity, no VPD. Air temperature alone says nothing about drying power, so a
    # half-present pair derives nothing — the same rule tank.py applies when it withholds
    # gallons with no headspace temperature. The measured halves above still went out.
    if t_c is None or rh_pct is None:
        if t_ch is not None or rh_ch is not None:
            log.info("node %s: VPD needs both AIR_TEMP and AIR_RH, not derived", node_id)
        return out

    out.append((f"{base}/vpd_kpa", _fmt(vpd.vpd_kpa(t_c, rh_pct))))
    return out


# Which derivation runs for a node is a property of its role, and the role lives in the
# node→location map (D7). Dispatching here rather than at each call site means a new
# node type is one branch in one place, not a grep for every caller of derive_tank.
_BY_ROLE = {
    "tank": derive_tank,
    "bed": derive_bed,
}


def derive_reading(msg: dict, cfg) -> list[tuple[str, str]]:
    """Derived (topic, payload) pairs for any reading, routed by the node's role.

    An unmapped node or an unknown role derives nothing and says so once at debug —
    it is a provisioning gap, not an error, and the raw reading still publishes on the
    node branch where it stays the durable record.
    """
    node = cfg.nodes.get(msg.get("node_id"))
    if node is None:
        return []
    fn = _BY_ROLE.get(node.role)
    if fn is None:
        log.debug("node %s has role %r, which has no derivation", node.node_id, node.role)
        return []
    return fn(msg, cfg)
