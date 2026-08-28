#!/usr/bin/env python3
"""Golden-vector generator for the Watermark tension curve (issue #20, SPEC §5.1).

FIXTURE AUTHORING ONLY — this is NOT the gateway implementation (that is
gateway/soundings_gateway/tension.py, written against the spec, and graded against the
JSON this emits). Keeping the generator standalone preserves the same
two-independent-implementations property the packet vectors have: the equation is
transcribed here directly from SPEC §5.1, and tension.py is checked against the result
rather than against this code.

Run from the repo root:  python3 contracts/tools/gen_tension_vectors.py
It rewrites contracts/vectors/tension-v1.json in place. Re-run after any coefficient
change — which is the point. A bench re-calibration then lands as a **diff on the
expected outputs**, reviewable line by line, rather than as a number someone changed
and a suite that stayed green.

The vectors cluster in the operator's own decision band — irrigate at 25-30 cb, stress
at 50-60 cb, recovered from a 2018 field season — rather than spreading evenly across
the 0-239 cb scale, because that band is where a wrong number changes what you do.
"""
import json
from pathlib import Path

# SPEC §5.1, transcribed:
#   kPa = (4.093 + 3.213·R) / (1 - 0.009733·R - 0.01205·T)     R in kΩ, T in °C
A = 4.093
B = 3.213
C = 0.009733
D = 0.01205

# Irrometer's meter reads 0-239 cb; past it the sensor is uncalibrated.
MAX_KPA = 239.0


def kpa(r_kohm, t_c):
    """The spec equation plus its two domain guards. Returns None where the expression
    has no physical meaning: a negative resistance, or a denominator at/past its pole."""
    if r_kohm < 0:
        return None
    den = 1.0 - C * r_kohm - D * t_c
    if den <= 0.0:
        return None
    value = (A + B * r_kohm) / den
    if value < 0.0:
        return None
    return min(value, MAX_KPA)


# (name, kind, resistance_kΩ, temp_°C, why)
CASES = [
    ("saturated_cold", "wet", 0.2, 5.0,
     "Freshly watered, cold spring soil. Well inside the wet end where the sensor "
     "stops being quantitative."),
    ("wet_end_boundary", "wet", 1.0, 20.0,
     "9.75 cb — just under the 10 cb floor below which resolution collapses."),

    ("trigger_low_cool", "trigger", 5.0, 10.0,
     "24.3 cb at dawn. The SAME resistance as trigger_low_warm — only soil temp moved."),
    ("trigger_low", "trigger", 5.0, 20.0,
     "28.4 cb — dead centre of the 25-30 cb irrigation trigger."),
    ("trigger_low_warm", "trigger", 5.0, 30.0,
     "34.2 cb in afternoon soil. Uncompensated this reads as 'irrigate' while "
     "trigger_low_cool reads 'hold off', on one unchanged sensor."),
    ("trigger_high", "trigger", 6.0, 20.0,
     "33.0 cb — the dry edge of the trigger band."),

    ("stress_low", "stress", 10.0, 20.0,
     "54.7 cb — the middle of the 50-60 cb stress threshold."),
    ("stress_high", "stress", 11.0, 20.0,
     "60.5 cb — the top of the stress band. Past here the crop is paying for it."),
    ("stress_cold_soil", "stress", 10.0, 2.0,
     "The same 10 kΩ in near-freezing soil reads lower — the compensation is signed."),

    ("very_dry", "dry", 20.0, 20.0,
     "121 cb. Well past any irrigation decision, still on the scale."),
    ("scale_ceiling", "dry", 32.0, 20.0,
     "238.9 cb — the last value under Irrometer's 239 cb ceiling."),
    ("above_scale_clamps", "dry", 35.0, 20.0,
     "278.6 cb computed, clamped to 239. Reported as 'at least this dry'."),
    ("near_pole_clamps", "dry", 77.0, 20.0,
     "~26,300 cb computed — the denominator is still positive, so the pole guard "
     "passes it. The clamp is what makes this region safe."),

    ("dead_short", "edge", 0.0, 20.0,
     "5.39 cb. Zero ohms is a legal reading — a shorted block — not an error."),

    ("pole_at_twenty_c", "refused", 78.0, 20.0,
     "Denominator -0.000174. The pole sits at 77.98 kΩ when T=20."),
    ("pole_moves_warm", "refused", 70.0, 30.0,
     "Fine at 0 °C, past the pole at 30 °C: the pole moves from 102.7 kΩ to 65.6 kΩ "
     "across the soil-temperature range. A fixed resistance ceiling cannot guard it."),
    ("in_domain_when_cold", "dry", 70.0, 0.0,
     "The positive control for pole_moves_warm — same resistance, cold soil, valid."),
    ("negative_resistance", "refused", -1.0, 20.0,
     "Not a resistance at all. Distinct from the legal zero above."),
]


def main():
    vectors = []
    for name, kind, r, t, why in CASES:
        value = kpa(r, t)
        vectors.append({
            "name": name,
            "kind": kind,
            "description": why,
            "resistance_kohm": r,
            "resistance_raw": None if r < 0 else round(r * 10),
            "temp_c": t,
            "expected_kpa": None if value is None else round(value, 4),
        })

    out = {
        "format": "soundings-tension",
        "curve": "Irrometer Watermark 200SS, SPEC §5.1",
        "equation": "kPa = (a + b*R) / (1 - c*R - d*T), R in kOhm, T in degC",
        "coefficients": {"a": A, "b": B, "c": C, "d": D},
        "guards": {
            "max_kpa": MAX_KPA,
            "refuse_when": "resistance < 0, or denominator <= 0 (the pole), or kPa < 0",
            "pole_kohm_at_0c": round((1.0 - D * 0.0) / C, 3),
            "pole_kohm_at_20c": round((1.0 - D * 20.0) / C, 3),
            "pole_kohm_at_30c": round((1.0 - D * 30.0) / C, 3),
        },
        "decision_bands_kpa": {
            "wet_end_below": 10.0,
            "irrigate": [25.0, 30.0],
            "stress": [50.0, 60.0],
        },
        "vectors": vectors,
    }
    dest = Path(__file__).resolve().parent.parent / "vectors" / "tension-v1.json"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(out, indent=2) + "\n")
    print(f"wrote {len(vectors)} vectors -> {dest}")
    for v in vectors:
        exp = "refused" if v["expected_kpa"] is None else f"{v['expected_kpa']:8.3f} kPa"
        print(f"  {v['name']:22s} {v['resistance_kohm']:7.1f} kOhm "
              f"{v['temp_c']:5.1f} C  {exp}")


if __name__ == "__main__":
    main()
