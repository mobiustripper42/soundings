#pragma once
#include <stdint.h>

namespace soundings {

// ISleeper — the give-up-the-CPU seam. Deliberately NOT a method on IClock: reading the
// clock and surrendering the processor are different capabilities, and the real ones
// differ in kind.
//
// ⚠ On the ESP32, deep sleep DOES NOT RETURN — the MCU resets and comes back through
// setup(). So sleepFor() is allowed to be terminal, and a caller must treat everything
// after the call as unreachable on real hardware. Hiding that behind IClock, whose every
// other method returns normally, would make the run cycle look like a loop when on-target
// it is a boot-sample-sleep. The fake DOES return (a test process that reset would be
// useless), which is exactly why the constraint is written here rather than inferred from
// the fake's behaviour.
struct ISleeper {
    virtual void sleepFor(uint32_t ms) = 0;
    virtual ~ISleeper() = default;
};

} // namespace soundings
