"""The publish envelope — Phase 3.7 (issue #46).

Graded against `contracts/publish-v1.md`, which is the document Poop Deck's ingest will
be written from. Where a rule in that contract is load-bearing — the ISO-8601 timestamp,
the omit-don't-null convention, battery always present — there is a test whose name says
so, because the contract is the only thing standing between two repos that cannot import
each other.
"""
from __future__ import annotations

from soundings_gateway import publish
from soundings_gateway.config import GatewayConfig, NodeLocation, TankGeometry

GEOM = TankGeometry(
    measured=True, capacity_gal=1490.0, sensor_zero_mm=2100, max_height_mm=2000,
    breakpoint_mm=1150, gal_per_mm_below=1.0, gal_per_mm_above=0.4,
)
CFG = GatewayConfig(
    ref_temp_c=20.0,
    nodes={7: NodeLocation(7, "water/cluster", "tank", "Rain catchment cluster")},
    tanks={"water/cluster": GEOM},
)


def reading(**over):
    msg = {
        "node_id": 7,
        "seq": 42,
        "fw_version": 259,
        "battery_mv": 3810,
        "channels": [
            {"name": "TANK_DISTANCE", "bit": 8, "raw": 1347, "fault": False},
            {"name": "SOIL_TEMP_0", "bit": 4, "raw": 320, "fault": False},
        ],
        "received_at": 1_786_594_320.0,   # 2026-08-13T04:12:00Z (computed, not guessed —
                                          # the first value here was a year out and the
                                          # test caught it, which is the point of pinning
                                          # a literal rather than reformatting time.time())
    }
    msg.update(over)
    return msg


# ---- Timestamp --------------------------------------------------------------

# Poop Deck's only implemented producer sends ISO-8601 UTC strings and lets Postgres
# parse them into TIMESTAMPTZ (their tinkle_publish.ino formats %Y-%m-%dT%H:%M:%SZ).
# An epoch float would land in a TIMESTAMPTZ column as an error, or worse, as 1970.
def test_timestamp_is_iso_8601_utc_with_a_z():
    env = publish.envelope(reading(), CFG)
    assert env["ts"] == "2026-08-13T04:12:00Z"


def test_timestamp_has_no_offset_notation():
    # "+00:00" is valid ISO-8601 and is NOT what the worked example produces. Matching
    # the existing producer costs nothing and removes a whole class of parser surprise.
    assert "+00:00" not in publish.envelope(reading(), CFG)["ts"]
    assert publish.envelope(reading(), CFG)["ts"].endswith("Z")


def test_a_reading_with_no_receipt_time_still_gets_one():
    # The gateway owns the timestamp because field nodes have no RTC. A reading that
    # somehow arrives without one must not publish a document with no time at all.
    env = publish.envelope(reading(received_at=None), CFG)
    assert env["ts"].endswith("Z")


# ---- Required fields --------------------------------------------------------

def test_the_envelope_carries_every_required_contract_field():
    env = publish.envelope(reading(), CFG)
    for key in ("v", "node_id", "seq", "ts", "fw_version", "battery_mv", "channels", "derived"):
        assert key in env, key


def test_schema_version_is_one():
    assert publish.envelope(reading(), CFG)["v"] == 1


def test_identity_and_sequence_come_through_unchanged():
    env = publish.envelope(reading(node_id=7, seq=65535), CFG)
    assert env["node_id"] == 7
    assert env["seq"] == 65535


# A node whose every sensor faulted is still worth recording as alive, so battery is not
# conditional on anything.
def test_battery_is_present_even_when_every_channel_faulted():
    faulted = [
        {"name": "TANK_DISTANCE", "bit": 8, "raw": 0, "fault": True},
        {"name": "SOIL_TEMP_0", "bit": 4, "raw": 0, "fault": True},
    ]
    env = publish.envelope(reading(channels=faulted), CFG)
    assert env["battery_mv"] == 3810


# ---- Raw channels -----------------------------------------------------------

def test_raw_channels_ride_through_with_their_fault_flags():
    env = publish.envelope(reading(), CFG)
    by_bit = {c["bit"]: c for c in env["channels"]}
    assert by_bit[8]["raw"] == 1347
    assert by_bit[8]["fault"] is False
    assert by_bit[8]["name"] == "TANK_DISTANCE"


# A faulted channel keeps its place in the array — dropping it would make "broken"
# indistinguishable from "not fitted" downstream, which is the whole of DEC-002.
def test_a_faulted_channel_is_still_in_the_array():
    faulted = [{"name": "TANK_DISTANCE", "bit": 8, "raw": 0, "fault": True}]
    env = publish.envelope(reading(channels=faulted), CFG)
    assert len(env["channels"]) == 1
    assert env["channels"][0]["fault"] is True


# ---- Location enrichment ----------------------------------------------------

def test_location_is_enriched_from_the_node_map():
    assert publish.envelope(reading(), CFG)["location"] == "water/cluster"


def test_an_unmapped_node_publishes_without_a_location_rather_than_a_guess():
    env = publish.envelope(reading(node_id=99), CFG)
    assert "location" not in env
    # …but the reading itself still goes out. An unmapped node is a provisioning gap,
    # not a reason to throw the measurement away.
    assert env["node_id"] == 99
    assert env["channels"]


# ---- Derived ----------------------------------------------------------------

def test_derived_values_land_under_their_own_key():
    env = publish.envelope(reading(), CFG)
    assert env["derived"]["distance_mm"] == 1347.0
    assert env["derived"]["headspace_temp_c"] == 20.0


def test_derived_is_present_but_empty_when_nothing_could_be_derived():
    # A consumer should be able to read env["derived"] unconditionally.
    env = publish.envelope(reading(node_id=99), CFG)
    assert env["derived"] == {}


# The omit-don't-null rule. A null reads as "we measured nothing"; a zero reads as a
# real measurement of zero. Absence is the only encoding that says "not derivable".
def test_undrivable_values_are_omitted_not_nulled_or_zeroed():
    unmeasured = GatewayConfig(
        ref_temp_c=20.0,
        nodes={7: NodeLocation(7, "water/cluster", "tank")},
        tanks={"water/cluster": TankGeometry(measured=False)},
    )
    env = publish.envelope(reading(), unmeasured)
    assert "level_gal" not in env["derived"]
    assert "percent" not in env["derived"]
    assert env["derived"]["distance_mm"] == 1347.0     # what IS derivable still lands


def test_gallons_appear_once_the_geometry_is_measured():
    env = publish.envelope(reading(), CFG)
    # height = 2100 - 1347 = 753 mm at the reference temperature → 753 gal
    assert env["derived"]["level_gal"] == 753.0


# ---- Robustness -------------------------------------------------------------

def test_the_envelope_is_json_serializable():
    import json
    env = publish.envelope(reading(), CFG)
    # Asserted before dumping: json.dumps(None) succeeds quietly, so the original
    # form passed against `envelope() { return None }` — confirmed by mutation.
    assert env is not None
    json.dumps(env)   # raises if not


def test_a_malformed_reading_does_not_raise():
    # This runs in the publish path of a long-lived daemon.
    assert publish.envelope({}, CFG) is None
    # Paired positive control: a well-formed reading still produces an envelope, so
    # this pins the malformed input as the cause rather than a function that refuses
    # everything.
    assert publish.envelope(reading(), CFG) is not None


# seq is half the natural key (node_id, seq). A document with a null seq publishes fine
# here and can never be stored idempotently there — and if their column is NOT NULL, as
# their irrigation natural-key index pattern suggests, it cannot be stored at all. The
# contract calls seq required, so the code has to actually refuse it.
def test_a_reading_with_no_seq_is_refused_because_it_cannot_be_keyed():
    assert publish.envelope({"node_id": 7, "battery_mv": 3800}, CFG) is None
    # The same reading WITH a seq is accepted — otherwise "refused because it cannot
    # be keyed" is asserted of a function that refuses regardless.
    assert publish.envelope({"node_id": 7, "seq": 1, "battery_mv": 3800}, CFG) is not None


def test_seq_zero_is_a_real_sequence_number_not_a_missing_one():
    # The first packet after a power cycle is seq 0. A truthiness check here would drop
    # exactly the reading that marks a node restart.
    env = publish.envelope(reading(seq=0), CFG)
    assert env is not None
    assert env["seq"] == 0


# fw_version is informational — the contract's own notes say it is never a drop gate — so
# a reading missing it still publishes. Marking it "required" was the contract
# disagreeing with itself, and the code is the honest half.
def test_a_reading_with_no_fw_version_still_publishes():
    env = publish.envelope(reading(fw_version=None), CFG)
    assert env is not None
    assert env["fw_version"] is None


def test_a_reading_with_no_channels_is_survivable():
    # A node that declared nothing is still worth recording as alive.
    env = publish.envelope(reading(channels=[]), CFG)
    assert env is not None
    assert env["channels"] == []
    assert env["battery_mv"] == 3810


# ---- Credentials ------------------------------------------------------------

def test_broker_config_never_prints_its_password():
    # Nothing logs the config today. But GatewayConfig carries the broker and is passed
    # into envelope() and derive_tank(), so one future log.debug("%r", cfg) — or a
    # traceback that formats it — would put a broker password in a log file.
    from soundings_gateway.config import BrokerConfig
    text = repr(BrokerConfig(host="mqtt.local", username="soundings", password="hunter2"))
    assert "hunter2" not in text
    assert "mqtt.local" in text     # the useful half is still debuggable
