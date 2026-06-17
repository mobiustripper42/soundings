"""Synthetic fleet tests: the drydown model, the jitter window, and that emitted
frames are real packets the gateway can decode."""
from __future__ import annotations

from soundings_gateway import packet
from soundings_gateway.emitter import FleetEmitter, NodeSpec, soil_tension_raw


def test_drydown_rises_and_stays_in_range():
    wet, dry, tau = 120, 2000, 2880.0
    early = soil_tension_raw(wet, dry, tau, 0)
    mid = soil_tension_raw(wet, dry, tau, tau)        # ~63% of the way
    late = soil_tension_raw(wet, dry, tau, 5 * tau)   # nearly dry
    assert early < mid < late
    assert abs(early - wet) < 0.05 * (dry - wet)      # starts near wet
    assert late <= dry
    assert 0 <= early <= 0xFFFF and 0 <= late <= 0xFFFF


def test_jitter_window_bounded_and_separates_nodes():
    fleet = FleetEmitter(
        specs=[NodeSpec(1), NodeSpec(2), NodeSpec(3)],
        cadence_min=12.0,
        max_minutes=120.0,
        jitter_s=30.0,
        seed=7,
    )
    sched = fleet.schedule()
    # Every transmit's jitter is within ±30 s.
    assert all(-30.0 <= jitter <= 30.0 for _nominal, _node, jitter in sched)

    # On a shared nominal tick the three nodes land at distinct (jittered) times —
    # the collision-avoidance the jitter exists for (SPEC §10).
    first_tick = min(n for n, _i, _j in sched)
    same_tick = [(node, jitter) for n, node, jitter in sched if n == first_tick]
    actual_times = [j for _node, j in same_tick]
    assert len(actual_times) == 3
    assert len(set(actual_times)) == 3  # no exact collision


def test_emitted_frames_decode_back():
    fleet = FleetEmitter(specs=[NodeSpec(1), NodeSpec(2)], cadence_min=30.0, max_minutes=300.0)
    frames = list(fleet)
    assert frames
    node_ids = set()
    for raw in frames:
        r = packet.decode(raw)
        assert r is not None                       # every emitted frame is valid
        assert r.channel("SOIL_TENSION_0") is not None
        node_ids.add(r.node_id)
    assert node_ids == {1, 2}


def test_event_count_matches_schedule():
    fleet = FleetEmitter(specs=[NodeSpec(1), NodeSpec(2)], cadence_min=10.0, max_minutes=100.0)
    # 10 ticks * 2 nodes
    assert len(fleet.events()) == 20
    # events are sorted by actual transmit time
    times = [e.sim_min for e in fleet.events()]
    assert times == sorted(times)
