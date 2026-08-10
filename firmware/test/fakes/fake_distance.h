#pragma once
#include "idistance.h"

namespace soundings {

// FakeDistance — host-test stand-in for the A02YYUW adapter (the real UART driver lands
// in Phase 3.8). A test scripts the readings it will return, in order; swapping it for
// the real driver at the bench is the entire point of the seam.
class FakeDistance : public IDistance {
public:
    static const int kCapacity = 8;

    // Queue one reading. Past kCapacity the push is dropped — a test that needs a longer
    // script wants a loop over reads, not a longer queue.
    void push(uint16_t raw, bool ok = true) {
        if (count_ < kCapacity) script_[count_++] = Reading{raw, ok};
    }

    Reading read() override {
        // Unscripted reads as a FAILURE, not as 0 mm. Zero is a legal-looking distance,
        // so a zero default would let a test that forgot to script the sensor pass while
        // asserting against a number the sensor never produced.
        if (count_ == 0) return Reading{0, false};
        // Past the end, hold the last value rather than wrapping to the first: wrapping
        // would fabricate a plausible drydown-and-refill out of a script that ran out.
        if (idx_ >= count_) return script_[count_ - 1];
        return script_[idx_++];
    }

private:
    Reading script_[kCapacity] = {};
    int     count_ = 0;
    int     idx_   = 0;
};

} // namespace soundings
