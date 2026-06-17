"""End-to-end spine wiring test, broker-free: emitter → gateway decode → publish
→ ingest → line protocol. Injected fakes stand in for MQTT and VictoriaMetrics;
the live broker + DB hop is covered by running spine.py against the docker stack.
"""
from __future__ import annotations

from soundings_gateway import packet
from soundings_gateway.emitter import FleetEmitter, NodeSpec
from soundings_gateway.gateway import Gateway
from soundings_gateway.ingest import Ingest, reading_to_line
from soundings_gateway.source import FakePacketSource


def _soil_tension(line: str) -> int:
    # line: "soundings,node=1 soil_tension_0=NNN,soil_temp_0=...,... <ts>"
    fields = line.split(" ")[1]
    for kv in fields.split(","):
        k, v = kv.split("=")
        if k == "soil_tension_0":
            return int(v)
    raise AssertionError("no soil_tension_0 in line")


def test_full_pipeline_drydown_reaches_db():
    fleet = FleetEmitter(specs=[NodeSpec(1)], cadence_min=30.0, max_minutes=3000.0, seed=3)

    published: list[dict] = []
    decoded = Gateway(fleet, published.append).run()
    assert decoded == len(published) > 0

    lines: list[str] = []
    sink = Ingest(lines.append)
    for msg in published:
        sink.handle(msg)
    assert sink.written == decoded

    # The drydown made it all the way through: tension rises from first to last point.
    tensions = [_soil_tension(line) for line in lines]
    assert tensions[0] < tensions[-1]
    # Lines are well-formed for VictoriaMetrics' Influx ingestion.
    assert all(line.startswith("soundings,node=1 ") for line in lines)
    assert all("battery_mv=" in line for line in lines)


def test_received_at_becomes_ns_timestamp():
    msg = packet.decode(NodeSpec(5).packet_at(0, 1)).to_dict()
    msg["received_at"] = 1_700_000_000.0
    line = reading_to_line(msg)
    assert line.endswith(" 1700000000000000000")  # seconds -> ns


def test_faulted_channel_not_written():
    # Hand-build a reading with a faulted channel; its value must not be emitted.
    raw = packet.encode(
        node_id=9, fw_version=100, seq=1, battery_mv=3800,
        channels={0: 150, 4: 296}, fault_bits={4},  # SOIL_TEMP_0 faulted
    )
    msg = packet.decode(raw).to_dict()
    line = reading_to_line(msg)
    assert "soil_tension_0=150" in line
    assert "soil_temp_0=" not in line  # faulted channel omitted (DEC-002)


def test_gateway_counts_dropped_frames():
    good = NodeSpec(1).packet_at(0, 1)
    bad = b"\x01\x02\x03"  # too short — decode() drops it
    gw = Gateway(FakePacketSource([good, bad, good]), lambda _m: None)
    assert gw.run() == 2
    assert gw.dropped == 1
