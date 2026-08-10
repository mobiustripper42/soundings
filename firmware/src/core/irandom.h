#pragma once
#include <stdint.h>

namespace soundings {

// IRandom — the entropy seam, and it exists for exactly one reason: wake jitter has to be
// ASSERTED, not observed. A cycle that calls esp_random() directly can only be tested by
// running it many times and eyeballing the spread, which is a test that passes on a
// generator stuck at a constant. With the source injected, a scripted value pins the sleep
// interval to a single expected number, and the ±30 s window can be checked at both
// extremes rather than sampled.
struct IRandom {
    virtual uint32_t next() = 0;
    virtual ~IRandom() = default;
};

} // namespace soundings
