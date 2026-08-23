"""Config loading — the node→location map as a file (D7).

The shipped `gateway/config/soundings.toml` is loaded here rather than a fixture copy,
so the file the gateway actually reads is the file under test. A placeholder that
drifts out of parseability is then a red suite, not a surprise at the tank.
"""
from __future__ import annotations

from pathlib import Path

import pytest

from soundings_gateway.config import load_config

SHIPPED = Path(__file__).resolve().parents[1] / "config" / "soundings.toml"


def test_the_shipped_config_parses():
    cfg = load_config(SHIPPED)
    assert cfg.ref_temp_c == 20.0


def test_the_shipped_config_maps_the_tank_node_to_the_cluster():
    cfg = load_config(SHIPPED)
    assert cfg.nodes[1].location == "water/cluster"
    assert cfg.nodes[1].role == "tank"


# The guard that keeps a fabricated volume off the chart. If this ever fails because
# someone set measured = true, the tape-measure numbers had better be in the same diff.
def test_the_shipped_geometry_is_still_marked_unmeasured():
    cfg = load_config(SHIPPED)
    assert cfg.tanks["water/cluster"].measured is False


def test_capacity_is_not_the_sum_of_the_three_nameplates():
    """2530 gal (2×1100 + 330) is the wrong denominator for percent-of-full, and it is
    wrong in the direction that hides: the tanks share one level and cannot all sit at
    their own maximum at once, so at the level where the IBC is full each cylinder holds
    well under 1100. Seeding 2530 would make every percent reading quietly low against a
    number that looks obviously correct. It stays 0 until the max shared level is
    measured (docs/tank-level-sensor.md § What the seed curve still needs)."""
    cfg = load_config(SHIPPED)
    assert cfg.tanks["water/cluster"].capacity_gal != 2530.0
    assert cfg.tanks["water/cluster"].capacity_gal == 0.0


def test_node_ids_load_as_integers_not_strings():
    # TOML table keys are strings; a node id that stays a string never matches the
    # integer node_id on a decoded reading, and every derivation silently returns [].
    cfg = load_config(SHIPPED)
    assert all(isinstance(k, int) for k in cfg.nodes)


def test_a_missing_config_file_raises_rather_than_returning_empty():
    # An empty config would look exactly like "no nodes are mapped yet" and derive
    # nothing, forever, silently.
    with pytest.raises(FileNotFoundError):
        load_config(SHIPPED.parent / "does-not-exist.toml")


def test_geometry_defaults_are_safe_when_a_key_is_absent(tmp_path):
    p = tmp_path / "partial.toml"
    p.write_text('[nodes.1]\nlocation = "water/cluster"\nrole = "tank"\n'
                 '[tanks."water/cluster"]\ncapacity_gal = 2530.0\n')
    cfg = load_config(p)
    # measured defaults FALSE — an absent flag must never read as "go ahead".
    assert cfg.tanks["water/cluster"].measured is False
    assert cfg.tanks["water/cluster"].breakpoint_mm == 0
    # The field that WAS supplied has to round-trip, or every assertion above is
    # satisfied by a parser that discards `entry` and returns bare defaults. The
    # shipped config has every geometry field at its zero default (the tanks are not
    # measured yet), so nothing else in this suite would notice.
    assert cfg.tanks["water/cluster"].capacity_gal == 2530.0
