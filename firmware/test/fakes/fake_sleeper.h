#pragma once
#include "isleeper.h"
#include "fake_clock.h"

namespace soundings {

// FakeSleeper — host-test stand-in for deep sleep. Advances the injected FakeClock by the
// requested interval and records what was asked for, so a run cycle's schedule and jitter
// window can be asserted with zero real time elapsed.
//
// ⚠ It RETURNS. The real ESP32 deep sleep does not — it resets the MCU (see isleeper.h).
// A test process that reset would be useless, so the difference is deliberate; do not
// infer from a passing test that code after sleepFor() runs on hardware.
class FakeSleeper : public ISleeper {
public:
    explicit FakeSleeper(FakeClock& clock) : clock_(clock) {}

    void sleepFor(uint32_t ms) override {
        last_   = ms;
        total_ += ms;
        ++count_;
        clock_.advance(ms);   // or a scheduled cycle would sample the same instant forever
    }

    uint32_t lastRequested() const { return last_; }
    uint32_t totalSlept()    const { return total_; }
    int      sleepCount()    const { return count_; }

private:
    FakeClock& clock_;
    uint32_t   last_  = 0;
    uint32_t   total_ = 0;
    int        count_ = 0;
};

} // namespace soundings
