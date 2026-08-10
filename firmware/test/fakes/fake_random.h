#pragma once
#include "irandom.h"

namespace soundings {

// FakeRandom — a scripted entropy source. Values are returned in order and the last one
// repeats, so a test can pin the jitter to an exact expected sleep interval instead of
// asserting a range and hoping.
class FakeRandom : public IRandom {
public:
    static const int kCapacity = 8;

    void push(uint32_t v) { if (count_ < kCapacity) script_[count_++] = v; }

    uint32_t next() override {
        ++calls_;
        if (count_ == 0) return 0;
        if (idx_ >= count_) return script_[count_ - 1];
        return script_[idx_++];
    }

    int calls() const { return calls_; }

private:
    uint32_t script_[kCapacity] = {};
    int      count_ = 0;
    int      idx_   = 0;
    int      calls_ = 0;
};

} // namespace soundings
