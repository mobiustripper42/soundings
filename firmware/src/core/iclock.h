#pragma once
#include <stdint.h>

namespace soundings {

// IClock — the millis() seam. The node run cycle is non-blocking: every long action
// times against a monotonic millisecond counter, never delay() (CLAUDE.md "non-blocking
// firmware"). The ESP32 binding wraps Arduino's millis(); host tests drive a FakeClock
// so the run cycle is exercised deterministically with zero real time elapsed.
struct IClock {
    virtual uint32_t millis() = 0;
    virtual ~IClock() = default;
};

} // namespace soundings
