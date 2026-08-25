#include <unity.h>
#include "runcycle.h"
#include "downlink.h"
#include "distance_sampler.h"
#include "../fakes/fake_clock.h"
#include "../fakes/fake_distance.h"
#include "../fakes/fake_radio.h"
#include "../fakes/fake_battery.h"
#include "../fakes/fake_sleeper.h"
#include "../fakes/fake_random.h"
#include "../fakes/fake_seqstore.h"
#include "../fakes/fake_sampler.h"
#include "../fakes/fake_downlinkhandler.h"

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
    RunCycle cycleWith(IDownlinkHandler& h) {
        return RunCycle(cfg(), slots, 1, battery, radio, clock, sleeper, rng, seq, &h);
    }
    // Queue a valid downlink for the window this cycle will hold.
    void pushDownlink(uint8_t node_id, uint16_t flags) {
        Downlink d;
        d.node_id = node_id;
        d.flags   = flags;
        uint8_t raw[kDownlinkLen] = {};
        encodeDownlink(d, raw, sizeof(raw));
        radio.pushReceive(raw, sizeof(raw));
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
// ⚠ These three assert an EXACT non-zero sleep, and the reason is not stylistic.
//
// Each of them originally pinned a sleep of 0 — arithmetically correct at the low end of
// the window, and also exactly what `nextSleepMs() { return 0; }` returns. Verified by
// mutation: with that body substituted, all three passed, and so did the entire
// test_manifest suite. The clamp that stops a node bricking itself for 49 days was not
// pinned by anything.
//
// A sleep of 0 is not a harmless assertion to land on, either — it means the node never
// sleeps, which is the wake-forever failure DEC-006 names as the actual battery killer.
// The one value these tests agreed on was a value that would have been a disaster.
//
// So: drive the RNG to a value whose correct answer is distinctive, and assert it. An
// implementation that returns 0, and one that underflows, now both fail.
void test_jitter_larger_than_interval_cannot_underflow_the_sleep() {
    Rig r;
    r.rng.push(1500);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 5000;          // nonsense, and must stay merely wrong
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    // jitter clamps to 1000, so span = 2001 and offset = 1500 % 2001 = 1500.
    // Unclamped this would be 1000 - 5000 + 1500, i.e. ~49 days of unsigned underflow.
    TEST_ASSERT_EQUAL_UINT32(1500, c.nextSleepMs());
}

void test_jitter_larger_than_interval_stays_bounded_for_any_rng_value() {
    Rig r;
    r.rng.push(0xFFFFFFFFu);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 5000;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    const uint32_t ms = c.nextSleepMs();
    // The bound is the property under test, but a bound alone is satisfied by 0 — which
    // is how this one survived the mutation. 0xFFFFFFFF % 2001 = 885, so the exact answer
    // is stated beside the bound and the bound keeps its meaning.
    TEST_ASSERT_EQUAL_UINT32(885, ms);
    TEST_ASSERT_TRUE(ms <= 2u * cfg.intervalMs);
}

// The exact boundary, where the clamp is a no-op. Two draws, because the low extreme here
// genuinely IS zero and a single assertion of it says nothing — the second draw is what
// distinguishes a working window from a stuck one.
void test_jitter_equal_to_interval_is_the_boundary_and_does_not_wrap() {
    Rig r;
    r.rng.push(0);
    r.rng.push(2000);
    RunCycleConfig cfg = r.cfg();
    cfg.intervalMs = 1000;
    cfg.jitterMs   = 1000;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_EQUAL_UINT32(0,    c.nextSleepMs());   // low extreme, and it does not wrap
    TEST_ASSERT_EQUAL_UINT32(2000, c.nextSleepMs());   // high extreme of the same window
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

// ---- Receive window (3.9b) -------------------------------------------------

// The node is asleep ~99% of the time, so the only moment the gateway can tell it
// anything is one the node offers itself (DEC-010). That moment is here, after transmit.
void test_cycle_holds_a_receive_window_after_transmitting() {
    Rig r;
    r.rng.push(0);
    RunCycle c = r.cycle();
    c.runOnce();
    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    TEST_ASSERT_EQUAL_INT(1, r.radio.receiveCalls());
    TEST_ASSERT_EQUAL_UINT32(kDefaultRxWindowMs, r.radio.lastTimeoutMs());
}

// Silence is the ordinary answer, not a fault. Nothing about the cycle may change.
void test_a_quiet_window_is_not_a_fault() {
    Rig r;
    r.rng.push(0);
    r.distance.push(1200);       // an unscripted FakeDistance is a deliberate failure
    RunCycle c = r.cycle();
    c.runOnce();                                 // nothing queued to receive
    // A QUIET window is still a window. Without this the test passes against a cycle
    // that never listens, which is not the thing being claimed.
    TEST_ASSERT_EQUAL_INT(1, r.radio.receiveCalls());
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
    Packet p;
    TEST_ASSERT_TRUE(r.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_FALSE(p.isFault(kTankChannel));  // the distance read was fine
}

// A downlink addressed to this node is accepted and its flags surface. v1 assigns no
// bits, so nothing acts on them yet — issue #76 does. What is pinned here is that the
// path works end to end, because that is what OTA will be built on.
void test_a_downlink_for_this_node_is_received_and_decoded() {
    Rig r;
    r.rng.push(0);
    Downlink d;
    d.node_id = 7;                               // Rig::cfg() sets node_id 7
    d.flags   = 0x0005;
    uint8_t raw[kDownlinkLen] = {};
    TEST_ASSERT_EQUAL_UINT32(kDownlinkLen, encodeDownlink(d, raw, sizeof(raw)));
    r.radio.pushReceive(raw, sizeof(raw));

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_TRUE(c.lastDownlinkValid());
    TEST_ASSERT_EQUAL_UINT16(0x0005, c.lastDownlink().flags);
}

// Two nodes in earshot both hear both replies. Dropping someone else's is the common
// case, and it must be silent — no fault bit, no retry, nothing.
void test_a_downlink_for_another_node_is_ignored_silently() {
    Rig r;
    r.rng.push(0);
    Downlink d;
    d.node_id = 9;                               // not us
    d.flags   = 0x0005;
    uint8_t raw[kDownlinkLen] = {};
    encodeDownlink(d, raw, sizeof(raw));
    r.radio.pushReceive(raw, sizeof(raw));

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_FALSE(c.lastDownlinkValid());
    TEST_ASSERT_EQUAL_INT(1, r.radio.receiveCalls());   // we DID listen — and dropped it
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());   // and it still slept normally
}

void test_a_corrupt_downlink_is_ignored_and_the_cycle_completes() {
    Rig r;
    r.rng.push(0);
    Downlink d;
    d.node_id = 7;
    uint8_t raw[kDownlinkLen] = {};
    encodeDownlink(d, raw, sizeof(raw));
    raw[4] ^= 0xFF;                              // wreck the CRC
    r.radio.pushReceive(raw, sizeof(raw));

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_FALSE(c.lastDownlinkValid());
    TEST_ASSERT_EQUAL_INT(1, r.radio.receiveCalls());   // listened, then dropped it
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
}

// The window must not become a way to stay awake. A configured 0 means "do not listen",
// and the radio is not touched at all.
void test_a_zero_window_skips_the_listen_entirely() {
    Rig r;
    r.rng.push(0);
    RunCycleConfig cfg = r.cfg();
    cfg.rxWindowMs = 0;
    RunCycle c(cfg, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    c.runOnce();
    TEST_ASSERT_EQUAL_INT(0, r.radio.receiveCalls());
    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());      // but it still transmitted

    // Contrast: the default window does listen, on an otherwise identical rig.
    Rig d;
    d.rng.push(0);
    RunCycle dc = d.cycle();
    dc.runOnce();
    TEST_ASSERT_EQUAL_INT(1, d.radio.receiveCalls());
}

// A packet that never went out earns no window — listening after a failed transmit is
// airtime spent on a reply to something nobody heard.
void test_no_window_is_held_when_the_transmit_failed() {
    Rig r;
    r.rng.push(0);
    r.radio.setResult(IRadio::TxResult::Failed);
    RunCycle c = r.cycle();
    c.runOnce();
    TEST_ASSERT_EQUAL_INT(0, r.radio.receiveCalls());
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());   // and it still sleeps

    // Contrast: a transmit that succeeds does earn a window.
    Rig ok;
    ok.rng.push(0);
    RunCycle okc = ok.cycle();
    okc.runOnce();
    TEST_ASSERT_EQUAL_INT(1, ok.radio.receiveCalls());
}

// ---- The downlink handler (issue #76) --------------------------------------
//
// The seam the OTA client binds to. RunCycle decides WHETHER a downlink is real and
// addressed to us; the handler decides what to do about it. These tests pin the
// whether, because that is the part that stays in core.

void test_a_valid_downlink_reaches_the_handler() {
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);
    r.pushDownlink(7, 0x0001);

    FakeDownlinkHandler h(r.sleeper);
    RunCycle c = r.cycleWith(h);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, h.calls());
    // The flags arrive intact — the handler's whole input is this field.
    TEST_ASSERT_EQUAL_UINT16(0x0001, h.last().flags);
    TEST_ASSERT_EQUAL_UINT8(7, h.last().node_id);
}

void test_the_handler_runs_before_the_node_sleeps() {
    // On hardware the sleep RESETS THE MCU, so a handler invoked after it would never
    // run in the field while passing every host test — FakeSleeper returns. runcycle.h
    // warns about exactly this shape in capitals; this is the assertion behind the words.
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);
    r.pushDownlink(7, 0x0001);

    FakeDownlinkHandler h(r.sleeper);
    RunCycle c = r.cycleWith(h);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, h.calls());
    TEST_ASSERT_EQUAL_INT(0, h.sleepsWhenCalled());   // no sleep had happened yet
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount()); // and one has by now
}

void test_a_quiet_window_does_not_call_the_handler() {
    // The ordinary case: nothing queued, so the window hears silence. Not a fault, and
    // nothing for the handler to do.
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);

    FakeDownlinkHandler h(r.sleeper);
    RunCycle c = r.cycleWith(h);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(0, h.calls());

    // POSITIVE CONTROL, same test. Without it this passes against a cycle that never
    // calls the handler at all — and "asserts a zero" is the false-green shape this repo
    // keeps producing (nineteen found in the Task 3 audit, five more written after it).
    Rig r2;
    r2.distance.push(1200);
    r2.rng.push(0);
    r2.pushDownlink(7, 0x0002);
    FakeDownlinkHandler h2(r2.sleeper);
    RunCycle c2 = r2.cycleWith(h2);
    c2.runOnce();
    TEST_ASSERT_EQUAL_INT(1, h2.calls());
}

void test_a_downlink_for_another_node_does_not_call_the_handler() {
    // Two nodes in earshot both hear both replies, so this is the COMMON rejection
    // rather than a rare error (contracts/downlink-v1.md step 4).
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);
    r.pushDownlink(9, 0x0001);   // node 9; the rig is node 7

    FakeDownlinkHandler h(r.sleeper);
    RunCycle c = r.cycleWith(h);
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(0, h.calls());
    // ...and the cycle still completed normally rather than treating it as a fault.
    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());

    // Positive control: the identical downlink addressed to US does reach the handler.
    Rig r2;
    r2.distance.push(1200);
    r2.rng.push(0);
    r2.pushDownlink(7, 0x0001);
    FakeDownlinkHandler h2(r2.sleeper);
    RunCycle c2 = r2.cycleWith(h2);
    c2.runOnce();
    TEST_ASSERT_EQUAL_INT(1, h2.calls());
}

void test_a_cycle_with_no_handler_still_completes() {
    // The handler is optional. Every existing caller passes none, and a node with no OTA
    // client bound must transmit and sleep exactly as before.
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);
    r.pushDownlink(7, 0x0001);

    RunCycle c = r.cycle();
    c.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());
    TEST_ASSERT_EQUAL_INT(1, r.sleeper.sleepCount());
    TEST_ASSERT_TRUE(c.lastDownlinkValid());   // heard it; simply had nowhere to send it
}

// ---- Listening on every Nth wake (issue #79) --------------------------------

void test_n_of_one_listens_on_every_wake() {
    Rig r;
    RunCycleConfig c = r.cfg();
    c.rxEveryNWakes = 1;
    for (uint16_t s = 0; s < 5; ++s) {
        r.seq.store(s);
        RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
        TEST_ASSERT_TRUE(cycle.shouldListenThisWake());
    }
}

void test_n_of_twelve_listens_on_one_wake_in_twelve() {
    Rig r;
    RunCycleConfig c = r.cfg();
    c.rxEveryNWakes = 12;

    int listened = 0;
    for (uint16_t s = 0; s < 24; ++s) {
        r.seq.store(s);
        RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
        if (cycle.shouldListenThisWake()) ++listened;
    }
    // Exactly two of twenty-four — not "few", not "some". A test that only asserted
    // "fewer than before" would pass against a node that never listens again, which is
    // the failure this whole setting is one step away from.
    TEST_ASSERT_EQUAL_INT(2, listened);
}

void test_a_freshly_powered_node_listens_immediately() {
    // seq lives in RTC memory: it survives deep sleep but NOT a power cycle, so a node
    // someone has just walked out and repowered starts at 0 — and 0 % N == 0. That is the
    // behaviour you want from the one action a person takes when a node is misbehaving,
    // and it is why the counter resetting is a feature rather than something to work around.
    Rig r;
    RunCycleConfig c = r.cfg();
    c.rxEveryNWakes = 96;          // once a day: about as deaf as this can legally get
    r.seq.store(0);
    RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_TRUE(cycle.shouldListenThisWake());

    // ...and the very next wake does NOT listen, so this is pinning the reset behaviour
    // rather than a node that listens unconditionally.
    r.seq.store(1);
    RunCycle next(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_FALSE(next.shouldListenThisWake());
}

void test_zero_is_treated_as_every_wake_not_as_never() {
    // A misconfigured 0 must not mean "never listen" — that is the unreachable-node
    // failure, and an off-by-one in whatever writes this config should not be able to
    // strand a node in a tank. rxWindowMs == 0 is the explicit, deliberate way to
    // disable listening; this field has no such meaning.
    Rig r;
    RunCycleConfig c = r.cfg();
    c.rxEveryNWakes = 0;
    r.seq.store(7);
    RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_TRUE(cycle.shouldListenThisWake());
}

void test_a_zero_window_still_wins_over_any_n() {
    Rig r;
    RunCycleConfig c = r.cfg();
    c.rxWindowMs    = 0;
    c.rxEveryNWakes = 1;
    r.seq.store(0);
    RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_FALSE(cycle.shouldListenThisWake());

    // Positive control: the same wake with a non-zero window does listen.
    c.rxWindowMs = kDefaultRxWindowMs;
    RunCycle on(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    TEST_ASSERT_TRUE(on.shouldListenThisWake());
}

void test_a_skipped_wake_does_not_touch_the_radio() {
    // The power claim, asserted rather than described: on a non-listening wake the radio
    // must not be put into receive at all. Counting receive() calls is the only way to
    // tell "listened and heard nothing" from "never listened".
    Rig r;
    r.distance.push(1200);
    r.rng.push(0);
    RunCycleConfig c = r.cfg();
    c.rxEveryNWakes = 12;
    r.seq.store(5);                       // 5 % 12 != 0

    RunCycle cycle(c, r.slots, 1, r.battery, r.radio, r.clock, r.sleeper, r.rng, r.seq);
    cycle.runOnce();

    TEST_ASSERT_EQUAL_INT(1, r.radio.sentCount());     // it still transmitted
    TEST_ASSERT_EQUAL_INT(0, r.radio.receiveCalls());  // and never listened

    // Positive control: a listening wake DOES call receive.
    Rig r2;
    r2.distance.push(1200);
    r2.rng.push(0);
    r2.seq.store(12);                     // 12 % 12 == 0
    RunCycle c2(c, r2.slots, 1, r2.battery, r2.radio, r2.clock, r2.sleeper, r2.rng, r2.seq);
    c2.runOnce();
    TEST_ASSERT_EQUAL_INT(1, r2.radio.receiveCalls());
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
    RUN_TEST(test_cycle_holds_a_receive_window_after_transmitting);
    RUN_TEST(test_a_quiet_window_is_not_a_fault);
    RUN_TEST(test_a_downlink_for_this_node_is_received_and_decoded);
    RUN_TEST(test_a_downlink_for_another_node_is_ignored_silently);
    RUN_TEST(test_a_corrupt_downlink_is_ignored_and_the_cycle_completes);
    RUN_TEST(test_a_zero_window_skips_the_listen_entirely);
    RUN_TEST(test_no_window_is_held_when_the_transmit_failed);
    RUN_TEST(test_a_valid_downlink_reaches_the_handler);
    RUN_TEST(test_the_handler_runs_before_the_node_sleeps);
    RUN_TEST(test_a_quiet_window_does_not_call_the_handler);
    RUN_TEST(test_a_downlink_for_another_node_does_not_call_the_handler);
    RUN_TEST(test_a_cycle_with_no_handler_still_completes);
    RUN_TEST(test_n_of_one_listens_on_every_wake);
    RUN_TEST(test_n_of_twelve_listens_on_one_wake_in_twelve);
    RUN_TEST(test_a_freshly_powered_node_listens_immediately);
    RUN_TEST(test_zero_is_treated_as_every_wake_not_as_never);
    RUN_TEST(test_a_zero_window_still_wins_over_any_n);
    RUN_TEST(test_a_skipped_wake_does_not_touch_the_radio);
    return UNITY_END();
}
