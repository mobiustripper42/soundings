#include <unity.h>
#include "a02yyuw.h"
#include "../fakes/fake_bytesource.h"
#include "../fakes/fake_powerrail.h"
#include "../fakes/fake_clock.h"

// Phase 3.8a — the A02YYUW frame parser and read strategy (issue 47, host-testable half).
//
// The vectors here are the sensor's protocol, not this project's wire contract, so they
// live in the test rather than contracts/vectors/ — that directory is the C++/Python
// packet agreement, and putting a third-party device protocol in it would blur what the
// contract round-trip is actually pinning.
//
// The load-bearing vector is DFRobot's own worked example (FF 07 A1 A7 -> 1953 mm,
// https://wiki.dfrobot.com/sen0311/docs/21651). Every other frame in this file is built
// by FakeByteSource::pushFrame using the published checksum formula, so a mistake in that
// formula fails the golden vector first and loudly, rather than being baked consistently
// into both the code and its tests.

using namespace soundings;

void setUp() {}
void tearDown() {}

// A clock that ticks 1 ms per reading. Every loop in the driver escapes on a deadline, so
// without this the suite would hang instead of failing (fake_clock.h).
static void tickingClock(FakeClock& c) { c.autoAdvance(1); }

// Frame accounting — discard, median, range rejection, clamping — is tested with the
// settle window switched OFF, and that is deliberate rather than a convenience.
//
// FakeByteSource hands over a byte on every poll, so against a ticking clock the queued
// stream arrives roughly twenty-five times faster than a real 9600-baud sensor emitting a
// frame every ~100 ms. With the window left on, the settle period swallows the whole
// script and every one of these tests would fail for a reason that has nothing to do with
// what it is named for. Slowing the fake down to real cadence would only move the problem:
// the window would then end partway through some frame, one frame would be lost to
// resynchronisation, and each test's expected median would silently depend on exactly how
// many times the driver's loop happened to read the clock.
//
// So the window gets its own test, which proves it drops frames by contrast, and these
// tests get a config where it is not in play. One behaviour per test.
static A02yyuwConfig settledConfig() {
    A02yyuwConfig cfg;
    cfg.settleMs = 0;
    return cfg;
}

// ---- Frame parser ----------------------------------------------------------

void test_parser_accepts_dfrobot_worked_example() {
    A02yyuwFrameParser p;
    uint16_t mm = 0;
    TEST_ASSERT_FALSE(p.feed(0xFF, mm));
    TEST_ASSERT_FALSE(p.feed(0x07, mm));
    TEST_ASSERT_FALSE(p.feed(0xA1, mm));
    TEST_ASSERT_TRUE(p.feed(0xA7, mm));
    TEST_ASSERT_EQUAL_UINT16(1953, mm);
}

// The rail comes up mid-transmission: the first bytes the driver ever sees are the tail
// of a frame that started before it was listening.
void test_parser_resynchronises_past_leading_garbage() {
    A02yyuwFrameParser p;
    uint16_t mm = 0;
    p.feed(0xA1, mm);          // tail of a frame we missed the start of
    p.feed(0x3C, mm);
    TEST_ASSERT_FALSE(p.feed(0x07, mm));
    // Now a real frame arrives.
    p.feed(0xFF, mm); p.feed(0x04, mm); p.feed(0xB0, mm);
    TEST_ASSERT_TRUE(p.feed(0xB3, mm));
    TEST_ASSERT_EQUAL_UINT16(1200, mm);
}

void test_parser_drops_frame_with_bad_checksum() {
    A02yyuwFrameParser p;
    uint16_t mm = 0xDEAD;
    p.feed(0xFF, mm); p.feed(0x04, mm); p.feed(0xB0, mm);
    TEST_ASSERT_FALSE(p.feed(0xB4, mm));            // correct SUM is 0xB3
    TEST_ASSERT_EQUAL_UINT16(0xDEAD, mm);           // and mm is left untouched
    TEST_ASSERT_EQUAL_UINT16(1, p.badChecksums());
}

// The failure that matters: one damaged frame must cost one frame, not the rest of the
// stream. A parser that lost sync here would time out every read on a noisy cable.
void test_parser_recovers_on_the_frame_after_a_bad_checksum() {
    A02yyuwFrameParser p;
    uint16_t mm = 0;
    p.feed(0xFF, mm); p.feed(0x04, mm); p.feed(0xB0, mm); p.feed(0x00, mm);   // corrupt
    p.feed(0xFF, mm); p.feed(0x07, mm); p.feed(0xA1, mm);
    TEST_ASSERT_TRUE(p.feed(0xA7, mm));
    TEST_ASSERT_EQUAL_UINT16(1953, mm);
}

// 0xFF is the header AND a legal SUM value: 0xFF + 0x00 + 0x00 = 0xFF. A parser that
// treated every 0xFF as a header would resynchronise onto the checksum byte of a
// perfectly good frame and drop the one behind it.
void test_parser_does_not_resync_on_a_sum_byte_that_is_ff() {
    A02yyuwFrameParser p;
    uint16_t mm = 0;
    // Frame reading 0 mm, whose SUM is therefore 0xFF. Invalid as a distance — the
    // driver rejects it — but structurally a valid frame, which is the point here.
    p.feed(0xFF, mm); p.feed(0x00, mm); p.feed(0x00, mm);
    TEST_ASSERT_TRUE(p.feed(0xFF, mm));
    TEST_ASSERT_EQUAL_UINT16(0, mm);
    // The NEXT frame must still parse — sync was not consumed by that trailing 0xFF.
    p.feed(0xFF, mm); p.feed(0x04, mm); p.feed(0xB0, mm);
    TEST_ASSERT_TRUE(p.feed(0xB3, mm));
    TEST_ASSERT_EQUAL_UINT16(1200, mm);
}

// ---- Driver: read strategy -------------------------------------------------

void test_read_discards_the_first_three_frames() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    // Three frames the sensor emits while still waking up, then five real ones. If the
    // discard were not happening, the median would land among the 900s.
    bytes.pushFrame(900); bytes.pushFrame(901); bytes.pushFrame(902);
    bytes.pushFrame(1200); bytes.pushFrame(1205); bytes.pushFrame(1210);
    bytes.pushFrame(1215); bytes.pushFrame(1220);
    // Trailing noise the driver must never reach: once it has its five samples it stops,
    // rather than draining the UART. On hardware those bytes are the sensor still talking
    // into a rail that is about to be cut, and reading them is time the node spends awake.
    bytes.push(0xFF); bytes.push(0xFF); bytes.push(0xFF); bytes.push(0xFF);

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(1210, r.raw);
    TEST_ASSERT_EQUAL_INT(32, bytes.consumed());   // 8 frames exactly, and not a byte more
}

// Median, not mean — the whole reason the read strategy exists. One dropped echo reading
// max range drags a mean of these five to 1686 mm; the median never notices it.
void test_read_takes_the_median_not_the_mean() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    bytes.pushFrame(900); bytes.pushFrame(900); bytes.pushFrame(900);   // discarded
    bytes.pushFrame(1000); bytes.pushFrame(1005); bytes.pushFrame(1010);
    bytes.pushFrame(1015); bytes.pushFrame(4400);

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(1010, r.raw);
}

// Out-of-order arrival is the normal case for an outlier — it does not politely come last.
void test_read_median_is_order_independent() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    bytes.pushFrame(900); bytes.pushFrame(900); bytes.pushFrame(900);
    bytes.pushFrame(4400); bytes.pushFrame(1005); bytes.pushFrame(1000);
    bytes.pushFrame(1010); bytes.pushFrame(1015);

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    TEST_ASSERT_EQUAL_UINT16(1010, sensor.read().raw);
}

// A malformed frame must not count toward the five, and must not reach the reading.
void test_read_ignores_malformed_frames_and_still_completes() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    bytes.pushFrame(900); bytes.pushFrame(900); bytes.pushFrame(900);
    bytes.pushFrame(1000);
    bytes.pushFrameWithSum(4000, 0x00);      // corrupt; correct SUM is not 0x00
    bytes.pushFrame(1005); bytes.pushFrame(1010);
    bytes.pushFrameWithSum(4000, 0x01);      // corrupt again
    bytes.pushFrame(1015); bytes.pushFrame(1020);

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(1010, r.raw);
    TEST_ASSERT_EQUAL_UINT16(2, sensor.badChecksums());
}

// 0x0000 is what the sensor emits with nothing to say. It is checksum-valid, so only a
// range check catches it — and it is below the 30 mm blind zone.
void test_read_rejects_zero_and_out_of_range_frames() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    bytes.pushFrame(900); bytes.pushFrame(900); bytes.pushFrame(900);
    bytes.pushFrame(0);        // nothing to say
    bytes.pushFrame(1000);
    bytes.pushFrame(29);       // inside the 3 cm blind zone
    bytes.pushFrame(1005);
    bytes.pushFrame(4501);     // past the 450 cm maximum
    bytes.pushFrame(1010); bytes.pushFrame(1015); bytes.pushFrame(1020);

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(1010, r.raw);
}

// ---- Driver: rail and deadline ---------------------------------------------

void test_read_switches_the_rail_on_and_off_exactly_once() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    for (int i = 0; i < 8; ++i) bytes.pushFrame((uint16_t)(1200 + i));

    A02yyuwDistance sensor(bytes, rail, clock, settledConfig());
    TEST_ASSERT_FALSE(rail.isOn());
    // Asserted, because a read that timed out also leaves the rail down — without this
    // the test would go green on the failure path and never mention it.
    TEST_ASSERT_TRUE(sensor.read().ok);
    // Off again is the one that flattens a pack if it is wrong: the sensor free-runs for
    // as long as it has power (HARDWARE_BUILD_PLAN.md §6).
    TEST_ASSERT_FALSE(rail.isOn());
    TEST_ASSERT_EQUAL_INT(1, rail.onCount());
    TEST_ASSERT_EQUAL_INT(1, rail.offCount());
}

// A sensor that never answers must not hold the node awake. This is the test that hangs
// rather than fails if the deadline is missing — which is why FakeClock ticks.
//
// The last two assertions are not decoration. {0, false} is also what a driver that does
// nothing at all returns, so on its own this test passed against an empty stub — it was
// green while asserting nothing. Powering the rail and actually spending the deadline are
// what make the pass mean "timed out" rather than "never tried".
void test_read_times_out_on_a_silent_sensor() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;   // no bytes queued at all
    tickingClock(clock);

    A02yyuwDistance sensor(bytes, rail, clock);
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(0, r.raw);
    TEST_ASSERT_EQUAL_INT(1, rail.onCount());
    TEST_ASSERT_TRUE(clock.millis() >= A02yyuwConfig().deadlineMs);
}

// The rail must come down on the failure path too — that is the path where a stuck rail
// would go unnoticed, because the packet already says the channel is faulted.
void test_read_powers_the_rail_down_after_a_timeout() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    bytes.pushFrame(1200);   // one frame, then silence — never enough to complete

    A02yyuwDistance sensor(bytes, rail, clock);
    TEST_ASSERT_FALSE(sensor.read().ok);
    TEST_ASSERT_FALSE(rail.isOn());
    TEST_ASSERT_EQUAL_INT(1, rail.offCount());
}

// Frames arriving inside the settling window are dropped wholesale, before the discard
// count is even consulted — the sensor is not yet claimed to be telling the truth.
//
// Proved by contrast rather than by counting bytes across the boundary: the SAME stream
// is read twice, once with a settle window that outlasts the deadline and once with none.
// A test that tried to land the boundary between two specific frames would be asserting
// against the driver's clock-read cadence, which is not a thing anyone should be able to
// break by rearranging a loop.
void test_read_ignores_frames_arriving_inside_the_settle_window() {
    A02yyuwConfig swallowed;
    swallowed.settleMs = 5000;     // longer than deadlineMs — nothing is ever trusted
    {
        FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
        tickingClock(clock);
        for (int i = 0; i < 8; ++i) bytes.pushFrame((uint16_t)(1200 + i));
        A02yyuwDistance sensor(bytes, rail, clock, swallowed);
        TEST_ASSERT_FALSE(sensor.read().ok);
    }

    A02yyuwConfig immediate;
    immediate.settleMs = 0;
    {
        FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
        tickingClock(clock);
        for (int i = 0; i < 8; ++i) bytes.pushFrame((uint16_t)(1200 + i));
        A02yyuwDistance sensor(bytes, rail, clock, immediate);
        IDistance::Reading r = sensor.read();
        TEST_ASSERT_TRUE(r.ok);      // same stream, and now it completes
        TEST_ASSERT_EQUAL_UINT16(1205, r.raw);
    }
}

// ---- Driver: configuration -------------------------------------------------

void test_read_honours_a_configured_sample_count() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    A02yyuwConfig cfg = settledConfig();
    cfg.discardFrames = 1;
    cfg.sampleFrames  = 7;
    bytes.pushFrame(900);
    bytes.pushFrame(1000); bytes.pushFrame(1005); bytes.pushFrame(1010);
    bytes.pushFrame(1015); bytes.pushFrame(1020); bytes.pushFrame(1025);
    bytes.pushFrame(1030);

    A02yyuwDistance sensor(bytes, rail, clock, cfg);
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(1015, r.raw);   // median of seven
}

// A manifest or a bench tweak that asks for more samples than the fixed array holds must
// clamp, not overrun it. The node has no allocator and no way to report the corruption.
void test_read_clamps_a_sample_count_above_the_ceiling() {
    FakeByteSource bytes; FakePowerRail rail; FakeClock clock;
    tickingClock(clock);
    A02yyuwConfig cfg = settledConfig();
    cfg.discardFrames = 0;
    cfg.sampleFrames  = 200;
    for (int i = 0; i < 12; ++i) bytes.pushFrame((uint16_t)(1000 + i * 5));

    A02yyuwDistance sensor(bytes, rail, clock, cfg);
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    // Clamped to 7: frames 1000..1030, median 1015.
    TEST_ASSERT_EQUAL_UINT16(1015, r.raw);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_accepts_dfrobot_worked_example);
    RUN_TEST(test_parser_resynchronises_past_leading_garbage);
    RUN_TEST(test_parser_drops_frame_with_bad_checksum);
    RUN_TEST(test_parser_recovers_on_the_frame_after_a_bad_checksum);
    RUN_TEST(test_parser_does_not_resync_on_a_sum_byte_that_is_ff);
    RUN_TEST(test_read_discards_the_first_three_frames);
    RUN_TEST(test_read_takes_the_median_not_the_mean);
    RUN_TEST(test_read_median_is_order_independent);
    RUN_TEST(test_read_ignores_malformed_frames_and_still_completes);
    RUN_TEST(test_read_rejects_zero_and_out_of_range_frames);
    RUN_TEST(test_read_switches_the_rail_on_and_off_exactly_once);
    RUN_TEST(test_read_times_out_on_a_silent_sensor);
    RUN_TEST(test_read_powers_the_rail_down_after_a_timeout);
    RUN_TEST(test_read_ignores_frames_arriving_inside_the_settle_window);
    RUN_TEST(test_read_honours_a_configured_sample_count);
    RUN_TEST(test_read_clamps_a_sample_count_above_the_ceiling);
    return UNITY_END();
}
