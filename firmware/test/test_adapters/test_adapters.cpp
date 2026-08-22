#include <unity.h>
#include <string.h>
#include "packet.h"
#include "../fakes/fake_clock.h"
#include "../fakes/fake_distance.h"
#include "../fakes/fake_radio.h"
#include "../fakes/fake_battery.h"
#include "../fakes/fake_sleeper.h"
#include "../fakes/fake_bytesource.h"
#include "../fakes/fake_powerrail.h"

// Phase 3.3 — the tank-node adapter seam (issue 42). These tests grade the FAKES, because
// the fakes are the deliverable that everything downstream (3.4's run cycle, 3.5's
// manifest) is written against: a fake that quietly lies makes every test above it a
// green light for nothing. The real drivers land in 3.8/3.9/3.10 behind these same
// interfaces.

using namespace soundings;

void setUp() {}
void tearDown() {}

// ---- IDistance -------------------------------------------------------------

void test_fake_distance_returns_scripted_readings_in_order() {
    FakeDistance sensor;
    sensor.push(1200);
    sensor.push(1185);
    TEST_ASSERT_EQUAL_UINT16(1200, sensor.read().raw);
    TEST_ASSERT_EQUAL_UINT16(1185, sensor.read().raw);
}

void test_fake_distance_signals_failed_read() {
    FakeDistance sensor;
    sensor.push(0, false);
    IDistance::Reading r = sensor.read();
    TEST_ASSERT_FALSE(r.ok);
}

// A run cycle that samples more often than the test scripted readings must not silently
// wrap back to the first value — that would fabricate a plausible drydown out of nothing.
void test_fake_distance_repeats_last_reading_when_script_exhausted() {
    FakeDistance sensor;
    sensor.push(900);
    sensor.push(880);
    sensor.read();
    sensor.read();
    TEST_ASSERT_EQUAL_UINT16(880, sensor.read().raw);
    TEST_ASSERT_EQUAL_UINT16(880, sensor.read().raw);
}

// An unscripted fake reads as a failure, not as zero millimetres. Zero is a legal-looking
// distance, so defaulting to it would let a test that forgot to script the sensor pass.
void test_fake_distance_unscripted_read_is_a_failure() {
    FakeDistance sensor;
    TEST_ASSERT_FALSE(sensor.read().ok);
}

// ---- IRadio ----------------------------------------------------------------

void test_fake_radio_records_sent_frame_byte_exactly() {
    FakeRadio radio;
    const uint8_t frame[] = {0x01, 0x02, 0xFF, 0x00, 0x7E};
    TEST_ASSERT_TRUE(radio.send(frame, sizeof(frame)) == IRadio::TxResult::Ok);
    TEST_ASSERT_EQUAL_INT(1, radio.sentCount());
    TEST_ASSERT_EQUAL_size_t(sizeof(frame), radio.frameLen(0));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, radio.frame(0), sizeof(frame));
}

void test_fake_radio_records_multiple_frames_in_order() {
    FakeRadio radio;
    const uint8_t a[] = {0xAA};
    const uint8_t b[] = {0xBB, 0xCC};
    radio.send(a, sizeof(a));
    radio.send(b, sizeof(b));
    TEST_ASSERT_EQUAL_INT(2, radio.sentCount());
    TEST_ASSERT_EQUAL_UINT8(0xAA, radio.frame(0)[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, radio.frame(1)[0]);
    TEST_ASSERT_EQUAL_size_t(2, radio.frameLen(1));
}

// Busy and Failed stay distinct through the seam — a caller that retries on Busy and
// faults on Failed can only be tested if the fake can produce each one.
void test_fake_radio_returns_scripted_tx_result() {
    FakeRadio radio;
    const uint8_t frame[] = {0x01};
    radio.setResult(IRadio::TxResult::Busy);
    TEST_ASSERT_TRUE(radio.send(frame, sizeof(frame)) == IRadio::TxResult::Busy);
    radio.setResult(IRadio::TxResult::Failed);
    TEST_ASSERT_TRUE(radio.send(frame, sizeof(frame)) == IRadio::TxResult::Failed);
}

// A frame the radio refused still happened as far as the caller is concerned, so it is
// recorded — a test asserting "we did not transmit" needs the count to mean attempts.
void test_fake_radio_records_frames_it_failed_to_send() {
    FakeRadio radio;
    const uint8_t frame[] = {0x01};
    radio.setResult(IRadio::TxResult::Failed);
    radio.send(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_INT(1, radio.sentCount());
}

// A fake with a fixed frame buffer that silently discards past capacity would let a
// 3.4 run-cycle test "send" a hundred frames and assert against the first eight. The
// overflow has to be visible, or the fake lies by omission.
void test_fake_radio_flags_capacity_overflow() {
    FakeRadio radio;
    const uint8_t frame[] = {0x01};
    TEST_ASSERT_FALSE(radio.overflowed());
    for (int i = 0; i < FakeRadio::kCapacity + 1; ++i) radio.send(frame, sizeof(frame));
    TEST_ASSERT_TRUE(radio.overflowed());
    TEST_ASSERT_EQUAL_INT(FakeRadio::kCapacity + 1, radio.sentCount());
}

// ---- IBattery --------------------------------------------------------------

void test_fake_battery_returns_set_millivolts() {
    FakeBattery battery;
    battery.setReading(3720);
    IBattery::Reading r = battery.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(3720, r.millivolts);
}

void test_fake_battery_signals_failed_read() {
    FakeBattery battery;
    battery.setReading(0, false);
    TEST_ASSERT_FALSE(battery.read().ok);
}

// Same reason as the distance seam: 0 mV is a legal-looking reading, so an unscripted
// battery must read as a failure rather than as a flat pack.
void test_fake_battery_unscripted_read_is_a_failure() {
    FakeBattery battery;
    TEST_ASSERT_FALSE(battery.read().ok);
}

// ---- ISleeper --------------------------------------------------------------

void test_fake_sleeper_records_requested_interval() {
    FakeClock clock;
    FakeSleeper sleeper(clock);
    sleeper.sleepFor(900000);
    TEST_ASSERT_EQUAL_UINT32(900000, sleeper.lastRequested());
    TEST_ASSERT_EQUAL_INT(1, sleeper.sleepCount());
}

// Sleeping must move the clock, or a scheduled run cycle would sample the same instant
// forever and its jitter window could never be observed.
void test_fake_sleeper_advances_the_clock() {
    FakeClock clock;
    FakeSleeper sleeper(clock);
    clock.set(5000);
    sleeper.sleepFor(1500);
    TEST_ASSERT_EQUAL_UINT32(6500, clock.millis());
}

void test_fake_sleeper_accumulates_total_slept() {
    FakeClock clock;
    FakeSleeper sleeper(clock);
    sleeper.sleepFor(1000);
    sleeper.sleepFor(2000);
    TEST_ASSERT_EQUAL_UINT32(3000, sleeper.totalSlept());
    TEST_ASSERT_EQUAL_UINT32(2000, sleeper.lastRequested());
    TEST_ASSERT_EQUAL_INT(2, sleeper.sleepCount());
}

// ---- Composition -----------------------------------------------------------

// The one test that proves the seams are usable TOGETHER: a scripted distance goes
// sensor → channel 8 → serialize → radio, and the bytes the radio actually caught
// deserialize back to the same value. No scheduling, no jitter, no sleep — that is 3.4.
void test_distance_reading_survives_serialize_transmit_deserialize() {
    FakeDistance sensor;
    FakeBattery  battery;
    FakeRadio    radio;
    sensor.push(1347);
    battery.setReading(3810);

    IDistance::Reading d = sensor.read();
    IBattery::Reading  b = battery.read();
    TEST_ASSERT_TRUE(d.ok);
    TEST_ASSERT_TRUE(b.ok);

    Packet tx;
    tx.node_id    = 7;
    tx.seq        = 42;
    tx.battery_mv = b.millivolts;
    tx.setChannel(8, d.raw);          // channel 8 = TANK_DISTANCE (packet.h registry)

    uint8_t buf[kMaxPacketLen];
    size_t  n = serialize(tx, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_TRUE(radio.send(buf, n) == IRadio::TxResult::Ok);

    // Deserialize what the RADIO received, not the buffer we still hold — otherwise the
    // seam is untested and this is just a packet round-trip test wearing a costume.
    Packet rx;
    TEST_ASSERT_TRUE(deserialize(radio.frame(0), radio.frameLen(0), rx) == ParseResult::Ok);
    TEST_ASSERT_TRUE(rx.hasChannel(8));
    TEST_ASSERT_FALSE(rx.isFault(8));
    TEST_ASSERT_EQUAL_UINT16(1347, rx.channels[8]);
    TEST_ASSERT_EQUAL_UINT16(3810, rx.battery_mv);
    TEST_ASSERT_EQUAL_UINT8(7, rx.node_id);
}

// A declared sensor that fails to answer is a fault bit, never a missing channel
// (DEC-002). The seam has to make that expressible, so the composition covers it.
void test_failed_distance_read_rides_as_a_declared_fault() {
    FakeDistance sensor;
    FakeRadio    radio;
    sensor.push(0, false);

    IDistance::Reading d = sensor.read();
    TEST_ASSERT_FALSE(d.ok);

    Packet tx;
    tx.node_id = 7;
    tx.setChannel(8, 0);
    if (!d.ok) tx.setFault(8);

    uint8_t buf[kMaxPacketLen];
    size_t  n = serialize(tx, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    radio.send(buf, n);

    Packet rx;
    TEST_ASSERT_TRUE(deserialize(radio.frame(0), radio.frameLen(0), rx) == ParseResult::Ok);
    TEST_ASSERT_TRUE(rx.hasChannel(8));
    TEST_ASSERT_TRUE(rx.isFault(8));
}

// ---- IByteSource / IPowerRail (Phase 3.8a) ---------------------------------

void test_fake_bytesource_delivers_pushed_bytes_in_order() {
    FakeByteSource src;
    src.push(0xFF); src.push(0x07);
    uint8_t b = 0;
    TEST_ASSERT_TRUE(src.readByte(b));  TEST_ASSERT_EQUAL_UINT8(0xFF, b);
    TEST_ASSERT_TRUE(src.readByte(b));  TEST_ASSERT_EQUAL_UINT8(0x07, b);
}

// Exhausted must read as "nothing right now", not as an error and not as a zero byte —
// that is what a free-running sensor's UART looks like between frames, and a fake that
// returned 0x00 instead would feed the parser a byte the sensor never sent.
void test_fake_bytesource_reports_empty_without_writing_the_byte() {
    FakeByteSource src;
    uint8_t b = 0x5A;
    TEST_ASSERT_FALSE(src.readByte(b));
    TEST_ASSERT_EQUAL_UINT8(0x5A, b);
}

// pushFrame is the helper every driver test leans on, so its checksum arithmetic is
// graded here against DFRobot's published worked example rather than trusted.
void test_fake_bytesource_pushframe_matches_the_published_vector() {
    FakeByteSource src;
    src.pushFrame(1953);
    const uint8_t want[4] = {0xFF, 0x07, 0xA1, 0xA7};
    for (int i = 0; i < 4; ++i) {
        uint8_t b = 0;
        TEST_ASSERT_TRUE(src.readByte(b));
        TEST_ASSERT_EQUAL_UINT8(want[i], b);
    }
}

void test_fake_powerrail_tracks_state_and_transitions() {
    FakePowerRail rail;
    TEST_ASSERT_FALSE(rail.isOn());
    rail.on();  rail.on();          // idempotent: a second on() is not a second transition
    TEST_ASSERT_TRUE(rail.isOn());
    TEST_ASSERT_EQUAL_INT(1, rail.onCount());
    rail.off();
    TEST_ASSERT_FALSE(rail.isOn());
    TEST_ASSERT_EQUAL_INT(1, rail.offCount());
}

// autoAdvance is opt-in, and the default has to stay frozen — every existing test in this
// repo assumes time moves only when it says so.
void test_fake_clock_does_not_advance_unless_asked() {
    FakeClock c;
    c.millis(); c.millis();
    TEST_ASSERT_EQUAL_UINT32(0, c.millis());

    c.autoAdvance(5);
    TEST_ASSERT_EQUAL_UINT32(0, c.millis());   // returns the instant BEFORE the tick
    TEST_ASSERT_EQUAL_UINT32(5, c.millis());
    TEST_ASSERT_EQUAL_UINT32(10, c.millis());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fake_distance_returns_scripted_readings_in_order);
    RUN_TEST(test_fake_distance_signals_failed_read);
    RUN_TEST(test_fake_distance_repeats_last_reading_when_script_exhausted);
    RUN_TEST(test_fake_distance_unscripted_read_is_a_failure);
    RUN_TEST(test_fake_radio_records_sent_frame_byte_exactly);
    RUN_TEST(test_fake_radio_records_multiple_frames_in_order);
    RUN_TEST(test_fake_radio_returns_scripted_tx_result);
    RUN_TEST(test_fake_radio_records_frames_it_failed_to_send);
    RUN_TEST(test_fake_radio_flags_capacity_overflow);
    RUN_TEST(test_fake_battery_returns_set_millivolts);
    RUN_TEST(test_fake_battery_signals_failed_read);
    RUN_TEST(test_fake_battery_unscripted_read_is_a_failure);
    RUN_TEST(test_fake_sleeper_records_requested_interval);
    RUN_TEST(test_fake_sleeper_advances_the_clock);
    RUN_TEST(test_fake_sleeper_accumulates_total_slept);
    RUN_TEST(test_distance_reading_survives_serialize_transmit_deserialize);
    RUN_TEST(test_failed_distance_read_rides_as_a_declared_fault);
    RUN_TEST(test_fake_bytesource_delivers_pushed_bytes_in_order);
    RUN_TEST(test_fake_bytesource_reports_empty_without_writing_the_byte);
    RUN_TEST(test_fake_bytesource_pushframe_matches_the_published_vector);
    RUN_TEST(test_fake_powerrail_tracks_state_and_transitions);
    RUN_TEST(test_fake_clock_does_not_advance_unless_asked);
    return UNITY_END();
}
