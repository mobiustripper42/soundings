#pragma once
#include "idownlinkhandler.h"

namespace soundings {

// FakeDownlinkHandler — records what the cycle handed it, and when.
//
// `sleepsWhenCalled` is the ordering evidence: the handler must run BEFORE the node
// sleeps, because on hardware the sleep resets the MCU and there is no afterwards. A test
// that only counted calls would pass against a cycle that ran the handler after the sleep
// — which is green on the host, where FakeSleeper returns, and dead code on a node. That
// is the exact failure runcycle.h warns about in capitals, so the fake makes it visible
// rather than leaving it to be reasoned about.
class FakeDownlinkHandler : public IDownlinkHandler {
public:
    explicit FakeDownlinkHandler(const FakeSleeper& sleeper) : sleeper_(sleeper) {}

    void onDownlink(const Downlink& d) override {
        ++calls_;
        last_ = d;
        sleepsWhenCalled_ = sleeper_.sleepCount();
    }

    int             calls() const { return calls_; }
    const Downlink& last()  const { return last_; }
    int sleepsWhenCalled()  const { return sleepsWhenCalled_; }

private:
    const FakeSleeper& sleeper_;
    int      calls_ = 0;
    Downlink last_{};
    int      sleepsWhenCalled_ = -1;
};

} // namespace soundings
