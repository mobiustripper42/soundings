#include <unity.h>
#include <string.h>
#include "serial_framing.h"
#include "packet.h"

// Phase 3.9a — serial framing v1, encode half (contracts/serial-framing-v1.md).
//
// The decoder is Python and lives in gateway/soundings_gateway/framing.py; there is no
// shared vector file, deliberately, and the contract doc says why: the framing is
// one-way, fixed-shape, and fails loudly rather than silently. The cases here and the
// cases in gateway/tests/test_framing.py describe the same three-field envelope from
// their own side.

using namespace soundings;

void setUp() {}
void tearDown() {}

// A payload of `n` bytes. Not a real packet — the framer neither knows nor cares what is
// inside, and building a real one here would test two contracts at once.
static void fillPayload(uint8_t* buf, size_t n, uint8_t fill) {
    for (size_t i = 0; i < n; ++i) buf[i] = fill;
}

void test_frames_a_payload_with_sync_and_length() {
    uint8_t payload[20];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[64] = {};

    const size_t n = frameForSerial(payload, sizeof(payload), out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT32(sizeof(payload) + kSerialFrameOverhead, n);
    TEST_ASSERT_EQUAL_UINT8(0xA5, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5A, out[1]);
    TEST_ASSERT_EQUAL_UINT8(20,   out[2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out + kSerialFrameOverhead, sizeof(payload));
}

// The payload is copied byte for byte with nothing escaped or substituted. A framer that
// escaped the sync pattern inside the payload would silently corrupt any packet
// containing 0xA5 0x5A — and the decoder does not unescape, because it does not have to.
void test_payload_containing_the_sync_pattern_is_copied_verbatim() {
    uint8_t payload[16] = {0x01, 0x02, kSerialSync0, kSerialSync1, 0x20,
                           0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                           0x11, kSerialSync0, kSerialSync0};
    uint8_t out[64] = {};

    const size_t n = frameForSerial(payload, sizeof(payload), out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT32(sizeof(payload) + kSerialFrameOverhead, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out + kSerialFrameOverhead, sizeof(payload));
}

void test_frames_the_smallest_and_largest_legal_payloads() {
    uint8_t payload[kMaxPacketLen];
    uint8_t out[kMaxPacketLen + kSerialFrameOverhead] = {};

    fillPayload(payload, kMinFramedPayload, 0xAA);
    TEST_ASSERT_EQUAL_UINT32(kMinFramedPayload + kSerialFrameOverhead,
                             frameForSerial(payload, kMinFramedPayload, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8(kMinFramedPayload, out[2]);

    fillPayload(payload, kMaxPacketLen, 0xBB);
    TEST_ASSERT_EQUAL_UINT32(kMaxPacketLen + kSerialFrameOverhead,
                             frameForSerial(payload, kMaxPacketLen, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8(kMaxPacketLen, out[2]);
}

// Out-of-range lengths are refused rather than clamped. A length byte the decoder will
// reject is a frame that cost airtime and arrives as garbage; refusing here means the
// caller finds out on the spot.
// Each refusal test also asserts the adjacent LEGAL length succeeds. Without that pair,
// an encoder that refused everything would pass — which is exactly what the stub these
// were written against did, and it is the boundary itself that is being pinned, not a
// general willingness to say no.
void test_refuses_a_payload_shorter_than_a_packet_can_be() {
    uint8_t payload[kMaxPacketLen];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[64] = {};
    TEST_ASSERT_EQUAL_UINT32(0, frameForSerial(payload, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(0, frameForSerial(payload, kMinFramedPayload - 1, out, sizeof(out)));
    TEST_ASSERT_TRUE(frameForSerial(payload, kMinFramedPayload, out, sizeof(out)) > 0);
}

void test_refuses_a_payload_longer_than_a_packet_can_be() {
    uint8_t payload[kMaxPacketLen + 8];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[128] = {};
    TEST_ASSERT_EQUAL_UINT32(0, frameForSerial(payload, kMaxPacketLen + 1, out, sizeof(out)));
    TEST_ASSERT_TRUE(frameForSerial(payload, kMaxPacketLen, out, sizeof(out)) > 0);
}

// Refusing rather than truncating is the point: a partial frame desynchronises the reader
// for a frame, and the caller — which has a real packet in hand and a 15-minute wait
// before the next one — would have no way to know it happened.
void test_refuses_rather_than_truncating_when_the_buffer_is_too_small() {
    uint8_t payload[20];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[10] = {};
    memset(out, 0xEE, sizeof(out));

    TEST_ASSERT_EQUAL_UINT32(0, frameForSerial(payload, sizeof(payload), out, sizeof(out)));
    // And it wrote nothing at all — not a header, not a partial payload.
    for (size_t i = 0; i < sizeof(out); ++i) TEST_ASSERT_EQUAL_UINT8(0xEE, out[i]);

    // Same payload, a buffer that fits: succeeds. Otherwise this passes against an
    // encoder that refuses unconditionally.
    uint8_t roomy[64] = {};
    TEST_ASSERT_TRUE(frameForSerial(payload, sizeof(payload), roomy, sizeof(roomy)) > 0);
}

// Exactly-fits is the boundary the off-by-one lives on, in both directions.
void test_accepts_a_buffer_that_exactly_fits_and_refuses_one_byte_less() {
    uint8_t payload[kMaxPacketLen];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[kMaxPacketLen + kSerialFrameOverhead] = {};

    TEST_ASSERT_EQUAL_UINT32(sizeof(out), frameForSerial(payload, sizeof(payload), out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(0, frameForSerial(payload, sizeof(payload), out, sizeof(out) - 1));
}

// The overhead constant and the bytes actually written must agree — they are stated in
// two places (the header and the encoder) and a reader sizing a buffer trusts the constant.
void test_overhead_constant_matches_the_bytes_written() {
    uint8_t payload[kMinFramedPayload];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t out[64] = {};
    const size_t n = frameForSerial(payload, sizeof(payload), out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(kSerialFrameOverhead, n - sizeof(payload));
    TEST_ASSERT_EQUAL_UINT32(3, kSerialFrameOverhead);
}

// A real packet, serialized by the real serializer, survives framing intact. This is the
// one case that connects the envelope to the thing it carries.
void test_a_real_serialized_packet_frames_intact() {
    Packet p;
    p.node_id    = 10;
    p.fw_version = 100;
    p.seq        = 1;
    p.battery_mv = 3700;
    p.setChannel(8, 1234);

    uint8_t raw[kMaxPacketLen];
    const size_t rawLen = serialize(p, raw, sizeof(raw));
    TEST_ASSERT_TRUE(rawLen >= kMinFramedPayload);

    uint8_t out[kMaxPacketLen + kSerialFrameOverhead] = {};
    const size_t n = frameForSerial(raw, rawLen, out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT32(rawLen + kSerialFrameOverhead, n);
    TEST_ASSERT_EQUAL_UINT8(rawLen, out[2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(raw, out + kSerialFrameOverhead, rawLen);
}

// ---- SerialFrameReader (decode) --------------------------------------------
//
// These mirror gateway/tests/test_framing.py case for case. Two independent readers of
// one contract is exactly the drift risk packet-v1 uses vectors for — here the pairing is
// the test names, deliberately, because the shapes are fixed and a divergence shows up as
// one suite failing rather than as a plausible wrong answer.

// Feed a whole buffer; collect every payload that completed. Returns how many.
static int feedAll(SerialFrameReader& r, const uint8_t* data, size_t n,
                   uint8_t out[][kMaxPacketLen], size_t* lens, int cap) {
    int got = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t tmp[kMaxPacketLen];
        const size_t len = r.feed(data[i], tmp, sizeof(tmp));
        if (len > 0 && got < cap) {
            memcpy(out[got], tmp, len);
            lens[got] = len;
            ++got;
        }
    }
    return got;
}

static size_t buildFrame(uint8_t* dst, const uint8_t* payload, uint8_t len) {
    dst[0] = kSerialSync0; dst[1] = kSerialSync1; dst[2] = len;
    memcpy(dst + 3, payload, len);
    return len + 3u;
}

void test_reader_yields_a_whole_frame() {
    uint8_t payload[20];
    fillPayload(payload, sizeof(payload), 0x11);
    uint8_t wire[64];
    const size_t n = buildFrame(wire, payload, 20);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(20, lens[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out[0], 20);
}

void test_reader_discards_leading_garbage() {
    uint8_t payload[16];
    fillPayload(payload, sizeof(payload), 0x22);
    uint8_t wire[128];
    const uint8_t junk[] = {'E','S','P','-','R','O','M',':','\r','\n'};
    memcpy(wire, junk, sizeof(junk));
    const size_t n = sizeof(junk) + buildFrame(wire + sizeof(junk), payload, 16);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(sizeof(junk), r.discarded());
}

// A stray sync byte before a real frame. Named for what it does: mutation-checked, this
// one does NOT reach the bad-length branch — `A5 | A5 5A 0E` resolves at step 1, because
// the scan finds the real pair at index 1 and never range-checks a bad length. It was
// originally named for the bad-length rule and tested no such thing, which is the exact
// mislabelling that turns an unverified claim into an apparently-verified one.
void test_reader_locks_on_after_a_stray_sync_byte() {
    uint8_t payload[14];
    fillPayload(payload, sizeof(payload), 0x33);
    uint8_t wire[64];
    wire[0] = kSerialSync0;                       // false start
    const size_t n = 1 + buildFrame(wire + 1, payload, 14);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(14, lens[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out[0], 14);
}

// THE BUG THIS CLASS EXISTS FOR, and the only test that reaches the bad-length branch —
// confirmed by mutation: reintroduce the sketch's discard-all-three and this is the one
// that fails. `A5 5A <impossible LEN> ...` where the LEN byte is itself 0xA5, so the real
// header starts one byte in. Step 3 of the contract is what makes it recoverable.
void test_reader_recovers_a_frame_hidden_behind_a_false_header() {
    uint8_t payload[6];
    fillPayload(payload, sizeof(payload), 0x44);
    uint8_t wire[64];
    wire[0] = kSerialSync0; wire[1] = kSerialSync1;   // sync, then an impossible length
    const size_t n = 2 + buildFrame(wire + 2, payload, 6);
    TEST_ASSERT_EQUAL_UINT8(kSerialSync0, wire[2]);   // the bad LEN *is* a sync byte

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(6, lens[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out[0], 6);
}

void test_reader_handles_back_to_back_frames() {
    uint8_t a[14], b[46];
    fillPayload(a, sizeof(a), 0xAA);
    fillPayload(b, sizeof(b), 0xBB);
    uint8_t wire[128];
    size_t n = buildFrame(wire, a, 14);
    n += buildFrame(wire + n, b, 46);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(2, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(14, lens[0]);
    TEST_ASSERT_EQUAL_UINT32(46, lens[1]);
}

// A six-byte downlink is the reason the floor moved from 14 to 6.
void test_reader_accepts_the_smallest_legal_payload() {
    uint8_t payload[kMinFramedPayload];
    fillPayload(payload, sizeof(payload), 0x55);
    uint8_t wire[32];
    const size_t n = buildFrame(wire, payload, (uint8_t)kMinFramedPayload);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(kMinFramedPayload, lens[0]);

    // ...and one byte below it is still refused, so the floor is pinned in both
    // directions rather than merely lowered.
    SerialFrameReader r2;
    uint8_t below[32];
    below[0] = kSerialSync0; below[1] = kSerialSync1; below[2] = kMinFramedPayload - 1;
    memset(below + 3, 0, 8);
    TEST_ASSERT_EQUAL_INT(0, feedAll(r2, below, 11, out, lens, 4));
}

void test_reader_survives_a_stream_of_pure_garbage_without_overflowing() {
    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    uint8_t junk[256];
    memset(junk, kSerialSync0, sizeof(junk));       // worst case: all sync bytes
    for (int round = 0; round < 20; ++round) {
        TEST_ASSERT_EQUAL_INT(0, feedAll(r, junk, sizeof(junk), out, lens, 4));
    }
    // And a real frame still gets through afterwards — proving it did not wedge.
    uint8_t payload[14];
    fillPayload(payload, sizeof(payload), 0x66);
    uint8_t wire[64];
    const size_t n = buildFrame(wire, payload, 14);
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
}

void test_reader_drops_a_payload_larger_than_the_callers_buffer() {
    uint8_t payload[46];
    fillPayload(payload, sizeof(payload), 0x77);
    uint8_t wire[64];
    const size_t n = buildFrame(wire, payload, 46);

    SerialFrameReader r;
    uint8_t small[20];
    int completed = 0;
    for (size_t i = 0; i < n; ++i) {
        if (r.feed(wire[i], small, sizeof(small)) > 0) ++completed;
    }
    TEST_ASSERT_EQUAL_INT(0, completed);   // consumed and dropped, not truncated

    // ...and the reader is still usable: it consumed the frame rather than stalling.
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    uint8_t nxt[64];
    uint8_t p2[14];
    fillPayload(p2, sizeof(p2), 0x88);
    const size_t n2 = buildFrame(nxt, p2, 14);
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, nxt, n2, out, lens, 4));
}

// Round trip against the encoder in the same translation unit — the two halves of one
// contract, which is the pairing the Python side cannot check.
void test_encoder_and_reader_round_trip() {
    uint8_t payload[30];
    fillPayload(payload, sizeof(payload), 0x99);
    uint8_t wire[64] = {};
    const size_t n = frameForSerial(payload, sizeof(payload), wire, sizeof(wire));
    TEST_ASSERT_TRUE(n > 0);

    SerialFrameReader r;
    uint8_t out[4][kMaxPacketLen]; size_t lens[4];
    TEST_ASSERT_EQUAL_INT(1, feedAll(r, wire, n, out, lens, 4));
    TEST_ASSERT_EQUAL_UINT32(30, lens[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out[0], 30);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reader_yields_a_whole_frame);
    RUN_TEST(test_reader_discards_leading_garbage);
    RUN_TEST(test_reader_locks_on_after_a_stray_sync_byte);
    RUN_TEST(test_reader_recovers_a_frame_hidden_behind_a_false_header);
    RUN_TEST(test_reader_handles_back_to_back_frames);
    RUN_TEST(test_reader_accepts_the_smallest_legal_payload);
    RUN_TEST(test_reader_survives_a_stream_of_pure_garbage_without_overflowing);
    RUN_TEST(test_reader_drops_a_payload_larger_than_the_callers_buffer);
    RUN_TEST(test_encoder_and_reader_round_trip);
    RUN_TEST(test_frames_a_payload_with_sync_and_length);
    RUN_TEST(test_payload_containing_the_sync_pattern_is_copied_verbatim);
    RUN_TEST(test_frames_the_smallest_and_largest_legal_payloads);
    RUN_TEST(test_refuses_a_payload_shorter_than_a_packet_can_be);
    RUN_TEST(test_refuses_a_payload_longer_than_a_packet_can_be);
    RUN_TEST(test_refuses_rather_than_truncating_when_the_buffer_is_too_small);
    RUN_TEST(test_accepts_a_buffer_that_exactly_fits_and_refuses_one_byte_less);
    RUN_TEST(test_overhead_constant_matches_the_bytes_written);
    RUN_TEST(test_a_real_serialized_packet_frames_intact);
    return UNITY_END();
}
