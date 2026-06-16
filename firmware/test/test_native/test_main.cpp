#include <unity.h>
#include "elapsed.h"
#include "fakes/fake_clock.h"
#include "fakes/fake_sensor.h"

// Native unit tests (Unity) — the load-bearing test tier (CLAUDE.md "Testing"). Phase
// 1.1 proves the harness end to end: a fake clock drives the non-blocking Elapsed timer
// and a fake sensor feeds the ISoilMoisture seam, all on the host with no hardware and
// no real time. Phase 2 grows this with the real sensor math and run cycle.

using namespace soundings;

void setUp() {}
void tearDown() {}

void test_elapsed_not_expired_before_interval() {
    FakeClock clock;
    Elapsed timer(clock);
    timer.arm(1000);
    clock.advance(999);
    TEST_ASSERT_FALSE(timer.expired());
}

void test_elapsed_expired_at_interval() {
    FakeClock clock;
    Elapsed timer(clock);
    timer.arm(1000);
    clock.advance(1000);
    TEST_ASSERT_TRUE(timer.expired());
}

void test_elapsed_unarmed_never_expires() {
    FakeClock clock;
    Elapsed timer(clock);
    clock.advance(100000);
    TEST_ASSERT_FALSE(timer.expired());
}

// The reason the subtraction is (now - start) and not (now >= start + interval): a
// deadline that straddles the uint32 millis() rollover must still fire.
void test_elapsed_wrap_safe_across_millis_rollover() {
    FakeClock clock;
    clock.set(0xFFFFFFFFu - 100);   // 100 ms before the millis() rollover
    Elapsed timer(clock);
    timer.arm(200);                  // deadline lands 100 ms past the wrap
    clock.advance(150);              // 150 ms elapsed, 50 ms past the wrap
    TEST_ASSERT_FALSE(timer.expired());
    clock.advance(50);               // 200 ms elapsed
    TEST_ASSERT_TRUE(timer.expired());
}

void test_fake_sensor_returns_set_reading() {
    FakeSoilMoisture sensor;
    sensor.setReading(2048);
    ISoilMoisture::Reading r = sensor.read();
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT16(2048, r.raw);
}

void test_fake_sensor_signals_failed_read() {
    FakeSoilMoisture sensor;
    sensor.setReading(0, false);
    TEST_ASSERT_FALSE(sensor.read().ok);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_elapsed_not_expired_before_interval);
    RUN_TEST(test_elapsed_expired_at_interval);
    RUN_TEST(test_elapsed_unarmed_never_expires);
    RUN_TEST(test_elapsed_wrap_safe_across_millis_rollover);
    RUN_TEST(test_fake_sensor_returns_set_reading);
    RUN_TEST(test_fake_sensor_signals_failed_read);
    return UNITY_END();
}
