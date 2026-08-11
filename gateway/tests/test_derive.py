"""Topic hierarchy and derivation gating — the plumbing half of Phase 3.6 (issue #45).

Resolves D9 (topic hierarchy below the `farm/soundings/…` root) and D7 (node→location
map). The tests that matter most here are the ones that assert derived values are
NOT published: a wrong number on a chart is worse than a gap, because nobody
re-checks a reading that looks fine.
"""
from __future__ import annotations

from soundings_gateway import derive
from soundings_gateway.config import GatewayConfig, NodeLocation, TankGeometry

GEOM = TankGeometry(
    measured=True,
    capacity_gal=1490.0,
    sensor_zero_mm=2100,
    max_height_mm=2000,
    breakpoint_mm=1150,
    gal_per_mm_below=1.0,
    gal_per_mm_above=0.4,
)

CFG = GatewayConfig(
    ref_temp_c=20.0,
    nodes={1: NodeLocation(1, "water/cluster", "tank", "Rain catchment cluster")},
    tanks={"water/cluster": GEOM},
)

UNMEASURED = GatewayConfig(
    ref_temp_c=20.0,
    nodes={1: NodeLocation(1, "water/cluster", "tank", "Rain catchment cluster")},
    tanks={"water/cluster": TankGeometry(measured=False)},
)


def reading(distance_mm=600, temp_raw=320, *, node_id=1,
            distance_fault=False, temp_fault=False, channels=None):
    """A decoded packet-v1 reading dict as gateway.py publishes it. temp_raw is the
    DS18B20's native 1/16 °C, so 320 = 20.0 °C (DEC-007 puts it on bit 4)."""
    if channels is None:
        channels = [
            {"name": "TANK_DISTANCE", "bit": 8, "raw": distance_mm, "fault": distance_fault},
            {"name": "SOIL_TEMP_0", "bit": 4, "raw": temp_raw, "fault": temp_fault},
        ]
    return {"node_id": node_id, "seq": 1, "battery_mv": 3800,
            "channels": channels, "received_at": 1_700_000_000.0}


def topics(pairs):
    return {t: v for t, v in pairs}


# ---- Topic hierarchy (D9) ---------------------------------------------------

def test_reading_topic_is_per_node_under_the_soundings_root():
    assert derive.reading_topic(7) == "farm/soundings/node/7/reading"


def test_derived_topics_are_the_four_named_in_the_spec():
    out = topics(derive.derive_tank(reading(), CFG))
    assert set(out) == {
        "farm/soundings/water/cluster/distance_mm",
        "farm/soundings/water/cluster/headspace_temp_c",
        "farm/soundings/water/cluster/level_gal",
        "farm/soundings/water/cluster/percent",
    }


def test_derived_topics_use_the_location_from_the_node_map_not_the_node_id():
    cfg = GatewayConfig(ref_temp_c=20.0,
                        nodes={4: NodeLocation(4, "water/north", "tank")},
                        tanks={"water/north": GEOM})
    out = topics(derive.derive_tank(reading(node_id=4), cfg))
    assert "farm/soundings/water/north/level_gal" in out


# ---- Values -----------------------------------------------------------------

def test_raw_distance_is_published_uncorrected():
    # 20 °C is the reference, so corrected == raw here; the point is that this topic
    # carries the SENSOR's number, which is what a later re-fit needs.
    out = topics(derive.derive_tank(reading(distance_mm=600, temp_raw=320), CFG))
    assert float(out["farm/soundings/water/cluster/distance_mm"]) == 600.0


def test_headspace_temp_is_published_in_celsius():
    out = topics(derive.derive_tank(reading(temp_raw=320), CFG))
    assert float(out["farm/soundings/water/cluster/headspace_temp_c"]) == 20.0


def test_headspace_temp_handles_the_signed_below_freezing_case():
    out = topics(derive.derive_tank(reading(temp_raw=-160), CFG))
    assert float(out["farm/soundings/water/cluster/headspace_temp_c"]) == -10.0


def test_gallons_come_from_the_corrected_distance_not_the_raw_one():
    # At 40 °C the true distance is ~2070.6 mm for a raw 2000 — so the water is LOWER
    # than the raw reading implies, and gallons must reflect the corrected number.
    hot = topics(derive.derive_tank(reading(distance_mm=2000, temp_raw=640), CFG))
    ref = topics(derive.derive_tank(reading(distance_mm=2000, temp_raw=320), CFG))
    assert float(hot["farm/soundings/water/cluster/level_gal"]) < float(
        ref["farm/soundings/water/cluster/level_gal"])


def test_percent_is_published_alongside_gallons():
    out = topics(derive.derive_tank(reading(distance_mm=1350, temp_raw=320), CFG))
    # height = 2100-1350 = 750 → 750 gal of 1490
    assert float(out["farm/soundings/water/cluster/level_gal"]) == 750.0
    assert float(out["farm/soundings/water/cluster/percent"]) == 50.34


# ---- Refusals — the part that matters ---------------------------------------

def test_unmeasured_geometry_publishes_raw_but_refuses_gallons():
    out = topics(derive.derive_tank(reading(), UNMEASURED))
    assert "farm/soundings/water/cluster/distance_mm" in out
    assert "farm/soundings/water/cluster/headspace_temp_c" in out
    assert "farm/soundings/water/cluster/level_gal" not in out
    assert "farm/soundings/water/cluster/percent" not in out


# Without a headspace temperature the correction cannot run, and an uncorrected volume
# silently carries the 14 cm error DEC-007 exists to remove. Raw still goes out.
def test_a_faulted_temperature_refuses_gallons_but_still_publishes_distance():
    out = topics(derive.derive_tank(reading(temp_fault=True), CFG))
    assert "farm/soundings/water/cluster/distance_mm" in out
    assert "farm/soundings/water/cluster/level_gal" not in out


def test_a_missing_temperature_channel_refuses_gallons():
    only_distance = [{"name": "TANK_DISTANCE", "bit": 8, "raw": 600, "fault": False}]
    out = topics(derive.derive_tank(reading(channels=only_distance), CFG))
    assert "farm/soundings/water/cluster/level_gal" not in out
    # percent is gated with gallons, and a percent derived from a suppressed volume is
    # the exact shape of leak this refusal exists to prevent.
    assert "farm/soundings/water/cluster/percent" not in out


def test_a_faulted_distance_publishes_nothing_derived():
    # Total silence, not "the two topics I thought to name" — a faulted distance leaves
    # nothing derivable, so anything appearing here at all is a regression.
    assert topics(derive.derive_tank(reading(distance_fault=True), CFG)) == {}


# Outside the A02YYUW's 30–4500 mm range the number is not a short or long distance,
# it is a sensor telling you it did not get a return.
def test_a_distance_below_the_sensor_range_is_rejected_entirely():
    out = topics(derive.derive_tank(reading(distance_mm=10), CFG))
    assert out == {}


def test_a_distance_above_the_sensor_range_is_rejected_entirely():
    out = topics(derive.derive_tank(reading(distance_mm=4600), CFG))
    assert out == {}


def test_a_reading_at_the_exact_range_boundaries_is_accepted():
    assert topics(derive.derive_tank(reading(distance_mm=30), CFG)) != {}
    assert topics(derive.derive_tank(reading(distance_mm=4500), CFG)) != {}


# An unmapped node is a provisioning gap, not a reason to guess a location.
def test_a_node_absent_from_the_map_derives_nothing():
    assert derive.derive_tank(reading(node_id=99), CFG) == []


def test_a_node_whose_role_is_not_tank_derives_nothing():
    cfg = GatewayConfig(ref_temp_c=20.0,
                        nodes={1: NodeLocation(1, "bed/north", "soil")},
                        tanks={})
    assert derive.derive_tank(reading(), cfg) == []


# A mapped node whose location has no geometry entry is a config error, and guessing a
# default capacity would put a fabricated number on a chart.
def test_a_location_with_no_geometry_derives_nothing():
    cfg = GatewayConfig(ref_temp_c=20.0,
                        nodes={1: NodeLocation(1, "water/cluster", "tank")},
                        tanks={})
    assert derive.derive_tank(reading(), cfg) == []


def test_derivation_never_raises_on_a_malformed_reading():
    # The daemon must survive junk (packet.py's contract) — a derivation that throws
    # takes the ingest loop down with it.
    assert derive.derive_tank({}, CFG) == []
    assert derive.derive_tank({"node_id": 1}, CFG) == []
    assert derive.derive_tank({"node_id": 1, "channels": "not a list"}, CFG) == []
