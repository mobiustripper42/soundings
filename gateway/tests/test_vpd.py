"""VPD math — the tunnel-air measurement (issue #21, Phase 2.3).

Every expected number below is derived from the SPEC §5.3 equations with stdlib `math`
only, written out in the comment above the assertion, so a rewrite of vpd.py has
something to be wrong against. The equations:

    SVP = 0.61078 · exp((17.27·T) / (T + 237.3))    [kPa]
    AVP = SVP · (RH / 100)
    VPD = SVP − AVP

⚠ The vectors in contracts/vectors/vpd-v1.json were generated the same way — NOT by
calling soundings_gateway.vpd. That independence is the same rule test_ds18b20.cpp
states for its CRC literals: a fixture built by the code under test agrees with itself
however wrong both are.

The band that matters is the tomato daytime window, ~0.8–1.2 kPa (SPEC §5.3), so the
vectors cluster there rather than spreading evenly — the same reasoning test_tension.py
gives for clustering at the irrigation thresholds.

⚠ Refusal cases here always carry the adjacent legal case in the SAME test — see
.claude/CLAUDE-context.md § Testing. A test that only asserts None passes against a
function that returns None unconditionally.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

from soundings_gateway import vpd


VECTORS = Path(__file__).resolve().parents[2] / "contracts" / "vectors" / "vpd-v1.json"


def _load():
    return json.loads(VECTORS.read_text())


# ---- Tick conversions, straight off the channel registry ---------------------


def test_tick_conversions_match_the_channel_registry():
    # contracts/packet-v1.md bits 6 and 7:
    #   T_°C = -45 + 175·ticks/65535      RH_% = -6 + 125·ticks/65535
    # Both endpoints are exact by construction, so they pin the formula's shape rather
    # than its rounding.
    assert vpd.air_temp_c(0) == pytest.approx(-45.0)
    assert vpd.air_temp_c(65535) == pytest.approx(130.0)
    assert vpd.air_rh_pct(0) == pytest.approx(-6.0)
    assert vpd.air_rh_pct(65535) == pytest.approx(119.0)

    # Midscale: -45 + 175/2 = 42.5, and -6 + 125/2 = 56.5
    assert vpd.air_temp_c(65535 / 2) == pytest.approx(42.5)
    assert vpd.air_rh_pct(65535 / 2) == pytest.approx(56.5)


# ---- The saturation curve ----------------------------------------------------


def test_svp_at_zero_celsius_is_the_bare_coefficient():
    # T = 0 makes the exponent (17.27·0)/(0+237.3) = 0, so exp() is exactly 1 and SVP
    # collapses to the leading coefficient. The one point on this curve that can be
    # checked without a calculator, which is exactly why it is worth asserting.
    assert vpd.svp_kpa(0.0) == pytest.approx(0.61078, abs=1e-9)


def test_svp_matches_hand_computed_points():
    # 0.61078·exp((17.27·20)/257.3) = 0.61078·exp(1.3424019) = 2.3382047
    assert vpd.svp_kpa(20.0) == pytest.approx(2.3382047, abs=1e-6)
    # 0.61078·exp((17.27·25)/262.3) = 0.61078·exp(1.6460160) = 3.1676740
    assert vpd.svp_kpa(25.0) == pytest.approx(3.1676740, abs=1e-6)
    # Below freezing the curve is still valid — the air simply holds almost nothing.
    # 0.61078·exp((17.27·-5)/232.3) = 0.61078·exp(-0.3717176) = 0.4211627
    assert vpd.svp_kpa(-5.0) == pytest.approx(0.4211627, abs=1e-6)


def test_svp_rises_monotonically_with_temperature():
    # Not a spot check — the property. A sign slip inside the exponent would still pass
    # a single point if that point were chosen badly.
    temps = [-40.0, -10.0, 0.0, 10.0, 20.0, 30.0, 40.0, 60.0, 100.0]
    values = [vpd.svp_kpa(t) for t in temps]
    assert values == sorted(values)
    assert all(v > 0 for v in values)


# ---- VPD itself --------------------------------------------------------------


def test_vpd_across_the_golden_vectors():
    for v in _load()["vectors"]:
        t = vpd.air_temp_c(v["air_temp_ticks"])
        rh = vpd.air_rh_pct(v["air_rh_ticks"])
        assert t == pytest.approx(v["t_c"], abs=1e-5), v["name"]
        assert rh == pytest.approx(v["rh_pct"], abs=1e-5), v["name"]
        assert vpd.svp_kpa(t) == pytest.approx(v["svp_kpa"], abs=1e-5), v["name"]
        assert vpd.vpd_kpa(t, rh) == pytest.approx(v["vpd_kpa"], abs=1e-5), v["name"]


def test_the_tomato_band_vectors_actually_land_in_the_tomato_band():
    # The vectors are only worth clustering there if they land there. This catches a
    # fixture edited into meaninglessness as much as it catches the math.
    lo, hi = _load()["decision_bands_kpa"]["tomato_daytime"]
    named = {v["name"]: v for v in _load()["vectors"]}
    for name in (
        "tomato band, typical greenhouse day",
        "tomato band, warm afternoon",
        "tomato band, cool morning — the low edge of the band",
    ):
        t = vpd.air_temp_c(named[name]["air_temp_ticks"])
        rh = vpd.air_rh_pct(named[name]["air_rh_ticks"])
        assert lo <= vpd.vpd_kpa(t, rh) <= hi, name


# ---- Saturation, and the floor it needs -------------------------------------


def test_saturated_air_floors_at_zero_and_dry_air_does_not():
    # ⚠ The reason the floor exists. The tick grid cannot land exactly on 100 %RH: the
    # nearest tick above gives 100.000610 %, and SVP·(1 − 1.0000061) is about
    # -1.4e-5 kPa. Negative drying power is not a thing — it renders as a bar below the
    # axis and reads as a broken sensor rather than as saturated air.
    assert vpd.vpd_kpa(20.001144, 100.000610) == 0.0
    assert vpd.vpd_kpa(20.0, 100.0) == 0.0

    # The adjacent legal case, without which the above passes against a function that
    # returns 0.0 unconditionally. At 99 %RH there is still a little drying power:
    # 2.338363 · (1 − 0.99) = 0.023384
    assert vpd.vpd_kpa(20.0, 99.0) == pytest.approx(0.023384, abs=1e-5)


def test_bone_dry_air_gives_back_the_whole_saturation_pressure():
    # RH = 0 means AVP = 0, so VPD is SVP exactly. The opposite end from saturation, and
    # it catches an inverted (RH/100) term that the mid-band vectors would not.
    assert vpd.vpd_kpa(30.0, 0.0) == pytest.approx(vpd.svp_kpa(30.0), abs=1e-9)


# ---- The physical band, which is narrower than the tick range ---------------


def test_full_scale_ticks_are_out_of_band_and_ordinary_ticks_are_not():
    # ⚠ The registry conversion spans -45..130 °C and -6..119 %RH. That is the RAW TICK
    # range, not the SHT45's physical range. Unclamped, full-scale ticks compute a VPD of
    # about -52 kPa, which is the clearest possible demonstration that the two differ.
    for case in _load()["out_of_band"]:
        t = vpd.air_temp_c(case["air_temp_ticks"])
        rh = vpd.air_rh_pct(case["air_rh_ticks"])
        assert vpd.out_of_band(t, rh), case["name"]

    # Paired legal case: every ordinary vector must be IN band, or the check above is
    # satisfied by a function that calls everything broken.
    for v in _load()["vectors"]:
        t = vpd.air_temp_c(v["air_temp_ticks"])
        rh = vpd.air_rh_pct(v["air_rh_ticks"])
        assert not vpd.out_of_band(t, rh), v["name"]


def test_out_of_band_is_reported_not_enforced():
    # vpd.py stays pure math; whether a reading is trustworthy enough to publish is
    # derive.py's call, the same split tension.py:8-10 describes. So an out-of-band
    # input still returns a number — it is simply flagged.
    assert vpd.out_of_band(130.0, 119.0)
    assert isinstance(vpd.vpd_kpa(130.0, 119.0), float)


def test_the_band_edges_are_inclusive():
    # -40 and 125 are the SHT45's specified limits, not one step outside them. An
    # exclusive comparison would reject a sensor sitting exactly at spec.
    assert not vpd.out_of_band(-40.0, 0.0)
    assert not vpd.out_of_band(125.0, 100.0)
    assert vpd.out_of_band(-40.001, 50.0)
    assert vpd.out_of_band(125.001, 50.0)
    assert vpd.out_of_band(20.0, -1.0)


def test_saturation_is_not_mistaken_for_a_broken_sensor():
    # ⚠ Found by this test file before the module shipped, and worth keeping as the
    # named reason the tolerance exists.
    #
    # The tick grid cannot land on exactly 100 %RH — the nearest tick above is
    # 100.000610 %. A naive `rh <= 100` check calls every saturated reading a broken
    # sensor, which is both wrong and the exact condition a greenhouse hits on a cool
    # morning. One tick is 125/65535 = 0.0019 %RH, so anything within a tick of the
    # limit is quantisation, not a fault.
    assert not vpd.out_of_band(20.0, 100.000610)
    assert not vpd.out_of_band(20.0, -0.000610)

    # And the paired case that must still be caught: beyond quantisation is a real
    # fault. Half a percent is 260 ticks past the limit — nothing rounds that far.
    assert vpd.out_of_band(20.0, 100.5)
    assert vpd.out_of_band(20.0, -0.5)


# ---- The whole path, ticks to kPa -------------------------------------------


def test_vpd_from_ticks_matches_the_two_step_path():
    # The convenience entry point derive.py will call. It must agree with doing it by
    # hand, or there are two curves in the codebase.
    for v in _load()["vectors"]:
        by_hand = vpd.vpd_kpa(
            vpd.air_temp_c(v["air_temp_ticks"]), vpd.air_rh_pct(v["air_rh_ticks"])
        )
        assert vpd.vpd_from_ticks(
            v["air_temp_ticks"], v["air_rh_ticks"]
        ) == pytest.approx(by_hand, abs=1e-12), v["name"]


def test_the_module_is_independent_of_the_vector_file():
    # The fixture is a record of the equation, not its definition. Recomputing one
    # vector from stdlib math here means an edit to the JSON that quietly changed a
    # number would fail rather than silently reset the expectation.
    v = _load()["vectors"][0]
    t = -45 + 175 * v["air_temp_ticks"] / 65535
    rh = -6 + 125 * v["air_rh_ticks"] / 65535
    svp = 0.61078 * math.exp((17.27 * t) / (t + 237.3))
    assert v["svp_kpa"] == pytest.approx(svp, abs=1e-5)
    assert v["vpd_kpa"] == pytest.approx(svp * (1 - rh / 100.0), abs=1e-5)


# ---- The sim and the field must agree ---------------------------------------


def test_the_emitters_tick_encoding_is_the_exact_inverse_of_this_module():
    """⚠ The sim encodes SHT45 ticks and this module decodes them. Nothing else pins
    those two formulas together.

    emitter._sht_temp_ticks / _sht_rh_ticks invert air_temp_c / air_rh_pct by hand, in a
    different file, with the constants written out twice. Edit one and the sim silently
    disagrees with the field — every chart shifts and no test fails. Raised in review on
    issue #21 as the drift this task most plausibly ships.
    """
    from soundings_gateway import emitter

    for t_c in (-40.0, -5.0, 0.0, 16.0, 20.0, 25.0, 37.5, 100.0, 125.0):
        # One tick is 175/65535 = 0.00267 °C, so a round trip cannot lose more than that.
        assert vpd.air_temp_c(emitter._sht_temp_ticks(t_c)) == pytest.approx(t_c, abs=0.003)

    for rh in (0.0, 12.5, 50.0, 60.0, 99.0, 100.0):
        # One tick is 125/65535 = 0.0019 %RH.
        assert vpd.air_rh_pct(emitter._sht_rh_ticks(rh)) == pytest.approx(rh, abs=0.002)


def test_the_sims_default_node_stays_inside_the_sensors_band():
    """A NodeSpec default nudged toward the SHT45's limits would start producing
    out-of-band readings, and derive.py would quietly stop publishing VPD for the whole
    sim fleet. The failure is a chart that goes empty, which looks like a broken pipeline
    rather than like a fixture someone edited.
    """
    from soundings_gateway import emitter

    spec = emitter.NodeSpec(node_id=2)
    # The emitter swings air temp by ±4 °C and RH by ∓12 % about the defaults, so check
    # the extremes of that swing rather than the midpoint.
    for t_c in (spec.air_temp_c - 4.0, spec.air_temp_c + 4.0):
        assert not vpd.temp_out_of_band(t_c), t_c
    for rh in (spec.air_rh_pct - 12.0, spec.air_rh_pct + 12.0):
        assert not vpd.rh_out_of_band(rh), rh


def test_the_two_halves_of_the_band_check_are_independent():
    """⚠ Split out of `out_of_band` on review of issue #21.

    derive.py publishes air temperature and humidity on their own merits and gates only
    the derived VPD on having both — the same split the soil temperatures take. That
    needs a per-channel verdict: a humidity channel reading 119 % must not throw away a
    perfectly good 20 °C sitting beside it.
    """
    assert vpd.rh_out_of_band(119.0)
    assert not vpd.temp_out_of_band(20.0)
    assert vpd.out_of_band(20.0, 119.0)          # the composite still says "no VPD"

    # And the mirror, so neither predicate can be the one that always answers.
    assert vpd.temp_out_of_band(130.0)
    assert not vpd.rh_out_of_band(60.0)
    assert vpd.out_of_band(130.0, 60.0)

    # Both fine means the composite is fine.
    assert not vpd.out_of_band(20.0, 60.0)
