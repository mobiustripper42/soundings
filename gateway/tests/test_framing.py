"""Serial framing v1, Python end — `contracts/serial-framing-v1.md`.

Vectors are literal byte strings rather than a shared fixture file, deliberately:
the framing is one-way and fixed-shape, so there is no round trip for a vector to
pin, and a framing bug means nothing decodes at all rather than a plausible wrong
number. The contract doc says so and says why.

The stream these tests describe is the real one: ESP32 boot chatter at every board
reset, a reset landing mid-frame, and a daemon restart opening the port partway
through somebody else's packet.
"""
from __future__ import annotations

import pytest

from soundings_gateway.framing import (
    MAX_PAYLOAD,
    MIN_PAYLOAD,
    OVERHEAD,
    SYNC,
    SerialFramer,
    encode,
)
from soundings_gateway.gateway import Gateway
from soundings_gateway.source import SerialPacketSource


def framed(payload: bytes) -> bytes:
    return SYNC + bytes([len(payload)]) + payload


def payload(n: int, fill: int = 0x11) -> bytes:
    """A payload of `n` bytes. Not a valid packet — framing does not care, and a
    test that used real packets here would be testing two contracts at once."""
    return bytes([fill]) * n


# Boot chatter is the reason resync exists at all. Real text, not random bytes:
# it contains no sync pair, which is the ordinary case.
BOOT_CHATTER = b"ESP-ROM:esp32s3-20210327\r\nBuild:Mar 27 2021\r\nrst:0x1 (POWERON)\r\n"

# Sync (2) + length (1) + the largest legal payload (46). The contract says a reader
# following its rules never holds more than this, which is what bounds the buffer
# without an explicit cap.
OVERHEAD_CEILING = 48


# ---- SerialFramer ----------------------------------------------------------

def test_single_frame_yields_its_payload():
    f = SerialFramer()
    assert list(f.feed(framed(payload(20)))) == [payload(20)]


def test_back_to_back_frames_both_arrive():
    f = SerialFramer()
    out = list(f.feed(framed(payload(14, 0xAA)) + framed(payload(46, 0xBB))))
    assert out == [payload(14, 0xAA), payload(46, 0xBB)]


def test_leading_boot_chatter_is_discarded():
    f = SerialFramer()
    assert list(f.feed(BOOT_CHATTER + framed(payload(16)))) == [payload(16)]
    assert f.discarded == len(BOOT_CHATTER)


def test_frame_split_across_reads_is_reassembled():
    f = SerialFramer()
    whole = framed(payload(30))
    # Split inside the payload — the case a chunked read produces constantly.
    assert list(f.feed(whole[:7])) == []
    assert list(f.feed(whole[7:])) == [payload(30)]


def test_sync_pair_split_across_reads_still_locks():
    """The trailing 0xA5 must be retained between feeds. Discarding it because no
    full sync was present would drop the frame behind it."""
    f = SerialFramer()
    whole = framed(payload(18))
    assert list(f.feed(whole[:1])) == []       # just the 0xA5
    assert list(f.feed(whole[1:])) == [payload(18)]


@pytest.mark.parametrize("bad_len", [0, 1, MIN_PAYLOAD - 1, MAX_PAYLOAD + 1, 200, 255])
def test_out_of_range_length_is_not_a_frame(bad_len):
    """A sync pair with an impossible length is boot text or payload data that
    happened to look like a header. Rejected, and the real frame behind it found."""
    f = SerialFramer()
    stream = SYNC + bytes([bad_len]) + b"\x00\x00\x00" + framed(payload(22))
    assert list(f.feed(stream)) == [payload(22)]


def test_false_sync_does_not_eat_the_frame_behind_it():
    """`A5 A5 5A ...` — discarding the whole pair on a failed length check would
    skip past the real sync. Step 3 of the contract says discard exactly one byte."""
    f = SerialFramer()
    assert list(f.feed(b"\xa5" + framed(payload(15)))) == [payload(15)]


def test_reset_mid_frame_absorbs_following_bytes_then_recovers():
    """Board resets with a frame half-sent: a truncated payload, then chatter, then
    normal service.

    The truncated frame's LEN byte was valid, so the reader keeps consuming until it
    has LEN bytes — meaning it swallows whatever arrives next and emits one garbage
    candidate. That is inherent to length-prefix framing and it is why the reader
    emits candidates rather than claiming to emit packets: the payload CRC rejects it
    downstream, on the path that already exists for radio-corrupted packets.

    The cost is bounded in BYTES — at most MAX_PAYLOAD of the following stream. See
    the test below for what that is worth in frames; it is not one."""
    f = SerialFramer()
    truncated = framed(payload(40))[:12]
    out = list(f.feed(truncated + BOOT_CHATTER + framed(payload(24, 0xCC))))

    assert len(out) == 2
    assert out[0] != payload(40)        # garbage: the 9 real bytes plus boot chatter
    assert out[1] == payload(24, 0xCC)  # and the next whole frame survives intact


def test_absorption_worst_case_is_five_frames_at_the_current_floor():
    """The worst case is arithmetic on the framer's own floor, and it moved in 3.9b.

    Smallest complete frame = OVERHEAD + MIN_PAYLOAD. At the original floor of 14 that
    was 17 bytes and floor(46 / 17) = 2. 3.9b lowered MIN_PAYLOAD to 6 so the reverse
    leg could carry a downlink, which makes the smallest frame 9 bytes and the worst
    case floor(46 / 9) = 5.

    ⚠ That is the FRAMER's bound, not the reachable one in a given direction. Board ->
    daemon carries only packet-v1, whose floor is still 14, so a real gap in tank level
    is capped at 2 frames there. Both numbers are true and they answer different
    questions; the contract states both.

    Pinned as a test because this bound has now been written down wrong twice — once as
    "at most one frame" and once as 2 after the floor moved underneath it."""
    f = SerialFramer()
    smallest = framed(payload(MIN_PAYLOAD, 0xAA))
    assert len(smallest) == OVERHEAD + MIN_PAYLOAD == 9

    fits = MAX_PAYLOAD // len(smallest)
    assert fits == 5

    window = smallest * fits + payload(MAX_PAYLOAD - fits * len(smallest), 0x00)
    assert len(window) == MAX_PAYLOAD

    out = list(f.feed(SYNC + bytes([MAX_PAYLOAD]) + window + framed(payload(20, 0xEE))))

    assert len(out) == 2
    assert out[0] == window                # all five swallowed whole
    assert out[1] == payload(20, 0xEE)     # and the reader is back in sync after one window


def test_daemon_restart_mid_stream_discards_the_partial_frame():
    """Opening the port partway through somebody else's packet."""
    f = SerialFramer()
    whole = framed(payload(28))
    out = list(f.feed(whole[9:] + framed(payload(19, 0xDD))))
    assert out == [payload(19, 0xDD)]


def test_sync_pattern_inside_a_payload_is_harmless():
    """0xA5 0x5A can occur in packet data. It is only hunted for between frames,
    so a payload containing it must round-trip untouched."""
    f = SerialFramer()
    tricky = b"\x01\x02" + SYNC + b"\x20" + payload(9) + b"\xa5\xa5"
    assert len(tricky) == 16
    assert list(f.feed(framed(tricky))) == [tricky]


def test_buffer_stays_bounded_under_pure_garbage():
    """A stream that never contains a frame must not accumulate. Without the
    discard-before-sync rule this grows without limit on a noisy or wrong port."""
    f = SerialFramer()
    for _ in range(200):
        assert list(f.feed(BOOT_CHATTER)) == []
    assert len(f._buf) <= OVERHEAD_CEILING


def test_incomplete_frame_is_held_not_dropped():
    """Held, because the rest may still be in flight — the sensor is 15 minutes
    away from saying anything again and a dropped tail is a lost cycle."""
    f = SerialFramer()
    whole = framed(payload(46))
    assert list(f.feed(whole[:-1])) == []
    assert list(f.feed(whole[-1:])) == [payload(46)]


# ---- encode ----------------------------------------------------------------
#
# The daemon could decode this envelope but never write one. 3.9b gave the reverse
# leg a payload type (downlink-v1) and a reader on the board, and left the daemon
# with no way to put bytes on it — `test_downlink.py` hand-rolled the three header
# bytes to prove a downlink survives the envelope. This is the missing half, and it
# is the mirror of `frameForSerial` in firmware/src/core/serial_framing.cpp.


def test_encode_wraps_a_payload_in_the_envelope():
    assert encode(payload(20)) == framed(payload(20))


def test_encode_round_trips_through_the_framer():
    """The two halves of the contract, checked against each other rather than
    against a literal either one of them produced."""
    for n in (MIN_PAYLOAD, 14, 30, MAX_PAYLOAD):
        assert list(SerialFramer().feed(encode(payload(n)))) == [payload(n)]


def test_encode_emits_exactly_three_bytes_of_overhead():
    assert len(encode(payload(MIN_PAYLOAD))) == MIN_PAYLOAD + OVERHEAD
    assert encode(payload(9))[:2] == SYNC
    assert encode(payload(9))[2] == 9


@pytest.mark.parametrize("bad_len", [0, 1, MIN_PAYLOAD - 1, MAX_PAYLOAD + 1, 255])
def test_encode_refuses_a_payload_outside_the_legal_range(bad_len):
    """Refuses rather than truncating, matching the C++ encoder: a partial frame
    desynchronises the reader for one frame instead of simply not existing, and the
    caller — which has a real message in hand — would have no way to tell."""
    with pytest.raises(ValueError):
        encode(payload(bad_len))
    # The adjacent legal lengths ARE accepted. Without these two lines this test
    # passes against an encoder that raises on everything, which is the exact false
    # green the Task 3 audit swept for and the one still being written by default.
    assert len(encode(payload(MIN_PAYLOAD))) == MIN_PAYLOAD + OVERHEAD
    assert len(encode(payload(MAX_PAYLOAD))) == MAX_PAYLOAD + OVERHEAD


def test_encode_does_not_escape_a_sync_pattern_in_the_payload():
    """There is no escaping in this contract — the length prefix is what delimits.
    A payload containing 0xA5 0x5A must go out byte-for-byte unchanged."""
    tricky = SYNC + payload(10) + b"\xa5"
    assert encode(tricky)[OVERHEAD:] == tricky


# ---- SerialPacketSource ----------------------------------------------------

class ScriptedStream:
    """A serial port that hands over scripted chunks and then reports EOF.

    Returns b"" for a scripted empty chunk — the read-timeout case, which must not
    end iteration — and raises EOFError once the script is exhausted, which must.
    """

    def __init__(self, chunks: list[bytes]):
        self.chunks = list(chunks)
        self.reads = 0

    def read(self, size: int) -> bytes:
        self.reads += 1
        if not self.chunks:
            raise EOFError
        return self.chunks.pop(0)


def test_source_yields_payloads_from_a_stream():
    src = SerialPacketSource(ScriptedStream([framed(payload(20)), framed(payload(21))]))
    assert list(src) == [payload(20), payload(21)]
    assert src.frames == 2


def test_source_treats_empty_read_as_nothing_right_now():
    """b"" is a read timeout on a live port, not end of stream. Ending iteration
    on it would stop the daemon the first quiet minute between packets."""
    stream = ScriptedStream([b"", b"", framed(payload(17)), b""])
    src = SerialPacketSource(stream)
    assert list(src) == [payload(17)]


def test_source_never_raises_on_malformed_input():
    """The daemon loop must survive anything the port produces. This is the whole
    reason the seam exists — a single bad read must not take the gateway down."""
    stream = ScriptedStream([BOOT_CHATTER, b"\xff" * 200, SYNC + b"\x00", framed(payload(14))])
    assert list(SerialPacketSource(stream)) == [payload(14)]


def test_source_survives_a_stream_that_is_only_garbage():
    src = SerialPacketSource(ScriptedStream([BOOT_CHATTER, b"\xa5" * 100]))
    assert list(src) == []
    # Positive control: the same garbage with a frame appended does yield it. Without
    # this the test passes against a source that yields nothing from any input.
    ok = SerialPacketSource(ScriptedStream([BOOT_CHATTER, b"\xa5" * 100, framed(payload(18))]))
    assert list(ok) == [payload(18)]


def test_source_drives_the_gateway_end_to_end():
    """The seam's actual job: a real packet, framed, off a serial stream, decoded.

    Built with the C++ serializer's own golden vector so this is graded against the
    packet contract rather than against a packet this test invented.
    """
    import json
    import pathlib

    vectors = json.loads(
        (pathlib.Path(__file__).resolve().parents[2] / "contracts/vectors/packet-v1.json").read_text()
    )
    raw = bytes.fromhex(vectors["vectors"][0]["expected"]["hex"])
    assert len(raw) == vectors["vectors"][0]["expected"]["len"]

    published: list[dict] = []
    src = SerialPacketSource(ScriptedStream([BOOT_CHATTER, framed(raw)]))
    assert Gateway(src, published.append).run() == 1
    assert len(published) == 1
