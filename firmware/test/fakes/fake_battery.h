#pragma once
#include "ibattery.h"

namespace soundings {

// FakeBattery — host-test stand-in for the ADC read (gated by ADC_Ctrl on the real board,
// Phase 3.10). The test sets the pack voltage and whether the read succeeds.
class FakeBattery : public IBattery {
public:
    void setReading(uint16_t millivolts, bool ok = true) {
        mv_ = millivolts;
        ok_ = ok;
        set_ = true;
    }

    Reading read() override {
        // Unscripted reads as a failure for the same reason as IDistance: 0 mV is a
        // legal-looking value (a flat pack), so defaulting to it would quietly turn a
        // forgotten setReading() into an assertion about a dead battery.
        if (!set_) return Reading{0, false};
        return Reading{mv_, ok_};
    }

private:
    uint16_t mv_  = 0;
    bool     ok_  = true;
    bool     set_ = false;
};

} // namespace soundings
