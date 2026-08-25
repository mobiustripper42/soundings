"""Downlink v1, daemon side — `contracts/downlink-v1.md`.

The node's decoder is an independent C++ implementation
(`firmware/test/test_downlink/test_downlink.cpp`). There is no shared vector file,
for the same reason serial framing has none — but the two sides do assert against
**the same literal frame**, written out below, so a divergence in field order,
offset or endianness shows up as two failing tests rather than a silent
disagreement discovered at a bench.

The CRC itself cannot drift: both ends call the packet-v1 implementation, which
`contracts/vectors/packet-v1.json` already pins.
"""
from __future__ import annotations

import pytest

from soundings_gateway import downlink
from soundings_gateway.downlink import SerialDownlinkSink
from soundings_gateway.framing import MIN_PAYLOAD, OVERHEAD, SerialFramer
from soundings_gateway.packet import crc16_ccitt_false

# The shared literal. Node 7, flags 0x1234. The identical bytes are asserted in
# test_downlink.cpp — if you change one, the other must fail.
SHARED_VECTOR = bytes.fromhex("01073412c68c")


def test_encode_matches_the_shared_literal():
    assert downlink.encode(7, 0x1234) == SHARED_VECTOR


def test_the_shared_literal_is_what_the_contract_describes():
    """Spelled out field by field, so the literal above is checkable by reading
    rather than by trusting the encoder that produced it."""
    assert SHARED_VECTOR[0] == downlink.PROTO_V1
    assert SHARED_VECTOR[1] == 7
    assert SHARED_VECTOR[2] == 0x34 and SHARED_VECTOR[3] == 0x12   # little-endian flags
    crc = crc16_ccitt_false(SHARED_VECTOR[:4])
    assert SHARED_VECTOR[4] == crc & 0xFF and SHARED_VECTOR[5] == crc >> 8
    assert len(SHARED_VECTOR) == downlink.DOWNLINK_LEN


def test_a_downlink_with_no_flags_is_the_ordinary_case():
    frame = downlink.encode(3)
    assert len(frame) == downlink.DOWNLINK_LEN
    assert frame[2] == 0 and frame[3] == 0
    # ...and a different node_id produces a different frame, so this is not passing
    # against an encoder that returns a constant.
    assert downlink.encode(4) != frame


def test_flags_are_little_endian():
    assert downlink.encode(1, 0xBEEF)[2:4] == b"\xef\xbe"


@pytest.mark.parametrize("node_id", [-1, 256, 1000])
def test_a_node_id_that_does_not_fit_raises(node_id):
    with pytest.raises(ValueError):
        downlink.encode(node_id)
    # The adjacent legal value is accepted — otherwise this passes against an
    # encoder that raises on everything.
    assert len(downlink.encode(255)) == downlink.DOWNLINK_LEN


@pytest.mark.parametrize("flags", [-1, 0x10000])
def test_flags_that_do_not_fit_raise(flags):
    with pytest.raises(ValueError):
        downlink.encode(1, flags)
    assert len(downlink.encode(1, 0xFFFF)) == downlink.DOWNLINK_LEN


def test_node_zero_is_addressable():
    """0 is a legal address, not an unset field."""
    assert downlink.encode(0)[1] == 0


def test_a_downlink_survives_the_serial_envelope():
    """It travels daemon -> serial -> board, so serial framing v1 carries it in the
    reverse direction. Its minimum payload dropped from 14 to 6 for exactly this."""
    assert downlink.DOWNLINK_LEN == MIN_PAYLOAD

    raw = downlink.encode(7, 0x1234)
    wire = b"\xa5\x5a" + bytes([len(raw)]) + raw
    assert len(wire) == downlink.DOWNLINK_LEN + OVERHEAD

    assert list(SerialFramer().feed(wire)) == [raw]


def test_a_six_byte_payload_was_rejected_before_the_amendment():
    """The floor really did move. A framer with the old minimum of 14 would drop
    this; pinning the boundary keeps the amendment honest rather than assumed."""
    assert MIN_PAYLOAD == 6
    raw = downlink.encode(7)
    assert list(SerialFramer().feed(b"\xa5\x5a" + bytes([len(raw)]) + raw)) == [raw]
    # One byte below the new floor is still refused.
    assert list(SerialFramer().feed(b"\xa5\x5a" + bytes([5]) + b"\x00" * 5)) == []


# ---- SerialDownlinkSink ----------------------------------------------------
#
# The write half of the USB link. Until now the daemon could encode a downlink and
# had nowhere to put it: `framing.py` decoded only, and `Gateway.run()` never wrote.


class ScriptedWriter:
    """A serial port that records what was written to it.

    `fail_after` makes write() raise from that call onward — a port that went away
    mid-run (cable pulled, board reset, /dev node vanished), which is the failure the
    daemon has to survive rather than die of.
    """

    def __init__(self, fail_after: int | None = None):
        self.written: list[bytes] = []
        self.fail_after = fail_after

    def write(self, data: bytes) -> int:
        if self.fail_after is not None and len(self.written) >= self.fail_after:
            raise OSError("port went away")
        self.written.append(bytes(data))
        return len(data)


def test_sink_writes_a_framed_downlink():
    port = ScriptedWriter()
    assert SerialDownlinkSink(port).send(7, 0x1234) is True
    # Framed, not raw — the board's reader is looking for the envelope.
    assert port.written == [b"\xa5\x5a\x06" + SHARED_VECTOR]


def test_what_the_sink_writes_is_what_the_board_reads_back():
    """Round trip through the framer, so this is graded against the decode half
    rather than against a literal this test built itself."""
    port = ScriptedWriter()
    SerialDownlinkSink(port).send(7, 0x1234)
    assert list(SerialFramer().feed(port.written[0])) == [SHARED_VECTOR]


def test_sink_survives_a_port_that_went_away():
    """The daemon must not die because the cable was pulled. It keeps decoding
    packets; it just cannot answer them."""
    port = ScriptedWriter(fail_after=0)
    sink = SerialDownlinkSink(port)
    assert sink.send(7) is False
    assert sink.failed == 1
    # Positive control, in the same test: a healthy port DOES return True and
    # increments `sent`. Without this the test passes against a sink that always
    # fails, which is exactly the shape the Task 3 audit found nineteen of.
    ok = SerialDownlinkSink(ScriptedWriter())
    assert ok.send(7) is True
    assert ok.sent == 1 and ok.failed == 0


def test_sink_refuses_a_node_id_that_does_not_fit():
    """The encoder raises on an out-of-range node_id and the sink does not swallow
    it. A misaddressed downlink is silently ignored by every node, so it would look
    exactly like a quiet window — unfalsifiable from this end."""
    port = ScriptedWriter()
    sink = SerialDownlinkSink(port)
    with pytest.raises(ValueError):
        sink.send(999)
    assert port.written == []
    # The adjacent legal address is accepted and does reach the port.
    assert sink.send(255) is True
    assert len(port.written) == 1


def test_sink_counts_across_many_sends():
    port = ScriptedWriter(fail_after=2)
    sink = SerialDownlinkSink(port)
    results = [sink.send(n) for n in range(4)]
    assert results == [True, True, False, False]
    assert sink.sent == 2 and sink.failed == 2
