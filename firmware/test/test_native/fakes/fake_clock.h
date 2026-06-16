#pragma once
#include "iclock.h"

namespace soundings {

// FakeClock — the host-test millis() seam. Time moves only when the test advances it, so
// the non-blocking run cycle is exercised deterministically with no real time elapsed.
class FakeClock : public IClock {
public:
    uint32_t millis() override { return now_; }
    void advance(uint32_t ms) { now_ += ms; }   // wrap-around is intentional — exercises rollover
    void set(uint32_t ms)     { now_ = ms; }
private:
    uint32_t now_ = 0;
};

} // namespace soundings
