"""Watermark tension math — the anchor measurement (issue #20, Phase 2.2).

Every expected number below is derived by hand from the SPEC §5.1 equation, written
out in the comment above the assertion, so a rewrite of tension.py has something to be
wrong against. The equation:

    kPa = (4.093 + 3.213·R) / (1 - 0.009733·R - 0.01205·T)      R in kΩ, T in °C

The decision thresholds this math exists to serve are the operator's own, recovered
from a 2018 field season: irrigate at 25-30 cb on the shallow sensor, stress at
50-60 cb. The vectors and the anchors below cluster there rather than spreading evenly
across the 0-239 cb scale, because that band is where a wrong number changes what you do.

⚠ Refusal cases here always carry the adjacent legal case in the SAME test — see
.claude/CLAUDE-context.md § Testing. A test that only asserts None passes against a
function that returns None unconditionally.
"""
from __future__ import annotations

import json
from pathlib import Path

import pytest

from soundings_gateway import tension


VECTORS = Path(__file__).resolve().parents[2] / "contracts" / "vectors" / "tension-v1.json"


# ---- Raw wire units ---------------------------------------------------------

def test_raw_counts_convert_at_a_tenth_of_a_kilohm_per_lsb():
    # contracts/packet-v1.md § Channel registry: SOIL_TENSION_* is 0.1 kΩ/LSB.
    assert tension.resistance_kohm(50) == pytest.approx(5.0)
    assert tension.resistance_kohm(1) == pytest.approx(0.1)


def test_the_registry_ceiling_converts_without_overflow():
    # 0xFFFE is the largest value the contract says a node may emit as a real reading.
    assert tension.resistance_kohm(0xFFFE) == pytest.approx(6553.4)


# ---- The equation, at the thresholds that matter ----------------------------

def test_trigger_band_five_kilohms_is_about_twenty_eight_centibars():
    # num = 4.093 + 3.213*5      = 20.158
    # den = 1 - 0.048665 - 0.241 = 0.710335
    # 20.158 / 0.710335 = 28.3782
    assert tension.tension_kpa(5.0, 20.0) == pytest.approx(28.3782, abs=0.001)


def test_stress_band_ten_kilohms_is_about_fifty_five_centibars():
    # num = 4.093 + 32.13      = 36.223
    # den = 1 - 0.09733 - 0.241 = 0.66167
    # 36.223 / 0.66167 = 54.7448
    assert tension.tension_kpa(10.0, 20.0) == pytest.approx(54.7448, abs=0.001)


def test_one_kilohm_is_just_under_ten_centibars():
    # num = 4.093 + 3.213        = 7.306
    # den = 1 - 0.009733 - 0.241 = 0.749267
    # 7.306 / 0.749267 = 9.7509
    assert tension.tension_kpa(1.0, 20.0) == pytest.approx(9.7509, abs=0.001)


# ---- Temperature compensation is load-bearing -------------------------------

def test_the_same_resistance_reads_differently_morning_and_afternoon():
    # SPEC §5.1: "Skipping temp compensation causes 20-30% drift between morning and
    # afternoon." This is that claim as an assertion. Same soil, same sensor, same
    # resistance — only the soil temperature moved.
    #
    # 10 °C: den = 1 - 0.048665 - 0.1205 = 0.830835 → 20.158 / 0.830835 = 24.2623
    # 30 °C: den = 1 - 0.048665 - 0.3615 = 0.589835 → 20.158 / 0.589835 = 34.1757
    morning = tension.tension_kpa(5.0, 10.0)
    afternoon = tension.tension_kpa(5.0, 30.0)
    assert morning == pytest.approx(24.2623, abs=0.001)
    assert afternoon == pytest.approx(34.1757, abs=0.001)
    # A 41 % swing on an unchanged sensor — larger than SPEC's 20-30 %, and it straddles
    # the 25-30 cb irrigation trigger. Uncompensated, the same soil reads "hold off" at
    # dawn and "irrigate" after lunch.
    assert (afternoon - morning) / morning > 0.30


def test_warmer_soil_always_reads_higher_tension_at_a_fixed_resistance():
    readings = [tension.tension_kpa(5.0, t) for t in (0.0, 10.0, 20.0, 30.0)]
    assert all(a < b for a, b in zip(readings, readings[1:]))


# ---- The pole: where the equation leaves its domain -------------------------

def test_the_denominator_pole_is_refused_and_the_value_just_below_it_is_not():
    # den = 1 - 0.009733·R - 0.241 hits zero at R = 0.759/0.009733 = 77.9822 kΩ.
    # Past it the denominator is negative and the formula returns large NEGATIVE
    # tensions that look like readings.
    #
    # 78.0 kΩ: den = 1 - 0.759174 - 0.241 = -0.000174  → refused
    # 77.0 kΩ: den = 1 - 0.749441 - 0.241 = +0.009559  → in domain (and clamped below)
    assert tension.tension_kpa(78.0, 20.0) is None
    assert tension.tension_kpa(77.0, 20.0) is not None


def test_the_pole_moves_with_temperature_so_a_fixed_ceiling_cannot_guard_it():
    # This is why the guard is on the denominator and not on R. The same 70 kΩ is
    # inside the domain in cold soil and outside it in warm soil.
    #
    #  0 °C: pole at 1/0.009733        = 102.744 kΩ  → 70 kΩ is fine
    # 30 °C: pole at 0.6385/0.009733   =  65.601 kΩ  → 70 kΩ is past it
    assert tension.tension_kpa(70.0, 0.0) is not None
    assert tension.tension_kpa(70.0, 30.0) is None


def test_a_negative_resistance_is_refused_and_zero_is_not():
    # Zero is a legal reading — a dead short across the block. Negative is not a
    # resistance at all.
    assert tension.tension_kpa(-1.0, 20.0) is None
    assert tension.tension_kpa(0.0, 20.0) is not None


# ---- The band ---------------------------------------------------------------

def test_the_scale_ceiling_clamps_and_a_reading_below_it_does_not():
    # Irrometer's scale tops out at 239 cb. Solving (4.093+3.213R)/(0.759-0.009733R)=239
    # gives R = 32.01 kΩ at 20 °C.
    #
    # 32.0 kΩ → 106.909 / 0.447544 = 238.87  → under the ceiling, passes through
    # 35.0 kΩ → 116.548 / 0.418345 = 278.60  → over it, clamps
    assert tension.tension_kpa(32.0, 20.0) == pytest.approx(238.87, abs=0.01)
    assert tension.tension_kpa(35.0, 20.0) == tension.TENSION_MAX_KPA


def test_the_near_pole_nonsense_value_is_what_the_clamp_actually_catches():
    # 77 kΩ at 20 °C computes to ~26,309 cb — two orders of magnitude past the scale.
    # The pole guard alone would let this through, because the denominator is still
    # positive. The clamp is the guard that makes the near-pole region safe.
    assert tension.tension_kpa(77.0, 20.0) == tension.TENSION_MAX_KPA


def test_the_wet_end_is_flagged_but_not_refused():
    # Below ~10 cb the Watermark is quantitatively unreliable, but "wetter than you
    # would ever irrigate at" is still the correct and actionable content. Flag it,
    # do not drop it — a dropped reading is indistinguishable from a dead sensor.
    #
    # kPa = 10 at R = 3.497/3.310 = 1.0565 kΩ (20 °C).
    assert tension.is_wet_end(tension.tension_kpa(0.5, 20.0)) is True
    assert tension.is_wet_end(tension.tension_kpa(5.0, 20.0)) is False
    assert tension.tension_kpa(0.5, 20.0) is not None


def test_is_wet_end_of_a_refused_reading_is_false_not_a_crash():
    assert tension.is_wet_end(None) is False
    assert tension.is_wet_end(2.0) is True


# ---- Coefficients are an input, not a constant ------------------------------

def test_the_default_coefficients_are_the_spec_equation():
    c = tension.IRROMETER_200SS
    assert (c.a, c.b, c.c, c.d) == (4.093, 3.213, 0.009733, 0.01205)


def test_a_different_curve_gives_a_different_answer_over_the_same_resistance():
    # SPEC §5.1 deploys commercial Watermarks and homemade gypsum blocks as matched
    # pairs on ONE node. A homemade block does not follow Irrometer's curve, so the
    # coefficients have to be per-channel or the second sensor is read with the first
    # one's calibration. Passing them in is what makes a re-fit a config change.
    steeper = tension.WatermarkCoefficients(a=4.093, b=4.000, c=0.009733, d=0.01205)
    default_kpa = tension.tension_kpa(5.0, 20.0)
    steeper_kpa = tension.tension_kpa(5.0, 20.0, steeper)
    assert steeper_kpa != default_kpa
    # num = 4.093 + 4.000*5 = 24.093; den unchanged 0.710335 → 33.9182
    assert steeper_kpa == pytest.approx(33.9182, abs=0.001)


# ---- Golden vectors ---------------------------------------------------------

def test_every_golden_vector_round_trips():
    """The vectors are generated from the SPEC equation by contracts/tools/
    gen_tension_vectors.py, independently of tension.py. A coefficient change shows
    up as a diff on that file rather than as a guess."""
    data = json.loads(VECTORS.read_text())
    assert data["vectors"], "vector file is empty — the generator did not run"
    for v in data["vectors"]:
        got = tension.tension_kpa(v["resistance_kohm"], v["temp_c"])
        if v["expected_kpa"] is None:
            assert got is None, f"{v['name']}: expected refusal, got {got}"
        else:
            assert got == pytest.approx(v["expected_kpa"], abs=0.01), v["name"]


def test_the_vector_file_covers_both_a_refusal_and_the_decision_band():
    data = json.loads(VECTORS.read_text())
    kinds = {v["kind"] for v in data["vectors"]}
    assert "refused" in kinds
    assert "trigger" in kinds
    assert "stress" in kinds
