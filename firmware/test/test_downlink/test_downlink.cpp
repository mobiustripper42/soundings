#include <unity.h>
#include <string.h>
#include "downlink.h"
#include "packet.h"
#include "serial_framing.h"

// Phase 3.9b — downlink v1 (contracts/downlink-v1.md), node side.
//
// Six bytes the gateway may send into the receive window the node holds after each
// transmit (DEC-010). The Python daemon has an independent encoder; both are exercised
// against the same literal byte strings rather than a shared vector file, for the same
// reason serial framing has none — fixed shape, no computed layout, and a mistake here
// means nothing decodes rather than a plausible wrong number.

using namespace soundings;

void setUp() {}
void tearDown() {}

// A known-good downlink for node 7 with flags 0x0000, built by hand rather than by the
// encoder — otherwise a bug in the encoder would be invisible to every decode test here.
static void handBuilt(uint8_t node_id, uint16_t flags, uint8_t out[kDownlinkLen]) {
    out[0] = kDownlinkProtoV1;
    out[1] = node_id;
    out[2] = (uint8_t)(flags & 0xFF);
    out[3] = (uint8_t)(flags >> 8);
    const uint16_t crc = crc16_ccitt_false(out, 4);
    out[4] = (uint8_t)(crc & 0xFF);
    out[5] = (uint8_t)(crc >> 8);
}

// ---- Decode ----------------------------------------------------------------

void test_decodes_a_downlink_addressed_to_this_node() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0x0000, raw);

    Downlink d;
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));
    TEST_ASSERT_EQUAL_UINT8(7, d.node_id);
    TEST_ASSERT_EQUAL_UINT16(0x0000, d.flags);
}

void test_flags_round_trip_little_endian() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0xBEEF, raw);
    TEST_ASSERT_EQUAL_UINT8(0xEF, raw[2]);   // low byte first, matching packet-v1
    TEST_ASSERT_EQUAL_UINT8(0xBE, raw[3]);

    Downlink d;
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, d.flags);
}

// Two nodes in earshot both hear both replies, so this is the common rejection rather
// than the exceptional one.
void test_rejects_a_downlink_for_another_node() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0x0001, raw);

    Downlink d;
    TEST_ASSERT_FALSE(decodeDownlink(raw, sizeof(raw), 8, d));
    // ...and the same bytes ARE accepted by the node they name. Without this the test
    // passes against a decoder that rejects everything.
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));
}

void test_rejects_a_corrupt_crc() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0x0000, raw);
    Downlink d;
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));   // good before

    raw[4] ^= 0xFF;                                             // ...and bad after
    TEST_ASSERT_FALSE(decodeDownlink(raw, sizeof(raw), 7, d));
}

// A corrupted flags byte must fail the CRC rather than arriving as different flags —
// that is the whole reason a six-byte message carries two bytes of checksum.
void test_a_flipped_flag_bit_fails_the_crc_rather_than_changing_the_flags() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0x0000, raw);

    Downlink d;
    d.flags = 0xAAAA;
    // Positive control first: without it this passed against a decoder that rejects
    // everything and never writes `out` — which is exactly what the stub did.
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));
    TEST_ASSERT_EQUAL_UINT16(0x0000, d.flags);

    raw[2] ^= 0x01;
    d.flags = 0xAAAA;
    TEST_ASSERT_FALSE(decodeDownlink(raw, sizeof(raw), 7, d));
    TEST_ASSERT_EQUAL_UINT16(0xAAAA, d.flags);   // and `out` is left untouched
}

void test_rejects_an_unknown_protocol_version() {
    uint8_t raw[kDownlinkLen];
    handBuilt(7, 0x0000, raw);
    raw[0] = 0x02;
    // Re-checksum so the ONLY thing wrong is the version — otherwise this passes on the
    // CRC path and says nothing about version handling.
    const uint16_t crc = crc16_ccitt_false(raw, 4);
    raw[4] = (uint8_t)(crc & 0xFF);
    raw[5] = (uint8_t)(crc >> 8);

    Downlink d;
    TEST_ASSERT_FALSE(decodeDownlink(raw, sizeof(raw), 7, d));
    raw[0] = kDownlinkProtoV1;                   // and with v1 restored...
    const uint16_t good = crc16_ccitt_false(raw, 4);
    raw[4] = (uint8_t)(good & 0xFF);
    raw[5] = (uint8_t)(good >> 8);
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 7, d));
}

void test_rejects_anything_that_is_not_exactly_six_bytes() {
    uint8_t raw[kDownlinkLen + 2];
    handBuilt(7, 0x0000, raw);
    raw[kDownlinkLen] = 0x00;
    raw[kDownlinkLen + 1] = 0x00;

    Downlink d;
    TEST_ASSERT_FALSE(decodeDownlink(raw, kDownlinkLen - 1, 7, d));   // short
    TEST_ASSERT_FALSE(decodeDownlink(raw, kDownlinkLen + 2, 7, d));   // long
    TEST_ASSERT_TRUE(decodeDownlink(raw, kDownlinkLen, 7, d));        // exact
}

// Node 0 is a legal address and must not be confused with a default-initialised field.
void test_node_zero_is_addressable() {
    uint8_t raw[kDownlinkLen];
    handBuilt(0, 0x0000, raw);
    Downlink d;
    TEST_ASSERT_TRUE(decodeDownlink(raw, sizeof(raw), 0, d));
    TEST_ASSERT_FALSE(decodeDownlink(raw, sizeof(raw), 1, d));
}

// ---- Encode ----------------------------------------------------------------

void test_encode_matches_the_hand_built_frame_byte_for_byte() {
    uint8_t expected[kDownlinkLen];
    handBuilt(7, 0x1234, expected);

    Downlink d;
    d.node_id = 7;
    d.flags   = 0x1234;
    uint8_t out[kDownlinkLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen, encodeDownlink(d, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, kDownlinkLen);
}

void test_encode_refuses_a_buffer_that_is_too_small() {
    Downlink d;
    d.node_id = 7;
    uint8_t out[kDownlinkLen - 1] = {};
    memset(out, 0xEE, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(0, encodeDownlink(d, out, sizeof(out)));
    for (size_t i = 0; i < sizeof(out); ++i) TEST_ASSERT_EQUAL_UINT8(0xEE, out[i]);

    uint8_t roomy[kDownlinkLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen, encodeDownlink(d, roomy, sizeof(roomy)));
}

// ---- Serial framing carries it too (the 3.9b amendment) --------------------

// A downlink travels daemon -> serial -> gateway board -> LoRa, so serial framing v1
// carries it in the reverse direction. Its minimum payload dropped from 14 to 6 for
// exactly this; that amendment is only real if a 6-byte payload actually frames.
void test_a_downlink_fits_the_serial_envelope() {
    Downlink d;
    d.node_id = 7;
    uint8_t raw[kDownlinkLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen, encodeDownlink(d, raw, sizeof(raw)));

    uint8_t framed[kDownlinkLen + kSerialFrameOverhead] = {};
    const size_t n = frameForSerial(raw, kDownlinkLen, framed, sizeof(framed));

    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen + kSerialFrameOverhead, n);
    TEST_ASSERT_EQUAL_UINT8(kDownlinkLen, framed[2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(raw, framed + kSerialFrameOverhead, kDownlinkLen);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_decodes_a_downlink_addressed_to_this_node);
    RUN_TEST(test_flags_round_trip_little_endian);
    RUN_TEST(test_rejects_a_downlink_for_another_node);
    RUN_TEST(test_rejects_a_corrupt_crc);
    RUN_TEST(test_a_flipped_flag_bit_fails_the_crc_rather_than_changing_the_flags);
    RUN_TEST(test_rejects_an_unknown_protocol_version);
    RUN_TEST(test_rejects_anything_that_is_not_exactly_six_bytes);
    RUN_TEST(test_node_zero_is_addressable);
    RUN_TEST(test_encode_matches_the_hand_built_frame_byte_for_byte);
    RUN_TEST(test_encode_refuses_a_buffer_that_is_too_small);
    RUN_TEST(test_a_downlink_fits_the_serial_envelope);
    return UNITY_END();
}
