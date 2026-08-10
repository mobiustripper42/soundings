#pragma once
#include "isampler.h"

namespace soundings {

// FakeSampler — a type-erased sensor for run-cycle tests that do not care which sensor a
// slot holds. Same script-then-hold semantics as the other fakes, and the same refusal to
// invent a plausible zero: unscripted reads as a failure.
class FakeSampler : public ISampler {
public:
    static const int kCapacity = 8;

    void push(uint16_t raw, bool ok = true) {
        if (count_ < kCapacity) script_[count_++] = Sample{raw, ok};
    }

    Sample sample() override {
        ++calls_;
        if (count_ == 0) return Sample{0, false};
        if (idx_ >= count_) return script_[count_ - 1];
        return script_[idx_++];
    }

    int calls() const { return calls_; }

private:
    Sample script_[kCapacity] = {};
    int    count_ = 0;
    int    idx_   = 0;
    int    calls_ = 0;
};

} // namespace soundings
