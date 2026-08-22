#pragma once
#include "iclock.h"

namespace soundings {

// FakeClock — the host-test millis() seam. Time moves only when the test advances it, so
// the non-blocking run cycle is exercised deterministically with no real time elapsed.
class FakeClock : public IClock {
public:
    uint32_t millis() const override {
        const uint32_t t = now_;
        now_ += autoAdvance_;
        return t;
    }
    void advance(uint32_t ms) { now_ += ms; }   // wrap-around is intentional — exercises rollover
    void set(uint32_t ms)     { now_ = ms; }

    // Make time pass on every reading, opt-in and OFF by default so no existing test
    // changes behaviour. Needed by any loop whose only escape is a deadline: RunCycle's
    // transmit() also bounds itself by a retry COUNT, so it terminates against a frozen
    // clock, but a driver polling a free-running UART has no count to bound — the sensor
    // is under no obligation to send anything, and "nothing available" is its ordinary
    // between-frames answer. Against a clock that never ticks, that loop is infinite, and
    // a test proving the timeout fires would hang the suite rather than fail it.
    void autoAdvance(uint32_t msPerRead) { autoAdvance_ = msPerRead; }

private:
    // mutable because millis() is const — reading the clock does not logically mutate it
    // (iclock.h), and that constness is load-bearing for Elapsed::expired(). The tick is
    // an artefact of the fake standing in for time that would have passed anyway.
    mutable uint32_t now_ = 0;
    uint32_t autoAdvance_ = 0;
};

} // namespace soundings
