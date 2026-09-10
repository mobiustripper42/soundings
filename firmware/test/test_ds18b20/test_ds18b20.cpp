#include <unity.h>
#include "ds18b20.h"
#include "temp_sampler.h"
#include "../fakes/fake_onewire.h"
#include "../fakes/fake_temp.h"
#include "../fakes/fake_powerrail.h"
#include "../fakes/fake_clock.h"
#include "../fakes/fake_healflag.h"

// Phase 3.8c — the DS18B20 driver (issue #94, host-testable half).
//
// The vectors here are the part's datasheet, not this project's wire contract, so they live
// in the test rather than contracts/vectors/ — the same rule test_a02yyuw.cpp:7-12 states.
//
// ⚠ The CRC-8 expectations are pinned from an INDEPENDENT implementation. They were
// computed in Python (Dallas/Maxim CRC-8, poly X^8+X^5+X^4+1 shifted LSB-first, register
// init 0) and written here as literals. That matters because FakeOneWireBus builds its
// scratchpads with ds18b20Crc8() — the very function under test — so a wrong C++ CRC would
// otherwise be baked identically into the code and its fixtures and every test would agree
// with itself. These four literals are the only thing standing outside that loop.

using namespace soundings;

void setUp() {}
void tearDown() {}

// The conversion wait polls the bus, and the bus answers against the clock. Nothing here
// terminates unless time moves, so every test that reaches a conversion needs this — the
// same reason test_a02yyuw.cpp:25-27 gives.
static void tickingClock(FakeClock& c) { c.autoAdvance(1); }

// Datasheet Table 1, the rows this project actually needs.
static constexpr int16_t kRaw125     =  0x07D0;   // +125 C, the sensor's ceiling
static constexpr int16_t kRawSentinel=  0x0550;   //  +85 C, the power-on reset value
static constexpr int16_t kRaw25_0625 =  0x0191;   // +25.0625 C
static constexpr int16_t kRawZero    =  0x0000;   //    0 C
static constexpr int16_t kRawNeg0_5  = (int16_t)0xFFF8;   // -0.5 C
static constexpr int16_t kRawNeg55   = (int16_t)0xFC90;   // -55 C, the sensor's floor

// ---- CRC-8 ------------------------------------------------------------------

// Graded against Python, not against the fake. See the header note.
void test_crc8_matches_independently_computed_vectors() {
    const uint8_t pad12[8] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10};
    TEST_ASSERT_EQUAL_HEX8(0x70, ds18b20Crc8(pad12, 8));

    const uint8_t pad9[8]  = {0x97, 0x01, 0x4B, 0x46, 0x1F, 0xFF, 0x0C, 0x10};
    TEST_ASSERT_EQUAL_HEX8(0x73, ds18b20Crc8(pad9, 8));

    const uint8_t padNeg[8] = {0xF8, 0xFF, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10};
    TEST_ASSERT_EQUAL_HEX8(0xC3, ds18b20Crc8(padNeg, 8));

    const uint8_t zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_HEX8(0x00, ds18b20Crc8(zeros, 8));
}

// ---- The undefined-bit rule -------------------------------------------------

void test_undefined_low_bits_follow_the_resolution() {
    TEST_ASSERT_EQUAL_UINT8(0x07, dsUndefinedLowBits(kDsConfig9Bit));
    TEST_ASSERT_EQUAL_UINT8(0x03, dsUndefinedLowBits(kDsConfig10Bit));
    TEST_ASSERT_EQUAL_UINT8(0x01, dsUndefinedLowBits(kDsConfig11Bit));
    TEST_ASSERT_EQUAL_UINT8(0x00, dsUndefinedLowBits(kDsConfig12Bit));
}

// ---- Scratchpad decode ------------------------------------------------------

namespace {
// A rig at 12-bit, where no bits are undefined and the decode can be checked against the
// datasheet's table exactly as printed.
struct Rig {
    FakePowerRail        rail;
    FakeClock            clock;
    FakeOneWireBus       bus{rail, clock};
    FakeHealAttemptFlag  flag;

    Rig(uint8_t eepromCfg = kDsConfig12Bit) {
        tickingClock(clock);
        bus.setEepromConfig(eepromCfg);
    }
    Ds18b20Temp driver(const Ds18b20Config& cfg = Ds18b20Config()) {
        return Ds18b20Temp(bus, rail, clock, flag, cfg);
    }
};
} // namespace

void test_decodes_datasheet_table1_vectors() {
    const int16_t vectors[] = {kRaw125, kRaw25_0625, kRawZero, kRawNeg0_5, kRawNeg55};
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        Rig rig;
        rig.bus.setTemperature(vectors[i]);
        Ds18b20Temp d = rig.driver();
        ITemp::Reading r = d.read();
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL_INT16(vectors[i], r.raw);
    }
}

// The sign is the one place the wire contract can disagree with itself silently, so it gets
// its own named test rather than riding in the loop above.
void test_below_zero_reading_keeps_its_sign() {
    Rig rig;
    rig.bus.setTemperature(kRawNeg0_5);
    Ds18b20Temp d = rig.driver();
    ITemp::Reading r = d.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT16(-8, r.raw);       // -0.5 C = -8 counts of 1/16 C
    TEST_ASSERT_TRUE(r.raw < 0);
}

// ---- Masking ----------------------------------------------------------------

// The bug this whole task nearly shipped. The fake fills the undefined bits with 1s, so an
// unmasked driver reads 0x0197 (25.4375 C) where the truth is 25.0 C.
void test_nine_bit_reading_masks_the_undefined_low_bits() {
    Rig rig(kDsConfig9Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();
    ITemp::Reading r = d.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT16(0x0190, r.raw);   // 400 counts = 25.0 C exactly
}

// The adjacent legal case: at 12-bit every bit is real and masking would DESTROY precision.
// Paired here so a driver that masks unconditionally cannot pass.
void test_twelve_bit_reading_keeps_every_bit() {
    Rig rig(kDsConfig12Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();
    ITemp::Reading r = d.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT16(0x0191, r.raw);
}

// ---- Refusals, each paired with its adjacent legal case ---------------------

void test_crc_mismatch_rejected_and_matching_crc_accepted() {
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.corruptCrc(true);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_FALSE(d.read().ok);
    }
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_TRUE(d.read().ok);
    }
}

void test_power_on_sentinel_rejected_and_a_neighbour_accepted() {
    {
        Rig rig;
        rig.bus.setTemperature(kRawSentinel);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_FALSE(d.read().ok);
    }
    {
        Rig rig;
        rig.bus.setTemperature((int16_t)(kRawSentinel + 1));
        Ds18b20Temp d = rig.driver();
        ITemp::Reading r = d.read();
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL_INT16(kRawSentinel + 1, r.raw);
    }
}

void test_missing_presence_pulse_rejected_and_a_present_part_accepted() {
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.setPresent(false);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_FALSE(d.read().ok);
    }
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.setPresent(true);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_TRUE(d.read().ok);
    }
}

void test_band_check_rejects_below_the_sensor_floor_and_accepts_the_floor() {
    {
        Rig rig;
        rig.bus.setTemperature((int16_t)(kRawNeg55 - 1));
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_FALSE(d.read().ok);
    }
    {
        Rig rig;
        rig.bus.setTemperature(kRawNeg55);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_TRUE(d.read().ok);
    }
}

// ⚠ Paired deliberately, and the pairing is load-bearing rather than tidy. Run against a
// do-nothing driver in step 4, the refusal half of this test PASSED — a stub that returns
// {0, false} and never touches the rail satisfies every assertion in it. An assert-it-fails
// test is green on a driver that can only fail, which is precisely the false green PR #75
// found nineteen of. The legal half is what makes it bite.
void test_conversion_deadline_fires_only_on_a_bus_that_never_answers() {
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.neverFinishConversion(true);
        Ds18b20Temp d = rig.driver();
        TEST_ASSERT_FALSE(d.read().ok);
        TEST_ASSERT_FALSE(rig.rail.isOn());   // the rail comes down on the failure path too
        TEST_ASSERT_EQUAL_INT(1, rig.rail.offCount());
    }
    {
        Rig rig;
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.neverFinishConversion(false);
        Ds18b20Temp d = rig.driver();
        ITemp::Reading r = d.read();
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL_INT16(kRaw25_0625, r.raw);
    }
}

// ---- The 750 ms deadline ----------------------------------------------------

// ⚠ This pair is what pins the deadline requirement to a test rather than a comment.
//
// A sensor out of the bag converts at 12-bit and takes 750 ms. A deadline sized for the
// 9-bit resolution we WANT times that out — and the node would then fault forever on a
// perfectly good sensor, because it never survives long enough to reach the heal step that
// would have made it 9-bit.
// Same pairing rule, same reason: the timeout half alone was green against the step-4 stub.
void test_factory_default_needs_a_deadline_that_covers_750ms() {
    {
        Rig rig(kDsConfig12Bit);
        rig.bus.setTemperature(kRaw25_0625);
        Ds18b20Config cfg;
        cfg.conversionDeadlineMs = 200;    // ample for 93.75 ms, hopeless for 750 ms
        Ds18b20Temp d = rig.driver(cfg);
        TEST_ASSERT_FALSE(d.read().ok);
    }
    {
        Rig rig(kDsConfig12Bit);
        rig.bus.setTemperature(kRaw25_0625);
        Ds18b20Temp d = rig.driver();      // default 900 ms
        TEST_ASSERT_TRUE(d.read().ok);
    }
}

// ---- A clone that lies about its conversion status --------------------------

// ⚠ The bench failure of 2026-09-09, written as a test (issue #94).
//
// The datasheet says an externally-powered DS18B20 answers read slots with 0 until its
// conversion finishes. The marketplace clones this project buys on purpose often never
// implement that and answer 1 immediately. A driver that believes the status bit reads the
// scratchpad before the conversion has put anything in it, gets 0x0550 — the power-on value
// — and the sentinel check correctly rejects it. The channel then faults forever on a probe
// that is working perfectly, which is exactly what a real probe did on a real bench.
//
// The second half is not decoration. Without the "did not sit out the deadline" assertion,
// a driver that abandons the poll and blindly waits conversionDeadlineMs on every read would
// pass — and it would cost 900 ms of awake time per wake against a power budget whose whole
// margin is 1.48x (DEC-006). The cheap wrong fix has to fail here.
void test_a_lying_conversion_status_still_yields_a_real_reading() {
    {
        Rig rig(kDsConfig9Bit);                    // healed already: 93.75 ms conversion
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.lieAboutConversionStatus(true);
        Ds18b20Temp d = rig.driver();

        const uint32_t before = rig.clock.millis();
        ITemp::Reading r      = d.read();
        const uint32_t waited = rig.clock.millis() - before;

        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL_INT16(0x0190, r.raw);    // 25.0 C, masked at 9-bit
        TEST_ASSERT_TRUE(waited >= 94);            // it did wait out the real conversion
        TEST_ASSERT_TRUE(waited < 300);            // and did NOT sit out the 900 ms deadline
    }
    {
        // Out of the bag at 12-bit, where the floor has to be 750 ms rather than 94. A fix
        // that hardcodes one conversion time passes the case above and fails this one.
        Rig rig(kDsConfig12Bit);
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.lieAboutConversionStatus(true);
        Ds18b20Temp d = rig.driver();
        ITemp::Reading r = d.read();
        TEST_ASSERT_TRUE(r.ok);
        TEST_ASSERT_EQUAL_INT16(kRaw25_0625, r.raw);
    }
}

// ---- The heal path ----------------------------------------------------------

void test_factory_default_is_healed_and_the_reading_still_publishes() {
    Rig rig(kDsConfig12Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();

    ITemp::Reading r = d.read();

    // The reading is good data taken at a finer resolution than we asked for. It publishes
    // un-faulted: the problem is power, not measurement.
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT16(kRaw25_0625, r.raw);

    TEST_ASSERT_TRUE(d.healAttemptedLastRead());
    TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());
    TEST_ASSERT_EQUAL_HEX8(kDsConfig9Bit, rig.bus.eepromConfig());
    // ⚠ Set even though the write was taken. The flag is marked on ATTEMPT, and since the
    // verify was removed it is the only thing bounding EEPROM writes — one per boot, whatever
    // the part does with them. It costs nothing: a part that took the write comes up 9-bit
    // forever and never re-enters the branch.
    TEST_ASSERT_TRUE(rig.flag.attempted());
}

// ⚠ THE ENDURANCE GUARANTEE, and since the verify was removed it is the ONLY thing standing
// between this driver and the part's 50,000-write rating.
//
// A DS18B20 can accept Copy Scratchpad, keep nothing, and report no error. Nothing the
// driver can do detects that — which is precisely why the verify went: it claimed to and
// couldn't (the rail does not collapse in 50 ms; measured on the bench 2026-09-09). So the
// bound is structural instead. The flag is marked on attempt, so a refusing part is written
// at most once per boot no matter what it says. At a fifteen-minute cadence the unbounded
// version would be ~35,000 writes a year.
void test_a_refused_eeprom_write_is_not_retried_in_the_same_boot() {
    Rig rig(kDsConfig12Bit);
    rig.bus.setTemperature(kRaw25_0625);
    rig.bus.refuseEepromWrite(true);

    Ds18b20Temp first = rig.driver();
    ITemp::Reading r = first.read();
    TEST_ASSERT_TRUE(r.ok);                                          // the reading still publishes
    TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());
    TEST_ASSERT_EQUAL_HEX8(kDsConfig12Bit, rig.bus.eepromConfig());  // nothing was kept
    TEST_ASSERT_TRUE(rig.flag.attempted());

    // The next wake finds the part still at 12-bit and must NOT write again.
    Ds18b20Temp second = rig.driver();
    TEST_ASSERT_TRUE(second.read().ok);
    TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());
    TEST_ASSERT_FALSE(second.healAttemptedLastRead());
}

// The steady-state case, and the one that catches an inverted comparison. A driver that
// tested the condition the wrong way round would write EEPROM on every single wake and burn
// the 50,000 rating in about seventeen months.
void test_a_sensor_already_at_nine_bit_is_never_written() {
    Rig rig(kDsConfig9Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();

    TEST_ASSERT_TRUE(d.read().ok);
    TEST_ASSERT_FALSE(d.healAttemptedLastRead());
    TEST_ASSERT_EQUAL_INT(0, rig.bus.copyScratchpadCount());
    TEST_ASSERT_EQUAL_INT(0, rig.bus.writeScratchpadCount());
}

// ⚠ THE REMOVED VERIFY, pinned as an assertion so it cannot creep back.
//
// The heal used to power-cycle the rail to prove the EEPROM had taken the write: the
// scratchpad reloads from EEPROM on power-up, so a 9-bit value surviving the cycle meant the
// write stuck. It never worked. Bench sweep, 2026-09-09: Ve does not fall below the part's
// reset threshold until somewhere between 50 and 100 ms, and the verify allowed 50 — so the
// part never restarted, handed back the RAM copy it had just been given, and reported
// success unconditionally. A check that cannot fail is not a check.
//
// A heal wake now costs exactly the same rail cycle as any other read. Paired with the
// ordinary-read case below so this cannot be satisfied by a driver that never heals.
void test_the_heal_does_not_cycle_the_rail_a_second_time() {
    Rig rig(kDsConfig12Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();
    TEST_ASSERT_TRUE(d.read().ok);

    TEST_ASSERT_TRUE(d.healAttemptedLastRead());          // a heal really did happen
    TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());
    TEST_ASSERT_EQUAL_INT(1, rig.rail.onCount());         // on, off. No second cycle.
    TEST_ASSERT_EQUAL_INT(1, rig.rail.offCount());
    TEST_ASSERT_FALSE(rig.rail.isOn());
}

// The ordinary read, which never healed and never cycled twice. Paired with the test above
// so "one cycle" cannot be satisfied by a driver that simply never heals.
void test_an_ordinary_read_cycles_the_rail_exactly_once() {
    Rig rig(kDsConfig9Bit);
    rig.bus.setTemperature(kRaw25_0625);
    Ds18b20Temp d = rig.driver();
    TEST_ASSERT_TRUE(d.read().ok);
    TEST_ASSERT_FALSE(d.healAttemptedLastRead());
    TEST_ASSERT_EQUAL_INT(1, rig.rail.onCount());
    TEST_ASSERT_EQUAL_INT(1, rig.rail.offCount());
}

void test_an_eeprom_that_refuses_the_copy_sets_the_flag() {
    Rig rig(kDsConfig12Bit);
    rig.bus.setTemperature(kRaw25_0625);
    rig.bus.refuseEepromWrite(true);
    Ds18b20Temp d = rig.driver();

    ITemp::Reading r = d.read();
    TEST_ASSERT_TRUE(r.ok);                    // still good data
    TEST_ASSERT_TRUE(d.healAttemptedLastRead());
    TEST_ASSERT_TRUE(rig.flag.attempted());
    TEST_ASSERT_EQUAL_HEX8(kDsConfig12Bit, rig.bus.eepromConfig());
}

// ⚠ The bound. A second wake — modelled by a second driver around the same flag, the way
// FakeSeqStore models a reset — must not issue Copy Scratchpad again.
void test_flag_suppresses_a_second_attempt_and_a_clear_flag_does_not() {
    {
        Rig rig(kDsConfig12Bit);
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.refuseEepromWrite(true);

        Ds18b20Temp first = rig.driver();
        first.read();
        const int afterFirst = rig.bus.copyScratchpadCount();
        TEST_ASSERT_EQUAL_INT(1, afterFirst);

        Ds18b20Temp second = rig.driver();     // next wake, same RTC flag
        ITemp::Reading r = second.read();
        TEST_ASSERT_TRUE(r.ok);                // still reads, still publishes
        TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());   // and still one write
        TEST_ASSERT_FALSE(second.healAttemptedLastRead());
    }
    {
        // The paired case: a fresh boot clears the flag, so one more attempt is made. Without
        // this the test above would pass on a driver that simply never heals.
        Rig rig(kDsConfig12Bit);
        rig.bus.setTemperature(kRaw25_0625);
        rig.bus.refuseEepromWrite(true);

        Ds18b20Temp first = rig.driver();
        first.read();
        TEST_ASSERT_EQUAL_INT(1, rig.bus.copyScratchpadCount());

        FakeHealAttemptFlag freshBoot;         // RTC memory lost to a power cycle
        Ds18b20Temp second(rig.bus, rig.rail, rig.clock, freshBoot);
        second.read();
        TEST_ASSERT_EQUAL_INT(2, rig.bus.copyScratchpadCount());
    }
}

// ---- The cast across the ISampler seam --------------------------------------

// ⚠ These two exist because of a mutation check. Clamping negatives to zero in
// TempSampler::sample() was caught by exactly ONE test in the whole 203-case suite — the
// end-to-end round trip in test_manifest. One guard on a silent reinterpretation is too few:
// weaken or delete that test and a node reports +0.0 C every time the headspace goes below
// freezing, and every parser downstream agrees with it.
void test_temp_sampler_carries_a_negative_reading_across_the_cast() {
    FakeTemp t;
    TempSampler s(t);

    t.setReading((int16_t)0xFFF8);            // -0.5 C
    ISampler::Sample negative = s.sample();
    TEST_ASSERT_TRUE(negative.ok);
    TEST_ASSERT_EQUAL_HEX16(0xFFF8, negative.raw);
    TEST_ASSERT_EQUAL_INT16(-8, (int16_t)negative.raw);

    // Paired, so an adapter that hard-coded the negative case cannot pass.
    t.setReading(400);                         // +25.0 C
    TEST_ASSERT_EQUAL_HEX16(400, s.sample().raw);
}

// The same crossing, but driven by the REAL driver rather than a fake reading — so the
// int16 the scratchpad decode produces is the int16 the cast receives.
void test_driver_negative_reading_reaches_the_sampler_seam_intact() {
    Rig rig;
    rig.bus.setTemperature(kRawNeg55);
    Ds18b20Temp d = rig.driver();
    TempSampler s(d);

    ISampler::Sample sample = s.sample();
    TEST_ASSERT_TRUE(sample.ok);
    TEST_ASSERT_EQUAL_HEX16(0xFC90, sample.raw);
    TEST_ASSERT_EQUAL_INT16(-880, (int16_t)sample.raw);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc8_matches_independently_computed_vectors);
    RUN_TEST(test_undefined_low_bits_follow_the_resolution);
    RUN_TEST(test_decodes_datasheet_table1_vectors);
    RUN_TEST(test_below_zero_reading_keeps_its_sign);
    RUN_TEST(test_nine_bit_reading_masks_the_undefined_low_bits);
    RUN_TEST(test_twelve_bit_reading_keeps_every_bit);
    RUN_TEST(test_crc_mismatch_rejected_and_matching_crc_accepted);
    RUN_TEST(test_power_on_sentinel_rejected_and_a_neighbour_accepted);
    RUN_TEST(test_missing_presence_pulse_rejected_and_a_present_part_accepted);
    RUN_TEST(test_band_check_rejects_below_the_sensor_floor_and_accepts_the_floor);
    RUN_TEST(test_conversion_deadline_fires_only_on_a_bus_that_never_answers);
    RUN_TEST(test_factory_default_needs_a_deadline_that_covers_750ms);
    RUN_TEST(test_a_lying_conversion_status_still_yields_a_real_reading);
    RUN_TEST(test_factory_default_is_healed_and_the_reading_still_publishes);
    RUN_TEST(test_a_refused_eeprom_write_is_not_retried_in_the_same_boot);
    RUN_TEST(test_a_sensor_already_at_nine_bit_is_never_written);
    RUN_TEST(test_the_heal_does_not_cycle_the_rail_a_second_time);
    RUN_TEST(test_an_ordinary_read_cycles_the_rail_exactly_once);
    RUN_TEST(test_an_eeprom_that_refuses_the_copy_sets_the_flag);
    RUN_TEST(test_flag_suppresses_a_second_attempt_and_a_clear_flag_does_not);
    RUN_TEST(test_temp_sampler_carries_a_negative_reading_across_the_cast);
    RUN_TEST(test_driver_negative_reading_reaches_the_sampler_seam_intact);
    return UNITY_END();
}
