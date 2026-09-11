#pragma once
#include "itemp.h"

namespace soundings {

// FakeTemp — a scripted headspace temperature, for tests above the driver (the sampler
// adapter, the manifest, the run cycle). Mirrors FakeDistance.
class FakeTemp : public ITemp {
public:
    void setReading(int16_t raw, bool ok = true) { raw_ = raw; ok_ = ok; }
    Reading read() override { ++reads_; return Reading{raw_, ok_}; }
    int reads() const { return reads_; }
private:
    int16_t raw_   = 0;
    bool    ok_    = true;
    int     reads_ = 0;
};

} // namespace soundings
