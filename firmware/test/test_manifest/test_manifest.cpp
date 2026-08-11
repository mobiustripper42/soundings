#include <unity.h>
#include "sensor_registry.h"
#include "tank_preset.h"
#include "missing_sampler.h"
#include "distance_sampler.h"
#include "../fakes/fake_clock.h"
#include "../fakes/fake_distance.h"
#include "../fakes/fake_radio.h"
#include "../fakes/fake_battery.h"
#include "../fakes/fake_sleeper.h"
#include "../fakes/fake_random.h"
#include "../fakes/fake_seqstore.h"
#include "../fakes/fake_sampler.h"
#include "../fakes/fake_manifeststore.h"

// Phase 3.5 — the minimal declared manifest (issue #44). Identity as data (DEC-002),
// narrowed to the tank node: no byte encoding, no NVS, no preset system.
//
// The test that matters most here is test_declared_channel_with_no_driver_is_faulted_not_dropped.
// Everything else is plumbing; that one is the decision.

using namespace soundings;

static const uint8_t kTankChannel  = 8;    // TANK_DISTANCE
static const uint8_t kSoilChannel  = 0;    // SOIL_TENSION_0
static const uint8_t kReservedBit  = 13;   // width 0 in the packet.h registry

void setUp() {}
void tearDown() {}

// A whole node, wired from a manifest the way the field node will be.
struct Node {
    FakeClock    clock;
    FakeDistance distance;
    FakeRadio    radio;
    FakeBattery  battery;
    FakeSleeper  sleeper{clock};
    FakeRandom   rng;
    FakeSeqStore seq;
    DistanceSampler distanceSampler{distance};
    SamplerEntry registry[1] = {{kSensorTypeDistance, &distanceSampler}};
    SensorSlot   slots[kMaxChannels] = {};
    size_t       slotCount = 0;

    BindResult bind(const NodeManifest& m) {
        return bindManifest(m, registry, 1, slots, kMaxChannels, slotCount);
    }
    ParseResult received(int i, Packet& out) {
        return deserialize(radio.frame(i), radio.frameLen(i), out);
    }
};

static NodeManifest manifestOf(uint8_t node_id, const ChannelDecl* decls, uint8_t n) {
    NodeManifest m;
    m.node_id    = node_id;
    m.intervalMs = kDefaultIntervalMs;
    m.jitterMs   = kDefaultJitterMs;
    for (uint8_t i = 0; i < n; ++i) m.channels[i] = decls[i];
    m.count = n;
    return m;
}

// ---- The preset ------------------------------------------------------------

void test_tank_preset_declares_exactly_the_distance_channel() {
    NodeManifest m = tankPreset(7);
    TEST_ASSERT_EQUAL_UINT8(7, m.node_id);
    TEST_ASSERT_EQUAL_UINT8(1, m.count);
    TEST_ASSERT_EQUAL_UINT8(kTankChannel, m.channels[0].channelBit);
    TEST_ASSERT_EQUAL_UINT8(kSensorTypeDistance, m.channels[0].sensorTypeId);
}

void test_tank_preset_carries_the_spec_cadence() {
    NodeManifest m = tankPreset(7);
    TEST_ASSERT_EQUAL_UINT32(kDefaultIntervalMs, m.intervalMs);
    TEST_ASSERT_EQUAL_UINT32(kDefaultJitterMs, m.jitterMs);
}

// The node id is the one field that genuinely differs between two tank nodes, and it must
// come from provisioning rather than a recompile (DEC-002, identity-as-data).
void test_tank_preset_takes_its_node_id_from_the_caller() {
    TEST_ASSERT_EQUAL_UINT8(3, tankPreset(3).node_id);
    TEST_ASSERT_EQUAL_UINT8(9, tankPreset(9).node_id);
}

// ---- Binding ---------------------------------------------------------------

void test_preset_binds_to_a_single_slot_on_the_distance_sampler() {
    Node n;
    TEST_ASSERT_TRUE(n.bind(tankPreset(7)) == BindResult::Ok);
    TEST_ASSERT_EQUAL_size_t(1, n.slotCount);
    TEST_ASSERT_EQUAL_UINT8(kTankChannel, n.slots[0].channelBit);
    TEST_ASSERT_EQUAL_PTR(&n.distanceSampler, n.slots[0].sampler);
}

// THE DEC-002 TEST. A declared sensor whose driver is absent must ride as declared AND
// faulted — never dropped. A dropped channel is indistinguishable downstream from a sensor
// that was never fitted, so the tank goes quiet and nothing says why.
void test_declared_channel_with_no_driver_is_faulted_not_dropped() {
    Node n;
    const ChannelDecl decls[] = {{kSoilChannel, 99}};   // type 99: nothing serves it
    NodeManifest m = manifestOf(7, decls, 1);

    TEST_ASSERT_TRUE(n.bind(m) == BindResult::Ok);
    TEST_ASSERT_EQUAL_size_t(1, n.slotCount);           // bound, not skipped
    TEST_ASSERT_EQUAL_UINT8(kSoilChannel, n.slots[0].channelBit);

    n.battery.setReading(3800);
    n.rng.push(0);
    RunCycleConfig cfg = configFromManifest(m, 0x0103);
    RunCycle c(cfg, n.slots, n.slotCount, n.battery, n.radio, n.clock,
               n.sleeper, n.rng, n.seq);
    c.runOnce();

    Packet p;
    TEST_ASSERT_TRUE(n.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_TRUE(p.hasChannel(kSoilChannel));   // declared: "expected, missing"
    TEST_ASSERT_TRUE(p.isFault(kSoilChannel));      // and faulted, every cycle
}

// A declaration with no type at all is the same case — a channel the node cannot serve.
void test_channel_declared_with_no_sensor_type_is_faulted_not_dropped() {
    Node n;
    const ChannelDecl decls[] = {{kSoilChannel, kSensorTypeNone}};
    TEST_ASSERT_TRUE(n.bind(manifestOf(7, decls, 1)) == BindResult::Ok);
    TEST_ASSERT_EQUAL_size_t(1, n.slotCount);
    TEST_ASSERT_FALSE(n.slots[0].sampler->sample().ok);
}

// The other half of DEC-002: one firmware carries every driver, and a node runs only what
// it declares. A compiled-in driver nobody asked for must not appear on the wire.
void test_registered_driver_that_is_not_declared_is_never_sampled() {
    Node n;
    const ChannelDecl decls[] = {{kSoilChannel, kSensorTypeNone}};
    NodeManifest m = manifestOf(7, decls, 1);
    n.distance.push(1347);                     // a perfectly good reading, undeclared
    n.battery.setReading(3800);
    n.rng.push(0);

    TEST_ASSERT_TRUE(n.bind(m) == BindResult::Ok);
    RunCycleConfig cfg = configFromManifest(m, 0x0103);
    RunCycle c(cfg, n.slots, n.slotCount, n.battery, n.radio, n.clock,
               n.sleeper, n.rng, n.seq);
    c.runOnce();

    Packet p;
    TEST_ASSERT_TRUE(n.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_FALSE(p.hasChannel(kTankChannel));
    // Non-vacuously: the manifest's OWN channel did bind and did ship, so the absence
    // above is a decision about the undeclared driver rather than a cycle that bound
    // nothing at all. Without these two lines an empty slot array passes this test.
    TEST_ASSERT_EQUAL_size_t(1, n.slotCount);
    TEST_ASSERT_TRUE(p.hasChannel(kSoilChannel));
}

void test_multiple_declarations_bind_in_order() {
    Node n;
    FakeSampler soil;
    soil.push(444);
    SamplerEntry reg[2] = {{kSensorTypeDistance, &n.distanceSampler}, {50, &soil}};
    const ChannelDecl decls[] = {{kSoilChannel, 50}, {kTankChannel, kSensorTypeDistance}};
    NodeManifest m = manifestOf(7, decls, 2);

    size_t count = 0;
    TEST_ASSERT_TRUE(bindManifest(m, reg, 2, n.slots, kMaxChannels, count) == BindResult::Ok);
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_UINT8(kSoilChannel, n.slots[0].channelBit);
    TEST_ASSERT_EQUAL_UINT8(kTankChannel, n.slots[1].channelBit);
}

// ---- Rejection -------------------------------------------------------------

// Caught at bind time so the error names the MANIFEST. serialize() would reject the same
// packet for being unsizable, which is a true statement about the wrong thing and sends
// whoever reads the log into the codec instead of into provisioning.
void test_channel_with_no_registry_width_is_rejected_at_bind_time() {
    Node n;
    const ChannelDecl decls[] = {{kReservedBit, kSensorTypeDistance}};
    TEST_ASSERT_TRUE(n.bind(manifestOf(7, decls, 1)) == BindResult::UnknownChannel);
    TEST_ASSERT_EQUAL_size_t(0, n.slotCount);
}

void test_channel_bit_beyond_the_registry_is_rejected() {
    Node n;
    const ChannelDecl decls[] = {{kMaxChannels, kSensorTypeDistance}};   // one past the end
    TEST_ASSERT_TRUE(n.bind(manifestOf(7, decls, 1)) == BindResult::UnknownChannel);
    TEST_ASSERT_EQUAL_size_t(0, n.slotCount);
}

// count is a uint8_t and the array it indexes holds kMaxChannels — a byte-format parser
// (the deferred provisioning task) can produce a manifest claiming more than it has room
// for, and bindManifest must not read past the array on the strength of that claim. The
// guard is inside bindManifest rather than resting on every caller passing
// outCap == kMaxChannels, because the next caller is a deserializer that has not been
// written yet.
void test_count_beyond_the_manifest_array_is_rejected_without_reading_past_it() {
    Node n;
    NodeManifest m = tankPreset(7);
    m.count = kMaxChannels + 1;          // a lie a parser could plausibly tell

    // The out array is deliberately LARGER than the manifest's own, because that is the
    // only configuration in which the existing `count > outCap` check does not already
    // cover this. With outCap == kMaxChannels the bound holds by coincidence of the two
    // numbers being equal — which is exactly the caller discipline this pins.
    SensorSlot roomy[kMaxChannels * 2] = {};
    size_t count = 0;
    TEST_ASSERT_TRUE(bindManifest(m, n.registry, 1, roomy, kMaxChannels * 2, count)
                     == BindResult::TooManyChannels);
    TEST_ASSERT_EQUAL_size_t(0, count);
}

// Last-wins would silently swallow a provisioning typo and ship half the sensors the
// operator thinks are fitted.
void test_duplicate_channel_declaration_is_rejected() {
    Node n;
    const ChannelDecl decls[] = {{kTankChannel, kSensorTypeDistance},
                                 {kTankChannel, kSensorTypeDistance}};
    TEST_ASSERT_TRUE(n.bind(manifestOf(7, decls, 2)) == BindResult::DuplicateChannel);
}

void test_more_declarations_than_slots_is_rejected() {
    Node n;
    const ChannelDecl decls[] = {{kSoilChannel, kSensorTypeNone},
                                 {kTankChannel, kSensorTypeDistance}};
    NodeManifest m = manifestOf(7, decls, 2);
    size_t count = 0;
    TEST_ASSERT_TRUE(bindManifest(m, n.registry, 1, n.slots, 1, count)
                     == BindResult::TooManyChannels);
}

// A rejected manifest must leave nothing half-bound — a partial slot array would run a
// node on a configuration the operator never wrote.
void test_a_rejected_manifest_binds_no_slots_at_all() {
    Node n;
    const ChannelDecl decls[] = {{kTankChannel, kSensorTypeDistance},
                                 {kReservedBit, kSensorTypeDistance}};
    TEST_ASSERT_TRUE(n.bind(manifestOf(7, decls, 2)) == BindResult::UnknownChannel);
    TEST_ASSERT_EQUAL_size_t(0, n.slotCount);
}

// ---- Config ----------------------------------------------------------------

void test_config_carries_identity_and_cadence_from_the_manifest() {
    NodeManifest m = tankPreset(11);
    m.intervalMs = 60000;
    m.jitterMs   = 5000;
    RunCycleConfig cfg = configFromManifest(m, 0x0207);
    TEST_ASSERT_EQUAL_UINT8(11, cfg.node_id);
    TEST_ASSERT_EQUAL_UINT16(0x0207, cfg.fw_version);
    TEST_ASSERT_EQUAL_UINT32(60000, cfg.intervalMs);
    TEST_ASSERT_EQUAL_UINT32(5000, cfg.jitterMs);
}

// The PR #55 clamp has to survive the manifest becoming its input — which is the exact
// path the reviewer flagged as the reason that bug mattered.
void test_manifest_supplied_nonsense_jitter_is_still_clamped() {
    Node n;
    NodeManifest m = tankPreset(7);
    m.intervalMs = 1000;
    m.jitterMs   = 5000;
    n.rng.push(0);
    RunCycleConfig cfg = configFromManifest(m, 0x0103);
    RunCycle c(cfg, n.slots, 0, n.battery, n.radio, n.clock, n.sleeper, n.rng, n.seq);
    TEST_ASSERT_EQUAL_UINT32(0, c.nextSleepMs());
}

// ---- End to end ------------------------------------------------------------

void test_manifest_store_to_packet_end_to_end() {
    Node n;
    FakeManifestStore store(tankPreset(7));
    n.distance.push(1347);
    n.battery.setReading(3810);
    n.rng.push(0);

    const NodeManifest& m = store.load();
    TEST_ASSERT_TRUE(n.bind(m) == BindResult::Ok);
    RunCycleConfig cfg = configFromManifest(m, 0x0103);
    RunCycle c(cfg, n.slots, n.slotCount, n.battery, n.radio, n.clock,
               n.sleeper, n.rng, n.seq);
    c.runOnce();

    Packet p;
    TEST_ASSERT_TRUE(n.received(0, p) == ParseResult::Ok);
    TEST_ASSERT_EQUAL_UINT8(7, p.node_id);
    TEST_ASSERT_TRUE(p.hasChannel(kTankChannel));
    TEST_ASSERT_FALSE(p.isFault(kTankChannel));
    TEST_ASSERT_EQUAL_UINT16(1347, p.channels[kTankChannel]);
    TEST_ASSERT_EQUAL_UINT16(3810, p.battery_mv);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tank_preset_declares_exactly_the_distance_channel);
    RUN_TEST(test_tank_preset_carries_the_spec_cadence);
    RUN_TEST(test_tank_preset_takes_its_node_id_from_the_caller);
    RUN_TEST(test_preset_binds_to_a_single_slot_on_the_distance_sampler);
    RUN_TEST(test_declared_channel_with_no_driver_is_faulted_not_dropped);
    RUN_TEST(test_channel_declared_with_no_sensor_type_is_faulted_not_dropped);
    RUN_TEST(test_registered_driver_that_is_not_declared_is_never_sampled);
    RUN_TEST(test_multiple_declarations_bind_in_order);
    RUN_TEST(test_channel_with_no_registry_width_is_rejected_at_bind_time);
    RUN_TEST(test_channel_bit_beyond_the_registry_is_rejected);
    RUN_TEST(test_count_beyond_the_manifest_array_is_rejected_without_reading_past_it);
    RUN_TEST(test_duplicate_channel_declaration_is_rejected);
    RUN_TEST(test_more_declarations_than_slots_is_rejected);
    RUN_TEST(test_a_rejected_manifest_binds_no_slots_at_all);
    RUN_TEST(test_config_carries_identity_and_cadence_from_the_manifest);
    RUN_TEST(test_manifest_supplied_nonsense_jitter_is_still_clamped);
    RUN_TEST(test_manifest_store_to_packet_end_to_end);
    return UNITY_END();
}
