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

int main(int, char**) {
    UNITY_BEGIN();
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
