"""Bed-node derivation gating — the plumbing half of issue #20 (Phase 2.2).

Sibling of test_derive.py, which covers the tank. Split into its own file because the
gating questions are different: the tank withholds gallons until someone measures the
geometry, and the bed withholds tension until it knows the soil temperature.

The tests that matter most are the ones asserting a value is NOT published. Each is
paired with the adjacent legal case in the same test, because `derive_bed` returning
`[]` unconditionally satisfies an empty-result assertion on its own
(.claude/CLAUDE-context.md § Testing).
"""
from __future__ import annotations

from soundings_gateway import derive
from soundings_gateway.config import GatewayConfig, NodeLocation, TankGeometry

BED = "tunnel/bed-north"

CFG = GatewayConfig(
    ref_temp_c=20.0,
    nodes={
        2: NodeLocation(2, BED, "bed", "North tunnel, tomato bed"),
        1: NodeLocation(1, "water/cluster", "tank", "Rain catchment cluster"),
        9: NodeLocation(9, "shed/roof", "weather", "A role with no derivation"),
    },
    tanks={"water/cluster": TankGeometry(measured=True, capacity_gal=1490.0,
                                         sensor_zero_mm=2100, max_height_mm=2000,
                                         breakpoint_mm=1150, gal_per_mm_below=1.0,
                                         gal_per_mm_above=0.4)},
)


def reading(*, node_id=2, tension_raw=50, temp_raw=320,
            tension_fault=False, temp_fault=False, channels=None):
    """A decoded bed reading. `tension_raw` is the contract's 0.1 kΩ/LSB, so 50 = 5.0 kΩ;
    `temp_raw` is the DS18B20's native 1/16 °C, so 320 = 20.0 °C."""
    if channels is None:
        channels = [
            {"name": "SOIL_TENSION_0", "bit": 0, "raw": tension_raw, "fault": tension_fault},
            {"name": "SOIL_TEMP_0", "bit": 4, "raw": temp_raw, "fault": temp_fault},
        ]
    return {"node_id": node_id, "seq": 1, "battery_mv": 3800,
            "channels": channels, "received_at": 1_700_000_000.0}


def topics(pairs):
    return {t: v for t, v in pairs}


def assert_derives_normally():
    """Positive control, paired with every "derives nothing" assertion in this file."""
    assert topics(derive.derive_bed(reading(), CFG)) != {}


# ---- The happy path ---------------------------------------------------------

def test_a_watermark_and_its_depth_probe_produce_a_tension():
    # 5.0 kΩ at 20 °C = 28.38 cb — the middle of the operator's 25-30 cb trigger.
    out = topics(derive.derive_bed(reading(), CFG))
    assert out[f"farm/soundings/{BED}/tension_0_kpa"] == "28.38"
    assert out[f"farm/soundings/{BED}/soil_temp_4_c"] == "20.00"


def test_each_watermark_is_compensated_by_the_probe_at_its_own_depth():
    # SPEC §5.1 stacks 6" and 12". Same resistance at both depths, different soil temps,
    # so a correct pairing gives two DIFFERENT tensions — and a pairing that fell back to
    # one probe for both would give the same number twice.
    #
    # 5.0 kΩ @ 10 °C = 24.26 cb   (6",  SOIL_TEMP_0)
    # 5.0 kΩ @ 30 °C = 34.18 cb   (12", SOIL_TEMP_1)
    out = topics(derive.derive_bed(reading(channels=[
        {"name": "SOIL_TENSION_0", "bit": 0, "raw": 50, "fault": False},
        {"name": "SOIL_TENSION_1", "bit": 1, "raw": 50, "fault": False},
        {"name": "SOIL_TEMP_0", "bit": 4, "raw": 160, "fault": False},   # 10.0 °C
        {"name": "SOIL_TEMP_1", "bit": 5, "raw": 480, "fault": False},   # 30.0 °C
    ]), CFG))
    assert out[f"farm/soundings/{BED}/tension_0_kpa"] == "24.26"
    assert out[f"farm/soundings/{BED}/tension_1_kpa"] == "34.18"


def test_the_homemade_pair_shares_the_depth_probe_of_the_sensor_beside_it():
    # Bits 2 and 3 are the homemade blocks, installed SIDE BY SIDE with bits 0 and 1 —
    # so bit 2 takes the 6" probe and bit 3 takes the 12" one. Same resistance on 0 and
    # 2 must therefore give the same tension.
    out = topics(derive.derive_bed(reading(channels=[
        {"name": "SOIL_TENSION_0", "bit": 0, "raw": 50, "fault": False},
        {"name": "SOIL_TENSION_2", "bit": 2, "raw": 50, "fault": False},
        {"name": "SOIL_TENSION_1", "bit": 1, "raw": 100, "fault": False},
        {"name": "SOIL_TENSION_3", "bit": 3, "raw": 100, "fault": False},
        {"name": "SOIL_TEMP_0", "bit": 4, "raw": 160, "fault": False},   # 10.0 °C
        {"name": "SOIL_TEMP_1", "bit": 5, "raw": 480, "fault": False},   # 30.0 °C
    ]), CFG))
    assert out[f"farm/soundings/{BED}/tension_0_kpa"] == out[f"farm/soundings/{BED}/tension_2_kpa"]
    assert out[f"farm/soundings/{BED}/tension_1_kpa"] == out[f"farm/soundings/{BED}/tension_3_kpa"]
    # And the two depths must not have collapsed onto one probe.
    assert out[f"farm/soundings/{BED}/tension_0_kpa"] != out[f"farm/soundings/{BED}/tension_1_kpa"]


# ---- Refusals, each with its positive control -------------------------------

def test_a_watermark_with_no_soil_temp_derives_no_tension_but_the_probe_still_would():
    # SPEC §5.1 calls the compensation mandatory. An uncompensated value is not a
    # rougher answer, it is a different one — at 5 kΩ the 10 °C-to-30 °C swing straddles
    # the irrigation trigger. So it is withheld, exactly as the tank withholds gallons.
    out = topics(derive.derive_bed(reading(channels=[
        {"name": "SOIL_TENSION_0", "bit": 0, "raw": 50, "fault": False},
    ]), CFG))
    assert out == {}
    assert_derives_normally()


def test_a_faulted_soil_temp_withholds_the_tension_it_would_have_compensated():
    out = topics(derive.derive_bed(reading(temp_fault=True), CFG))
    assert f"farm/soundings/{BED}/tension_0_kpa" not in out
    assert_derives_normally()


def test_a_faulted_watermark_publishes_no_tension_and_the_temp_still_goes_out():
    # A declared sensor that did not answer (DEC-002). The soil temperature is a real
    # measurement from a different probe and is unaffected — dropping it too would
    # discard good data because a neighbour failed.
    out = topics(derive.derive_bed(reading(tension_fault=True), CFG))
    assert f"farm/soundings/{BED}/tension_0_kpa" not in out
    assert out[f"farm/soundings/{BED}/soil_temp_4_c"] == "20.00"


def test_a_resistance_past_the_curves_pole_is_dropped_and_one_below_it_is_not():
    # 78.0 kΩ at 20 °C puts the denominator at -0.000174. Past the pole the equation
    # returns large negative tensions that would look like readings.
    past = topics(derive.derive_bed(reading(tension_raw=780), CFG))
    below = topics(derive.derive_bed(reading(tension_raw=770), CFG))
    assert f"farm/soundings/{BED}/tension_0_kpa" not in past
    assert f"farm/soundings/{BED}/tension_0_kpa" in below


def test_the_wet_end_is_flagged_and_published_not_dropped():
    # 0.5 kΩ at 20 °C = 5.4 cb. Below the 10 cb floor the number is directional rather
    # than quantitative — but "far wetter than you would irrigate at" is true and
    # actionable, and a dropped reading looks like a dead sensor.
    wet = topics(derive.derive_bed(reading(tension_raw=5), CFG))
    assert f"farm/soundings/{BED}/tension_0_kpa" in wet
    assert wet[f"farm/soundings/{BED}/tension_0_wet_end"] == "1"
    # The positive control: a normal reading carries no flag at all.
    assert f"farm/soundings/{BED}/tension_0_wet_end" not in topics(derive.derive_bed(reading(), CFG))


# ---- Role routing -----------------------------------------------------------

def test_derive_bed_ignores_a_tank_node_and_derive_tank_ignores_a_bed_node():
    assert derive.derive_bed(reading(node_id=1), CFG) == []
    assert derive.derive_tank(reading(node_id=2), CFG) == []
    assert_derives_normally()


def test_derive_reading_routes_by_role():
    bed = topics(derive.derive_reading(reading(node_id=2), CFG))
    assert f"farm/soundings/{BED}/tension_0_kpa" in bed
    tank = topics(derive.derive_reading(
        {"node_id": 1, "channels": [
            {"name": "TANK_DISTANCE", "bit": 8, "raw": 600, "fault": False},
            {"name": "SOIL_TEMP_0", "bit": 4, "raw": 320, "fault": False},
        ]}, CFG))
    assert "farm/soundings/water/cluster/level_gal" in tank


def test_derive_reading_on_an_unmapped_node_and_an_unknown_role_is_quiet():
    assert derive.derive_reading(reading(node_id=99), CFG) == []
    assert derive.derive_reading(reading(node_id=9), CFG) == []
    # Positive control: routing works for a role that does have a derivation.
    assert derive.derive_reading(reading(node_id=2), CFG) != []


# ---- Malformed input never reaches the daemon -------------------------------

def test_a_malformed_reading_derives_nothing_rather_than_raising():
    assert derive.derive_bed({}, CFG) == []
    assert derive.derive_bed({"node_id": 2}, CFG) == []
    assert derive.derive_bed({"node_id": 2, "channels": "not a list"}, CFG) == []
    assert_derives_normally()
