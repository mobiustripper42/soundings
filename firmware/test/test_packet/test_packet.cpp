#include <unity.h>
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include "packet.h"

// Packet v1 round-trip tests, graded against the SHARED golden vectors
// (contracts/vectors/packet-v1.json) — the same fixtures the Python gateway
// parser is checked against, which is what guarantees the two sides can't drift.
// For every vector: struct -> serialize must equal the exact hex, and hex ->
// deserialize must reproduce the struct. Plus the malformed-input rejection paths.

using namespace soundings;

// Path is relative to the CWD `pio test` runs from (the firmware/ project dir).
#ifndef PACKET_VECTORS_JSON
#define PACKET_VECTORS_JSON "../contracts/vectors/packet-v1.json"
#endif

static JsonDocument g_doc;
static std::map<std::string, int> g_bit;   // channel name -> registry bit

void setUp() {}
void tearDown() {}

static std::string toHex(const uint8_t* b, size_t n) {
    static const char* h = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(h[b[i] >> 4]);
        s.push_back(h[b[i] & 0xF]);
    }
    return s;
}

static size_t fromHex(const std::string& s, uint8_t* out, size_t cap) {
    size_t n = s.size() / 2;
    TEST_ASSERT_TRUE(n <= cap);
    for (size_t i = 0; i < n; ++i)
        out[i] = (uint8_t)std::stoi(s.substr(i * 2, 2), nullptr, 16);
    return n;
}

static void loadVectors() {
    std::ifstream f(PACKET_VECTORS_JSON);
    TEST_ASSERT_TRUE_MESSAGE(f.good(),
        "cannot open " PACKET_VECTORS_JSON " — run `pio test` from firmware/");
    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();
    DeserializationError e = deserializeJson(g_doc, json);
    TEST_ASSERT_TRUE_MESSAGE(!e, "golden-vector JSON failed to parse");
    for (JsonObject c : g_doc["channel_registry"].as<JsonArray>())
        g_bit[std::string(c["name"].as<const char*>())] = c["bit"].as<int>();
}

static Packet buildFrom(JsonObject f) {
    Packet p;
    p.proto_ver    = f["proto_ver"];
    p.node_id      = f["node_id"];
    p.fw_version   = f["fw_version"];
    p.seq          = f["seq"];
    p.battery_mv   = f["battery_mv"];
    p.channel_mask = f["channel_mask"];
    p.fault_mask   = f["fault_mask"];
    for (JsonObject c : f["channels"].as<JsonArray>()) {
        int bit = g_bit[std::string(c["name"].as<const char*>())];
        long raw = c["raw"].as<long>();                 // signed in JSON (e.g. -80)
        p.channels[bit] = (uint16_t)(raw & 0xFFFF);      // two's-complement on the wire
    }
    return p;
}

// The load-bearing test: every shared vector must round-trip exactly.
void test_all_vectors_roundtrip() {
    loadVectors();
    JsonArray vectors = g_doc["vectors"].as<JsonArray>();
    TEST_ASSERT_GREATER_THAN(0u, vectors.size());

    for (JsonObject v : vectors) {
        const char* name = v["name"];
        JsonObject f = v["fields"];
        std::string expHex = v["expected"]["hex"].as<const char*>();
        size_t expLen = v["expected"]["len"].as<size_t>();

        // struct -> bytes == golden hex
        Packet p = buildFrom(f);
        uint8_t buf[kMaxPacketLen];
        size_t n = serialize(p, buf, sizeof(buf));
        TEST_ASSERT_EQUAL_size_t_MESSAGE(expLen, n, name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expHex.c_str(), toHex(buf, n).c_str(), name);

        // bytes -> struct, and fields match
        uint8_t raw[kMaxPacketLen];
        size_t rn = fromHex(expHex, raw, sizeof(raw));
        Packet q;
        TEST_ASSERT_EQUAL_MESSAGE((int)ParseResult::Ok, (int)deserialize(raw, rn, q), name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(p.node_id, q.node_id, name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(p.fw_version, q.fw_version, name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(p.seq, q.seq, name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(p.battery_mv, q.battery_mv, name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(p.channel_mask, q.channel_mask, name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(p.fault_mask, q.fault_mask, name);
        for (int bit = 0; bit < kMaxChannels; ++bit)
            if (p.channel_mask & (1u << bit))
                TEST_ASSERT_EQUAL_HEX16_MESSAGE(p.channels[bit], q.channels[bit], name);
    }
}

// A known-good minimal packet (tank_node_nominal) for the rejection-path tests.
static size_t goodTankPacket(uint8_t* out) {
    Packet p;
    p.node_id = 10;
    p.fw_version = 100;
    p.seq = 1;
    p.battery_mv = 3700;
    p.setChannel(8, 1234);   // TANK_DISTANCE
    return serialize(p, out, kMaxPacketLen);
}

void test_self_roundtrip_via_api() {
    uint8_t buf[kMaxPacketLen];
    size_t n = goodTankPacket(buf);
    Packet q;
    TEST_ASSERT_EQUAL((int)ParseResult::Ok, (int)deserialize(buf, n, q));
    TEST_ASSERT_TRUE(q.hasChannel(8));
    TEST_ASSERT_EQUAL_UINT16(1234, q.channels[8]);
}

void test_bad_crc_rejected() {
    uint8_t buf[kMaxPacketLen];
    size_t n = goodTankPacket(buf);
    buf[12] ^= 0xFF;                       // corrupt a payload byte
    Packet q;
    TEST_ASSERT_EQUAL((int)ParseResult::BadCrc, (int)deserialize(buf, n, q));
}

void test_truncated_rejected() {
    uint8_t buf[kMaxPacketLen];
    size_t n = goodTankPacket(buf);
    Packet q;
    TEST_ASSERT_EQUAL((int)ParseResult::TooShort, (int)deserialize(buf, kHeaderLen, q));
    (void)n;
}

void test_unknown_proto_rejected() {
    uint8_t buf[kMaxPacketLen];
    size_t n = goodTankPacket(buf);
    buf[0] = 0x02;                         // not v1
    Packet q;
    TEST_ASSERT_EQUAL((int)ParseResult::UnknownProto, (int)deserialize(buf, n, q));
}

void test_unknown_channel_rejected() {
    // Declare reserved bit 13 (width 0) — parser can't size the layout, must drop.
    uint8_t buf[kMaxPacketLen];
    Packet p;
    p.node_id = 5;
    p.channel_mask = (uint16_t)(1u << 13);
    // Build the bytes by hand: serialize() refuses unknown channels, so craft directly.
    size_t o = 0;
    buf[o++] = kProtoV1; buf[o++] = 5;
    buf[o++] = 0; buf[o++] = 0;            // fw
    buf[o++] = 0; buf[o++] = 0;            // seq
    buf[o++] = 0; buf[o++] = 0;            // batt
    buf[o++] = 0x00; buf[o++] = 0x20;      // channel_mask = 0x2000 (bit 13)
    buf[o++] = 0; buf[o++] = 0;            // fault_mask
    buf[o++] = 0; buf[o++] = 0;            // two "value" bytes so length is plausible
    uint16_t crc = crc16_ccitt_false(buf, o);
    buf[o++] = (uint8_t)(crc & 0xFF); buf[o++] = (uint8_t)(crc >> 8);
    Packet q;
    TEST_ASSERT_EQUAL((int)ParseResult::UnknownChannel, (int)deserialize(buf, o, q));
}

void test_serialize_refuses_unknown_channel() {
    Packet p;
    p.channel_mask = (uint16_t)(1u << 14);  // reserved
    uint8_t buf[kMaxPacketLen];
    TEST_ASSERT_EQUAL_size_t(0, serialize(p, buf, sizeof(buf)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_all_vectors_roundtrip);
    RUN_TEST(test_self_roundtrip_via_api);
    RUN_TEST(test_bad_crc_rejected);
    RUN_TEST(test_truncated_rejected);
    RUN_TEST(test_unknown_proto_rejected);
    RUN_TEST(test_unknown_channel_rejected);
    RUN_TEST(test_serialize_refuses_unknown_channel);
    return UNITY_END();
}
