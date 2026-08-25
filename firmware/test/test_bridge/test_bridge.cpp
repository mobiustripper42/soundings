#include <unity.h>
#include <string.h>
#include "gateway_bridge.h"
#include "serial_framing.h"
#include "packet.h"
#include "downlink.h"
#include "../fakes/fake_bytesource.h"
#include "../fakes/fake_clock.h"

// Phase 3.9c — the gateway board's reply window.
//
// WHY THIS IS IN src/core AND NOT IN THE SKETCH. The board's relay logic lived in
// src/gateway/main.cpp, which NO test env compiles (platformio.ini) — and in 3.9b that
// cost us: the hand-rolled frame reader there got the invalid-length resync wrong and
// nothing could have caught it. The reader was moved to core at review. This is the same
// class of logic, so it starts there.
//
// WHAT IT IS FOR. The node holds a ~250 ms receive window after each transmit
// (DEC-010, runcycle.h). The board used to relay a packet up to the daemon and then
// immediately re-enter a 200 ms BLOCKING LoRa receive, so the daemon's reply sat unread
// in the UART buffer for up to 200 ms of a 250 ms budget. The window is nearly gone
// before the downlink is even transmitted.
//
// The fix follows the protocol's own shape: once a packet has gone up, the node that
// sent it is holding its window RIGHT NOW and nothing else is going to transmit for
// fifteen minutes. So the board stops listening to the radio and polls the serial port
// hard until the reply arrives or the window closes.

using namespace soundings;

void setUp() {}
void tearDown() {}

namespace {

// A framed downlink, the thing that actually travels this direction.
void pushFramedDownlink(FakeByteSource& src, uint8_t node_id, uint16_t flags) {
    Downlink d;
    d.node_id = node_id;
    d.flags   = flags;
    uint8_t raw[kDownlinkLen] = {};
    encodeDownlink(d, raw, sizeof(raw));

    uint8_t framed[kDownlinkLen + kSerialFrameOverhead] = {};
    const size_t n = frameForSerial(raw, kDownlinkLen, framed, sizeof(framed));
    for (size_t i = 0; i < n; ++i) src.push(framed[i]);
}

// Boot chatter — the ESP32 prints it on every reset, and the daemon's side of this cable
// is a process that can restart. Real text: it contains no sync pair.
void pushChatter(FakeByteSource& src) {
    const char* s = "rst:0x1 (POWERON),boot:0x8\r\n";
    for (const char* p = s; *p; ++p) src.push((uint8_t)*p);
}

} // namespace

// ---- The reply arrives -----------------------------------------------------

void test_relays_a_downlink_that_arrives_inside_the_window() {
    FakeByteSource src;
    FakeClock clock;
    clock.autoAdvance(1);
    pushFramedDownlink(src, 7, 0x0001);

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    const size_t n = awaitFramedPayload(src, reader, clock, 400, out, sizeof(out));

    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen, n);
    // And it is the real thing, not just the right length — decodes for node 7.
    Downlink d;
    TEST_ASSERT_TRUE(decodeDownlink(out, n, 7, d));
    TEST_ASSERT_EQUAL_UINT16(0x0001, d.flags);
}

void test_returns_as_soon_as_the_frame_is_complete_not_at_the_deadline() {
    // THE POINT OF THE WHOLE CHANGE. A relay that waits out its window before handing
    // the frame over spends the node's receive budget doing nothing, which is exactly
    // the 200 ms stall this replaces. Burning the window is the bug, not slowness.
    FakeByteSource src;
    FakeClock clock;
    clock.autoAdvance(1);
    pushFramedDownlink(src, 7, 0);

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen,
                             awaitFramedPayload(src, reader, clock, 400, out, sizeof(out)));

    // The frame is 9 bytes; it must not have cost anything like the 400 ms window.
    TEST_ASSERT_LESS_THAN_UINT32(100, clock.millis());
}

void test_finds_the_frame_behind_boot_chatter() {
    FakeByteSource src;
    FakeClock clock;
    clock.autoAdvance(1);
    pushChatter(src);
    pushFramedDownlink(src, 7, 0x0001);

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen,
                             awaitFramedPayload(src, reader, clock, 400, out, sizeof(out)));
}

// ---- The window closes -----------------------------------------------------

void test_a_silent_port_returns_nothing_and_costs_exactly_the_window() {
    FakeByteSource src;   // nothing queued: the ordinary case, on almost every wake
    FakeClock clock;
    clock.autoAdvance(1);

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    TEST_ASSERT_EQUAL_UINT32(0, awaitFramedPayload(src, reader, clock, 50, out, sizeof(out)));

    // It waited, rather than returning instantly — a poll loop that gave up on the first
    // empty read would miss every reply that was merely in flight.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(50, clock.millis());

    // POSITIVE CONTROL, same test. Without it this passes against a function that always
    // returns 0 — and "returns 0 on silence" is the single most common false green in
    // this repo (nineteen found in the Task 3 audit, five more written after).
    FakeByteSource live;
    FakeClock c2;
    c2.autoAdvance(1);
    pushFramedDownlink(live, 7, 0);
    SerialFrameReader r2;
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen,
                             awaitFramedPayload(live, r2, c2, 50, out, sizeof(out)));
}

void test_a_partial_frame_is_not_reported_as_a_reply() {
    // The daemon wrote half a frame and stopped — or the window closed mid-write.
    // Half a downlink is not a downlink, and reporting one would hand the radio a
    // truncated payload to transmit.
    FakeByteSource src;
    FakeClock clock;
    clock.autoAdvance(1);
    src.push(kSerialSync0);
    src.push(kSerialSync1);
    src.push((uint8_t)kDownlinkLen);
    src.push(0x01);   // one payload byte of six

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    TEST_ASSERT_EQUAL_UINT32(0, awaitFramedPayload(src, reader, clock, 50, out, sizeof(out)));

    // Positive control: the SAME reader, given the rest of the frame in a later window,
    // completes it. The bytes already consumed must not have been thrown away — a real
    // port splits writes wherever it likes.
    for (int i = 0; i < 5; ++i) src.push(0x01);
    FakeClock c2;
    c2.autoAdvance(1);
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen,
                             awaitFramedPayload(src, reader, c2, 50, out, sizeof(out)));
}

void test_a_zero_window_does_not_poll_at_all() {
    // 0 disables the reply window, mirroring rxWindowMs == 0 disabling the node's
    // listen (runcycle.h). It must not read a single byte — a bridge configured not to
    // relay that still drains the port would eat a frame the next window needed.
    FakeByteSource src;
    FakeClock clock;
    clock.autoAdvance(1);
    pushFramedDownlink(src, 7, 0);

    SerialFrameReader reader;
    uint8_t out[kMaxPacketLen] = {};
    TEST_ASSERT_EQUAL_UINT32(0, awaitFramedPayload(src, reader, clock, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(0, src.consumed());

    // Positive control: a non-zero window on the same queued frame does relay it.
    FakeClock c2;
    c2.autoAdvance(1);
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen,
                             awaitFramedPayload(src, reader, c2, 50, out, sizeof(out)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_relays_a_downlink_that_arrives_inside_the_window);
    RUN_TEST(test_returns_as_soon_as_the_frame_is_complete_not_at_the_deadline);
    RUN_TEST(test_finds_the_frame_behind_boot_chatter);
    RUN_TEST(test_a_silent_port_returns_nothing_and_costs_exactly_the_window);
    RUN_TEST(test_a_partial_frame_is_not_reported_as_a_reply);
    RUN_TEST(test_a_zero_window_does_not_poll_at_all);
    return UNITY_END();
}
