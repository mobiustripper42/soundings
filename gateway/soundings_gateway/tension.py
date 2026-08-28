"""Watermark soil-tension math — raw resistance into centibars, in Python (DEC-004).

None of this runs on the node. The node puts raw resistance on the wire in 0.1 kΩ/LSB
(contracts/packet-v1.md § Channel registry) and the calibration curve lives here, so it
can be re-fitted against stored raw readings without reflashing a sensor buried at 12
inches. §12 D1 resolved that split on 2026-07-10.

Pure functions, no I/O, no config reading. The gating decisions — whether a reading is
trustworthy enough to publish a tension for — live in derive.py, the same way tank.py
leaves them there.

**Scope.** This is the kΩ → kPa half of issue #20. The other half, AC excitation and
ADC counts → kΩ, is firmware and is blocked on the excitation circuit (§12 D11, open).
That is not a size split: the counts→kΩ constant is set by a divider ratio and an
excitation voltage on a circuit that does not exist, so writing it now would pin an
invented number that no test could ever fail against.
"""
from __future__ import annotations

from dataclasses import dataclass

__all__ = [
    "WatermarkCoefficients",
    "IRROMETER_200SS",
    "RESISTANCE_COUNTS_PER_KOHM",
    "TENSION_MAX_KPA",
    "WET_END_KPA",
    "resistance_kohm",
    "tension_kpa",
    "is_wet_end",
]

# contracts/packet-v1.md pins SOIL_TENSION_* at 0.1 kΩ/LSB, u16, saturating at 0xFFFE.
RESISTANCE_COUNTS_PER_KOHM = 10.0

# Irrometer's meter reads 0-239 cb and the sensor is not specified past it. Above this
# the curve is still monotonic but the number has left the range anyone calibrated, so
# it is reported as "at least this dry" rather than as a measurement.
TENSION_MAX_KPA = 239.0

# Below ~10 cb the Watermark's resolution collapses — the gypsum is near saturation and
# small resistance changes swamp the signal. The reading is directionally right and well
# below any irrigation trigger, so it is flagged rather than dropped: a dropped reading
# is indistinguishable downstream from a sensor that died.
WET_END_KPA = 10.0


@dataclass(frozen=True)
class WatermarkCoefficients:
    """The four constants of the SPEC §5.1 calibration curve.

    A parameter rather than four module constants because SPEC §5.1 deploys commercial
    Watermarks and homemade gypsum blocks as **matched pairs on one node**, and a
    homemade block does not follow Irrometer's curve. Two channels in one packet can
    need two curves. Passing them in also makes a bench re-fit a config change and a
    vector diff, which is what issue #20's fourth acceptance criterion asks for.
    """

    a: float  # intercept, kPa
    b: float  # kPa per kΩ
    c: float  # resistance term in the denominator, per kΩ
    d: float  # temperature term in the denominator, per °C


# SPEC §5.1: kPa = (4.093 + 3.213·R) / (1 - 0.009733·R - 0.01205·T)
IRROMETER_200SS = WatermarkCoefficients(a=4.093, b=3.213, c=0.009733, d=0.01205)


def resistance_kohm(raw: int) -> float:
    """Wire units to kΩ. The inverse of what the node's serializer wrote."""
    return raw / RESISTANCE_COUNTS_PER_KOHM


def tension_kpa(
    resistance_kohm: float,
    temp_c: float,
    coeffs: WatermarkCoefficients = IRROMETER_200SS,
) -> float | None:
    """Soil-water tension in kPa (== centibars), or None if the input is out of domain.

    Temperature compensation is not optional. SPEC §5.1 measures 20-30 % drift between
    morning and afternoon without it; at 5 kΩ the swing from 10 °C to 30 °C is 24.3 cb
    to 34.2 cb, which straddles the 25-30 cb irrigation trigger. The same soil would
    read "hold off" at dawn and "irrigate" after lunch.

    **The denominator has a pole, and guarding it is the whole reason this function can
    return None.** `1 - c·R - d·T` reaches zero near 78 kΩ at 20 °C; past it the
    expression returns large negative tensions that look like readings. The pole *moves
    with temperature* — about 103 kΩ in near-freezing soil, about 66 kΩ at 30 °C — so a
    fixed resistance ceiling cannot guard it and the check is on the denominator itself.
    """
    if resistance_kohm < 0:
        # Not a resistance. Distinct from zero, which is a legal dead short.
        return None

    denominator = 1.0 - coeffs.c * resistance_kohm - coeffs.d * temp_c
    if denominator <= 0.0:
        return None

    kpa = (coeffs.a + coeffs.b * resistance_kohm) / denominator

    if kpa < 0.0:
        # Unreachable with the Irrometer coefficients (a positive numerator over a
        # positive denominator), but a custom curve with a negative intercept could
        # get here, and a negative tension is not a dry soil — it is a bad curve.
        return None

    if kpa > TENSION_MAX_KPA:
        # Clamp rather than refuse. This is the guard that actually catches the
        # near-pole region: at 77 kΩ the denominator is still positive and the
        # expression computes ~26,300 cb, which the pole check happily passes.
        return TENSION_MAX_KPA

    return kpa


def is_wet_end(kpa: float | None) -> bool:
    """Whether a tension sits in the band where the sensor stops being quantitative.

    None is not wet — it is nothing. Returning False keeps a caller from treating a
    refused reading as a very wet one, which would be the worst possible confusion:
    "the soil is soaked" is the one answer that stops an irrigation.
    """
    if kpa is None:
        return False
    return kpa < WET_END_KPA
