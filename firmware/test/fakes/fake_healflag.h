#pragma once
#include "ihealattemptflag.h"

namespace soundings {

// FakeHealAttemptFlag — RAM stand-in for RTC slow memory. Like FakeSeqStore, the fake
// outliving a simulated wake is the point: a test constructs a second Ds18b20Temp around
// the same flag to model the next wake after a deep sleep, and the flag is what must
// survive that.
class FakeHealAttemptFlag : public IHealAttemptFlag {
public:
    explicit FakeHealAttemptFlag(bool initial = false) : attempted_(initial) {}
    bool attempted() const override { return attempted_; }
    void markAttempted() override { attempted_ = true; ++marks_; }
    int marks() const { return marks_; }
private:
    bool attempted_ = false;
    int  marks_     = 0;
};

} // namespace soundings
