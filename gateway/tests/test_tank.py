"""Tank derivation math — the physics half of Phase 3.6 (issue #45).

Pinned to computed golden values, not to whatever the implementation happens to
return. Every expected number below is derived by hand from the formula in DEC-007
or from the geometry in the test's own fixture, so a rewrite of tank.py has something
to be wrong against.
"""
from __future__ import annotations

import pytest

from soundings_gateway import tank
from soundings_gateway.config import TankGeometry


# A deliberately round fixture: 2000 mm of usable height, breakpoint at 1150 mm
# (the IBC's ~46 in), 1.0 gal/mm below and 0.4 above. Not the real tanks — the real
# ones are unmeasured — but arithmetic anyone can check in their head.
GEOM = TankGeometry(
    measured=True,
    capacity_gal=1490.0,          # 1150*1.0 + 850*0.4 = 1150 + 340
    sensor_zero_mm=2100,
    max_height_mm=2000,
    breakpoint_mm=1150,
    gal_per_mm_below=1.0,
    gal_per_mm_above=0.4,
)


# ---- Speed of sound ---------------------------------------------------------

def test_speed_of_sound_at_zero_c_is_the_bare_constant():
    assert tank.speed_of_sound_mps(0.0) == pytest.approx(331.3)


def test_speed_of_sound_at_twenty_c():
    # 331.3 + 0.606*20 = 343.42 — the number every textbook quotes for room air.
    assert tank.speed_of_sound_mps(20.0) == pytest.approx(343.42)


def test_speed_of_sound_below_freezing_is_slower_not_clamped():
    assert tank.speed_of_sound_mps(-10.0) == pytest.approx(325.24)


# ---- Temperature correction -------------------------------------------------

def test_correction_at_the_reference_temperature_is_a_no_op():
    assert tank.correct_distance_mm(2000, 20.0, 20.0) == pytest.approx(2000.0)


def test_hot_headspace_reads_long_and_is_corrected_upward():
    # Sound travels faster when hot, so a fixed flight time covers more distance:
    # the true distance is LARGER than the sensor's 20 °C-calibrated report.
    corrected = tank.correct_distance_mm(2000, 40.0, 20.0)
    assert corrected > 2000
    # 2000 * (331.3+0.606*40)/(331.3+0.606*20) = 2000 * 355.54/343.42
    assert corrected == pytest.approx(2070.58, abs=0.01)


def test_cold_headspace_is_corrected_downward():
    # 2000 * 331.3/343.42 = 1929.416
    assert tank.correct_distance_mm(2000, 0.0, 20.0) == pytest.approx(1929.42, abs=0.01)


# This is the error DEC-007 exists to remove: over a 2 m headspace, 0 °C → 40 °C is
# ~14 cm of apparent level change with no water moving, against a sensor specified to
# ±1 cm. If this assertion ever softens, the correction has stopped doing its job.
def test_the_uncorrected_swing_over_the_seasonal_range_is_about_14_cm():
    hot = tank.correct_distance_mm(2000, 40.0, 20.0)
    cold = tank.correct_distance_mm(2000, 0.0, 20.0)
    assert (hot - cold) == pytest.approx(141.2, abs=1.0)


# ---- Height -----------------------------------------------------------------

def test_height_is_the_zero_reading_minus_the_corrected_distance():
    assert tank.water_height_mm(600.0, GEOM.sensor_zero_mm, GEOM.max_height_mm) == pytest.approx(1500.0)


def test_empty_tank_reads_zero_height():
    assert tank.water_height_mm(2100.0, GEOM.sensor_zero_mm, GEOM.max_height_mm) == pytest.approx(0.0)


# Noise at a full tank can put the corrected distance slightly past the zero point.
# That is not a negative volume, it is an empty tank plus jitter.
def test_a_reading_past_the_zero_point_clamps_to_empty_not_negative():
    assert tank.water_height_mm(2150.0, GEOM.sensor_zero_mm, GEOM.max_height_mm) == 0.0


# A tank filled past its overflow is still a full tank, not 105 % of one.
def test_a_reading_above_max_height_clamps_to_full():
    assert tank.water_height_mm(50.0, GEOM.sensor_zero_mm, GEOM.max_height_mm) == 2000.0


# ---- Volume -----------------------------------------------------------------

def test_empty_is_zero_gallons():
    assert tank.gallons_from_height(0.0, GEOM) == pytest.approx(0.0)


def test_below_the_breakpoint_uses_the_steep_segment():
    assert tank.gallons_from_height(500.0, GEOM) == pytest.approx(500.0)


def test_exactly_at_the_breakpoint():
    assert tank.gallons_from_height(1150.0, GEOM) == pytest.approx(1150.0)


# The whole reason the curve is two segments: once the IBC tops out, only the two
# cylinders keep rising, so the same millimetre is worth fewer gallons.
def test_above_the_breakpoint_uses_the_shallow_segment():
    # 1150 + (1500-1150)*0.4 = 1150 + 140
    assert tank.gallons_from_height(1500.0, GEOM) == pytest.approx(1290.0)


def test_the_curve_is_continuous_across_the_breakpoint():
    below = tank.gallons_from_height(1149.9, GEOM)
    above = tank.gallons_from_height(1150.1, GEOM)
    assert abs(above - below) < 1.0


def test_full_height_reaches_capacity():
    assert tank.gallons_from_height(2000.0, GEOM) == pytest.approx(GEOM.capacity_gal)


# ---- Percent ----------------------------------------------------------------

def test_percent_of_capacity():
    assert tank.percent_full(745.0, 1490.0) == pytest.approx(50.0)


def test_percent_clamps_at_a_hundred():
    assert tank.percent_full(1600.0, 1490.0) == 100.0


def test_percent_never_goes_negative():
    assert tank.percent_full(-5.0, 1490.0) == 0.0


# A zero capacity is an unmeasured tank, not a division to attempt.
def test_percent_of_zero_capacity_is_zero_not_an_exception():
    assert tank.percent_full(100.0, 0.0) == 0.0
