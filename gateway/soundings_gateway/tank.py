"""Tank derivation — raw sensor counts into a water level, in Python (DEC-004).

None of this runs on the node. The volume curve and the speed-of-sound correction are
gateway-side so they can be re-derived against years of stored raw readings without
reflashing a node sealed into a tank lid — which, per DEC-007's 2026-08-08 amendment,
is not a tidiness argument but a load-bearing one: the stratification bias is signed
and seasonal, and the model that removes it will be fitted from history that does not
exist yet.

Pure functions, no I/O, no config reading. The gating decisions — whether a reading is
trustworthy enough to derive a volume from at all — live in derive.py.
"""
from __future__ import annotations

__all__ = [
    "speed_of_sound_mps",
    "correct_distance_mm",
    "water_height_mm",
    "gallons_from_height",
    "percent_full",
    "SENSOR_MIN_MM",
    "SENSOR_MAX_MM",
]

# A02YYUW published range: 3–450 cm (docs/tank-level-sensor.md, blind zone corrected
# 2026-08-08 from an earlier 20–25 cm claim). Outside this the number is not a short or
# long distance — it is the sensor reporting that it got no usable return.
SENSOR_MIN_MM = 30
SENSOR_MAX_MM = 4500


def speed_of_sound_mps(temp_c: float) -> float:
    """c(T) = 331.3 + 0.606·T, the linear approximation DEC-007 specifies.

    Not clamped at either end: a below-freezing headspace is a real winter condition
    and the formula is still the best available estimate there.
    """
    return 331.3 + 0.606 * temp_c


def correct_distance_mm(raw_mm: int, temp_c: float, ref_temp_c: float) -> float:
    """The sensor reports distance as if the air were at its calibration temperature.

    Sound moves ~0.176 %/°C, so a fixed flight time covers more ground in hot air: a hot
    headspace makes the true distance LARGER than the reported one. Over a 2 m path,
    0 °C → 40 °C is ~14 cm of apparent level change with no water moving, against a
    sensor specified to ±1 cm — the dominant error term by an order of magnitude, which
    is the whole of DEC-007.
    """
    return raw_mm * speed_of_sound_mps(temp_c) / speed_of_sound_mps(ref_temp_c)


def water_height_mm(corrected_mm: float, sensor_zero_mm: int, max_height_mm: int) -> float:
    """Water column height, clamped to the physically possible.

    `sensor_zero_mm` is the reading from a bone-dry tank, so height is what the water
    has taken away from it. Both clamps exist because noise at the extremes is not new
    information: a reading past the zero point is an empty tank plus jitter, not a
    negative volume, and a tank filled past its overflow is full rather than 105 % full.
    """
    height = sensor_zero_mm - corrected_mm
    if height < 0:
        return 0.0
    if height > max_height_mm:
        return float(max_height_mm)
    return height


def gallons_from_height(height_mm: float, geom) -> float:
    """Two linear segments, one breakpoint (docs/tank-level-sensor.md § Volume math).

    The three tanks are plumbed at the bottom and share a level. Below the IBC's top all
    three rise together; above it only the two cylinders do, so the same millimetre is
    worth fewer gallons. Continuous by construction — the upper segment starts from the
    volume at the breakpoint rather than from its own intercept.
    """
    if height_mm <= geom.breakpoint_mm:
        return height_mm * geom.gal_per_mm_below
    at_breakpoint = geom.breakpoint_mm * geom.gal_per_mm_below
    return at_breakpoint + (height_mm - geom.breakpoint_mm) * geom.gal_per_mm_above


def percent_full(gallons: float, capacity_gal: float) -> float:
    """Percent of capacity, clamped to 0–100.

    A zero capacity means an unmeasured tank, not a division to attempt — and returning
    0 rather than raising keeps a config gap from taking the ingest loop down.
    """
    if capacity_gal <= 0:
        return 0.0
    pct = gallons / capacity_gal * 100.0
    if pct < 0:
        return 0.0
    if pct > 100:
        return 100.0
    return pct
