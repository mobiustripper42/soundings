#pragma once
#include "iseqstore.h"

namespace soundings {

// FakeSeqStore — RAM stand-in for RTC slow memory. Because the fake outlives a simulated
// reset (the test just constructs a new RunCycle around the same store), it models the
// real thing's one interesting property: the counter survives the cycle that wrote it.
class FakeSeqStore : public ISeqStore {
public:
    explicit FakeSeqStore(uint16_t initial = 0) : seq_(initial) {}
    uint16_t load() const override { return seq_; }
    void store(uint16_t seq) override { seq_ = seq; ++writes_; }
    int writes() const { return writes_; }
private:
    uint16_t seq_    = 0;
    int      writes_ = 0;
};

} // namespace soundings
