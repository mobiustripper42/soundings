#pragma once
#include <stdint.h>
#include "iclock.h"

namespace soundings {

// Elapsed — a non-blocking deadline measured against an IClock. This is the delay()
// replacement for the run path: arm() records a start instant, expired() reports whether
// the interval has passed. The unsigned subtraction (now - start) is wrap-safe, so a
// deadline that straddles the ~49.7-day uint32 millis() rollover still fires correctly
// for any interval well below that period.
class Elapsed {
public:
    explicit Elapsed(IClock& clock) : clock_(clock) {}

    // Arm the timer: the deadline is intervalMs after the current clock reading.
    void arm(uint32_t intervalMs) {
        start_    = clock_.millis();
        interval_ = intervalMs;
        armed_    = true;
    }

    // True once the armed interval has elapsed; false if never armed.
    bool expired() const {
        if (!armed_) return false;
        return (clock_.millis() - start_) >= interval_;
    }

    // Milliseconds since arm() (0 if never armed). Wrap-safe.
    uint32_t since() const {
        return armed_ ? (clock_.millis() - start_) : 0;
    }

private:
    IClock&  clock_;
    uint32_t start_    = 0;
    uint32_t interval_ = 0;
    bool     armed_    = false;
};

} // namespace soundings
