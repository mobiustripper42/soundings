#include <unity.h>
#include "runcycle.h"
#include "distance_sampler.h"
#include "../fakes/fake_clock.h"
#include "../fakes/fake_distance.h"
#include "../fakes/fake_radio.h"
#include "../fakes/fake_battery.h"
#include "../fakes/fake_sleeper.h"
#include "../fakes/fake_random.h"
#include "../fakes/fake_seqstore.h"
#include "../fakes/fake_sampler.h"

// Phase 3.4 — the run cycle (issue #43). wake → sample → assemble → transmit → sleep.
//
// Every assertion here reads what the RADIO received, never the packet the cycle built,
// because the packet is the truth about what left the node and the struct is only what we
// believed. The cycle is terminal on hardware (deep sleep resets the MCU), so these tests
// call runOnce() repeatedly to model repeated BOOTS, not repeated loop iterations — the
// FakeSeqStore outliving each call is what makes that model honest.

using namespace soundings;

static const uint8_t kTankChannel = 8;   // TANK_DISTANCE, packet.h registry

void setUp() {}
void tearDown() {}

// A whole node's worth of fakes, wired the way the field node will be. Lives in one place
// so a test says what it is varying and stays silent about the rest.
struct Rig {
    FakeClock    clock;
    FakeDistance distance;
    FakeRadio    radio;
    FakeBattery  battery;
    FakeSleeper  sleeper{clock};
    FakeRandom   rng;
    FakeSeqStore seq;
    DistanceSampler sampler{distance};
    SensorSlot   slots[1] = {{kTankChannel, &sampler}};

    RunCycleConfig cfg() {
        RunCycleConfig c;
        c.node_id    = 7;
        c.fw_version = 0x0103;
        return c;
    }
    RunCycle cycle() {
        return RunCycle(cfg(), slots, 1, battery, radio, clock, sleeper, rng, seq);
    }
    // What the radio actually received, parsed back.
    ParseResult received(int i, Packet& out) {
        return deserialize(radio.frame(i), radio.frameLen(i), out);
    }
};

// ---- Assemble --------------------------------------------------------------

void test_cycle_transmits_one_frame_carrying_the_reading() {
    Rig r;
    r.distance.push(1347);
    r.battery.setReading(3810);
    r.rng.push(0);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT8(7, p.node_id);
    TEST_ASSERT_EQUAL_UINT16(0x0103, p.fw_version);
    TEST_ASSERT_EQUAL_UINT16(3810, p.battery_mv);
    TEST_ASSERT_TRUE(p.hasChannel(kTankChannel));
    TEST_ASSERT_FALSE(p.isFault(kTankChannel));
    TEST_ASSERT_EQUAL_UINT16(1347, p.channels[kTankChannel]);
}

// DEC-002: a declared sensor that does not answer is a FAULT, not a silent gap. The
// channel must still be declared, or the gateway cannot tell "broken" from "not fitted".
void test_failed_sensor_ships_as_declared_fault_not_missing_channel() {
    Rig r;
    r.distance.push(0, false);
    r.battery.setReading(3800);
    r.rng.push(0);

    RunCycle c = r.cycle();
    c.runOnce();

    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_TRUE(p.hasChannel(kTankChannel));
    TEST_ASSERT_TRUE(p.isFault(kTankChannel));
}

// The sensor data is the point of the packet; a battery read that fails must not suppress
// it. The node still transmits, with a zero it did not pretend to measure.
void test_failed_battery_read_still_transmits() {
    Rig r;
    r.distance.push(1200);
    r.battery.setReading(0, false);
    r.rng.push(0);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(0, p.battery_mv);
    TEST_ASSERT_EQUAL_UINT16(1200, p.channels[kTankChannel]);
}

// A node with nothing fitted is still worth hearing from: the header alone reports that it
// is alive and what its pack voltage is.
void test_zero_declared_sensors_still_sends_a_battery_only_frame() {
    Rig r;
    r.battery.setReading(3650);
    r.rng.push(0);

    RunCycleConfig cfg = r.cfg();
    RunCycle c(cfg, nullptr, 0, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(0, p.channel_mask);
    TEST_ASSERT_EQUAL_UINT16(3650, p.battery_mv);
}

void test_cycle_samples_every_declared_slot() {
    Rig r;
    FakeSampler a, b;
    a.push(111);
    b.push(222);
    SensorSlot slots[2] = {{0, &a}, {kTankChannel, &b}};   // 0 = SOIL_TENSION_0
    r.battery.setReading(3700);
    r.rng.push(0);

    RunCycleConfig cfg = r.cfg();
    RunCycle c(cfg, slots, 2, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, a.calls());
    TEST_ASSERT_EQUAL_INT(1, b.calls());
    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(111, p.channels[0]);
    TEST_ASSERT_EQUAL_UINT16(222, p.channels[kTankChannel]);
}

// ---- Sequence --------------------------------------------------------------

void test_seq_increments_across_cycles() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);

    RunCycle c1 = r.cycle(); c1.runOnce();
    RunCycle c2 = r.cycle(); c2.runOnce();
    RunCycle c3 = r.cycle(); c3.runOnce();

    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(0, p.seq);
    TEST_ASSERT_TRUE(r.received(1, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(1, p.seq);
    TEST_ASSERT_TRUE(r.received(2, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(2, p.seq);
}

// The whole reason ISeqStore exists: deep sleep resets the MCU, so a counter in a member
// variable would ship 0 forever. Constructing a fresh RunCycle around the SAME store is
// what a wake actually looks like.
void test_seq_survives_a_simulated_reset() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);

    { RunCycle c = r.cycle(); c.runOnce(); }    // boot 1, then "deep sleep"
    { RunCycle c = r.cycle(); c.runOnce(); }    // boot 2 — brand new object, same store

    Packet p;
    TEST_ASSERT_TRUE(r.received(1, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(1, p.seq);
    TEST_ASSERT_EQUAL_UINT16(2, r.seq.load());
}

// 65535 → 0 with no gap and no stall. A saturating counter would look identical for the
// first 65535 packets and then quietly stop being a sequence.
void test_seq_wraps_at_uint16_max_without_a_gap() {
    Rig r;
    r.seq.store(0xFFFF);
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);

    { RunCycle c = r.cycle(); c.runOnce(); }
    { RunCycle c = r.cycle(); c.runOnce(); }

    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, p.seq);
    TEST_ASSERT_TRUE(r.received(1, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT16(0, p.seq);
}

// ---- Jitter and sleep ------------------------------------------------------

// Pinned at both extremes and the midpoint, not sampled for "somewhere in range" — a
// jitter calculation that collapses to a constant passes a range check every time.
void test_jitter_low_extreme_is_interval_minus_full_jitter() {
    Rig r;
    r.rng.push(0);                       // 0 => the bottom of the window
    RunCycle c = r.cycle();
    TEST_ASSERT_EQUAL_UINT32(kDefaultIntervalMs - kDefaultJitterMs, c.nextSleepMs());
}

void test_jitter_high_extreme_is_interval_plus_full_jitter() {
    Rig r;
    r.rng.push(2 * kDefaultJitterMs);    // the top of the window
    RunCycle c = r.cycle();
    TEST_ASSERT_EQUAL_UINT32(kDefaultIntervalMs + kDefaultJitterMs, c.nextSleepMs());
}

void test_jitter_midpoint_is_the_bare_interval() {
    Rig r;
    r.rng.push(kDefaultJitterMs);
    RunCycle c = r.cycle();
    TEST_ASSERT_EQUAL_UINT32(kDefaultIntervalMs, c.nextSleepMs());
}

// An RNG returning a huge value must not escape the window — the modulus, not the raw
// value, is what bounds it, and getting that wrong is how one node ends up sleeping for
// an hour.
void test_jitter_never_escapes_the_window_for_any_rng_value() {
    Rig r;
    r.rng.push(0xFFFFFFFFu);
    RunCycle c = r.cycle();
    uint32_t ms = c.nextSleepMs();
    TEST_ASSERT_TRUE(ms >= kDefaultIntervalMs - kDefaultJitterMs);
    TEST_ASSERT_TRUE(ms <= kDefaultIntervalMs + kDefaultJitterMs);
}

// jitter > interval underflows `interval - jitter` as unsigned and yields a sleep of
// roughly 49 days. On a node whose sleep is TERMINAL that is not a long nap, it is a
// brick until someone walks out and pulls the cell. The invariant is clamped rather than
// asserted: a misconfigured node that wakes too often is recoverable over the air, and
// one that aborts on boot is a reflash in a tunnel.
void test_jitter_larger_than_interval_cannot_underflow_the_sleep() {
    Rig r;
    r.rng.push(0);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 5000;          // nonsense, and must stay merely wrong
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_EQUAL_UINT32(0, c.nextSleepMs());
}

void test_jitter_larger_than_interval_stays_bounded_for_any_rng_value() {
    Rig r;
    r.rng.push(0xFFFFFFFFu);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 5000;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_TRUE(c.nextSleepMs() <= 2u * cfg.intervalMs);
}

// The exact boundary, where the clamp is a no-op and the low extreme is zero.
void test_jitter_equal_to_interval_is_the_boundary_and_does_not_wrap() {
    Rig r;
    r.rng.push(0);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 1000;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_EQUAL_UINT32(0, c.nextSleepMs());
}

void test_cycle_sleeps_exactly_once_for_the_jittered_interval() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
    TEST_ASSERT_EQUAL_UINT32(kDefaultIntervalMs - kDefaultJitterMs, r.sleeper.lastRequested());
}

// Two nodes that boot at the same instant must not wake together forever — the point of
// jitter. Different entropy, different sleep.
void test_two_nodes_with_different_entropy_sleep_differently() {
    Rig a, b;
    a.rng.push(0);
    b.rng.push(2 * kDefaultJitterMs);
    RunCycle ca = a.cycle();
    RunCycle cb = b.cycle();
    TEST_ASSERT_TRUE(ca.nextSleepMs() != cb.nextSleepMs());
}

// ---- Transmit failure ------------------------------------------------------

// Busy is retryable — the radio is mid-something, not broken.
void test_busy_radio_is_retried_up_to_the_cap() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);
    r.radio.setResult(IRadio::TxResult::Busy);

    RunCycleConfig cfg = r.cfg();
    cfg.txRetries = 3;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    c.runOnce();

    // 1 initial attempt + 3 retries.
    TEST_ASSERT_EQUAL_INT(4, r.radio.sentCount());
}

// Failed is not retryable — retrying a broken radio burns the budget that DEC-006 says is
// actually at risk: a node stuck awake.
void test_failed_radio_is_not_retried() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);
    r.radio.setResult(IRadio::TxResult::Failed);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
}

// Whatever the radio did, the node sleeps. A cycle that returns without sleeping is a
// node that stays awake until the pack is flat.
void test_node_sleeps_even_when_transmit_fails() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);
    r.radio.setResult(IRadio::TxResult::Failed);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
}

// The retry spin is bounded by a wall-clock window as well as a count, so a radio that
// says Busy forever cannot hold the cycle open even if the cap were raised.
void test_busy_retries_stop_at_the_window_even_with_a_high_cap() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);
    r.radio.setResult(IRadio::TxResult::Busy);

    RunCycleConfig cfg = r.cfg();
    cfg.txRetries  = 200;         // far more than the frame buffer or the window allows
    cfg.txWindowMs = 0;           // window is already expired on the first check
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
}

// A successful send stops immediately — no gratuitous second transmission, which would be
// double the airtime and a duplicate row downstream.
void test_successful_send_is_not_retried() {
    Rig r;
    r.distance.push(1000);
    r.battery.setReading(3800);
    r.rng.push(0);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    TEST_ASSERT_FALSE(r.radio.overflowed());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cycle_transmits_one_frame_carrying_the_reading);
    RUN_TEST(test_failed_sensor_ships_as_declared_fault_not_missing_channel);
    RUN_TEST(test_failed_battery_read_still_transmits);
    RUN_TEST(test_zero_declared_sensors_still_sends_a_battery_only_frame);
    RUN_TEST(test_cycle_samples_every_declared_slot);
    RUN_TEST(test_seq_increments_across_cycles);
    RUN_TEST(test_seq_survives_a_simulated_reset);
    RUN_TEST(test_seq_wraps_at_uint16_max_without_a_gap);
    RUN_TEST(test_jitter_low_extreme_is_interval_minus_full_jitter);
    RUN_TEST(test_jitter_high_extreme_is_interval_plus_full_jitter);
    RUN_TEST(test_jitter_midpoint_is_the_bare_interval);
    RUN_TEST(test_jitter_never_escapes_the_window_for_any_rng_value);
    RUN_TEST(test_jitter_larger_than_interval_cannot_underflow_the_sleep);
    RUN_TEST(test_jitter_larger_than_interval_stays_bounded_for_any_rng_value);
    RUN_TEST(test_jitter_equal_to_interval_is_the_boundary_and_does_not_wrap);
    RUN_TEST(test_cycle_sleeps_exactly_once_for_the_jittered_interval);
    RUN_TEST(test_two_nodes_with_different_entropy_sleep_differently);
    RUN_TEST(test_busy_radio_is_retried_up_to_the_cap);
    RUN_TEST(test_failed_radio_is_not_retried);
    RUN_TEST(test_node_sleeps_even_when_transmit_fails);
    RUN_TEST(test_busy_retries_stop_at_the_window_even_with_a_high_cap);
    RUN_TEST(test_successful_send_is_not_retried);
    return UNITY_END();
}
