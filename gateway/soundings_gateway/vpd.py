"""Vapour-pressure-deficit math — SHT45 raw ticks into kPa, in Python (DEC-004).

None of this runs on the node. The node puts raw SHT45 ticks on the wire — bits 6 and 7
of the channel registry, `contracts/packet-v1.md` — and the curve lives here so it can be
re-fitted against stored raw readings without reflashing a node in a tunnel. §12 D1
resolved that split on 2026-07-10; tension.py and tank.py sit either side of this file
for the same reason.

**VPD is computed, not measured** (SPEC §5.3). There is no VPD sensor. You measure air
temperature and relative humidity, and the drying power of the air falls out:

    SVP = 0.61078 · exp((17.27·T) / (T + 237.3))    [kPa]   saturation pressure
    AVP = SVP · (RH / 100)                          [kPa]   what the air is holding
    VPD = SVP − AVP                                 [kPa]   the deficit — the drying power

Pure functions, no I/O, no config reading. The gating decisions — whether a reading is
trustworthy enough to publish — live in derive.py, the same way tension.py and tank.py
leave them there. `out_of_band` reports; it does not refuse.

**Scope.** This is the ticks → kPa half of issue #21. The other half is an SHT45 driver
on real silicon, which is Phase 6.1 and has no hardware yet. That is not a size split:
the node's side of this is already finished — the registry pins both channels at 2 bytes
(`firmware/src/core/packet.h`) and names the exact tick conversions, so there is nothing
left to decide on the wire.
"""
from __future__ import annotations

import math

__all__ = [
    "TICKS_FULL_SCALE",
    "SVP_COEFF_KPA",
    "SVP_EXP_A",
    "SVP_EXP_B",
    "RH_MIN_PCT",
    "RH_MAX_PCT",
    "RH_TICK_PCT",
    "SHT45_T_MIN_C",
    "SHT45_T_MAX_C",
    "TOMATO_BAND_KPA",
    "air_temp_c",
    "air_rh_pct",
    "svp_kpa",
    "avp_kpa",
    "vpd_kpa",
    "vpd_from_ticks",
    "out_of_band",
]

#: Both SHT45 channels are u16, so full scale is 0xFFFF (packet-v1.md bits 6 and 7).
TICKS_FULL_SCALE = 65535.0

# SPEC §5.3. Kept as named constants rather than inlined so a re-fit against greenhouse
# data is a diff a person can read, which is the whole point of the math living here.
SVP_COEFF_KPA = 0.61078
SVP_EXP_A = 17.27
SVP_EXP_B = 237.3

#: Physically meaningful relative humidity. Nothing outside this exists.
RH_MIN_PCT = 0.0
RH_MAX_PCT = 100.0

#: One AIR_RH tick, in percent: 125/65535 ≈ 0.0019 %RH.
#:
#: ⚠ THE BAND CHECK NEEDS THIS OR IT CONDEMNS SATURATED AIR. The tick grid cannot land
#: on exactly 100 %RH — the nearest tick above is 100.000610 % — so a plain `rh <= 100`
#: calls every saturated reading a broken sensor. That is the condition a greenhouse
#: meets on any cool morning. Within one tick of a limit is quantisation; beyond it is a
#: fault. Half a percent, for scale, is 260 ticks past the limit.
RH_TICK_PCT = 125.0 / TICKS_FULL_SCALE

#: ⚠ THE SENSOR'S RANGE, WHICH IS NARROWER THAN THE TICK RANGE.
#:
#: The registry conversions span -45..130 °C and -6..119 %RH. Those are the endpoints of
#: the raw tick grid, NOT values an SHT45 can be at. Full-scale ticks on both channels
#: compute a VPD of about -52 kPa, which is the clearest demonstration available that the
#: two ranges are different things. A reading outside the band below is a broken sensor,
#: a disconnected one answering with nothing, or condensation — not weather.
SHT45_T_MIN_C = -40.0
SHT45_T_MAX_C = 125.0

#: Tomato daytime target window (SPEC §5.3). Carried here so the band a person cares
#: about is next to the math that produces it; nothing in this module enforces it.
TOMATO_BAND_KPA = (0.8, 1.2)


def air_temp_c(ticks: float) -> float:
    """Raw AIR_TEMP ticks to °C. `contracts/packet-v1.md` bit 6."""
    return -45.0 + 175.0 * ticks / TICKS_FULL_SCALE


def air_rh_pct(ticks: float) -> float:
    """Raw AIR_RH ticks to percent. `contracts/packet-v1.md` bit 7."""
    return -6.0 + 125.0 * ticks / TICKS_FULL_SCALE


def svp_kpa(t_c: float) -> float:
    """Saturation vapour pressure at `t_c`, in kPa — how much the air *could* hold.

    Valid below freezing: the curve keeps falling rather than doing anything strange, it
    just describes air that holds almost nothing. At exactly 0 °C the exponent vanishes
    and this collapses to SVP_COEFF_KPA, which is the one point on the curve checkable
    without a calculator.
    """
    return SVP_COEFF_KPA * math.exp((SVP_EXP_A * t_c) / (t_c + SVP_EXP_B))


def avp_kpa(t_c: float, rh_pct: float) -> float:
    """Actual vapour pressure — what the air *is* holding, in kPa."""
    return svp_kpa(t_c) * _clamped_rh(rh_pct) / 100.0


def vpd_kpa(t_c: float, rh_pct: float) -> float:
    """Vapour pressure deficit in kPa: the drying power of the air.

    ⚠ FLOORED AT ZERO, and the floor is not defensive tidying — it is load-bearing.
    The tick grid cannot land exactly on 100 %RH; the nearest tick above gives
    100.000610 %, and SVP·(1 − 1.0000061) is about -1.4e-5 kPa. Saturated air has zero
    drying power, never negative. Left alone, a saturated sensor draws a bar below the
    axis, which reads as a fault rather than as a muggy morning.

    Clamping happens on RH rather than on the result, so the floor is reached by the
    physics rather than applied over the top of it.
    """
    return max(0.0, svp_kpa(t_c) - avp_kpa(t_c, rh_pct))


def vpd_from_ticks(temp_ticks: float, rh_ticks: float) -> float:
    """The whole path — raw channel values straight to kPa. What derive.py calls."""
    return vpd_kpa(air_temp_c(temp_ticks), air_rh_pct(rh_ticks))


def out_of_band(t_c: float, rh_pct: float) -> bool:
    """Is this reading outside what an SHT45 can physically be?

    Reports; does not refuse. `vpd_kpa` still returns a number for an out-of-band input,
    because deciding whether to publish it is derive.py's job and this module has no
    business having an opinion about a gateway's policy.

    Edges are inclusive: -40 and 125 °C are the sensor's specified limits, not one step
    outside them, and a probe sitting exactly at spec is at spec.

    The RH limits carry one tick of slack on each side — see RH_TICK_PCT. Temperature
    does not, because its band sits well inside the tick range and quantisation cannot
    push a real reading across it.
    """
    if not SHT45_T_MIN_C <= t_c <= SHT45_T_MAX_C:
        return True
    return not (RH_MIN_PCT - RH_TICK_PCT) <= rh_pct <= (RH_MAX_PCT + RH_TICK_PCT)


def _clamped_rh(rh_pct: float) -> float:
    """RH pinned into 0..100. See `vpd_kpa` for why the upper clamp has to exist."""
    return min(RH_MAX_PCT, max(RH_MIN_PCT, rh_pct))
