#pragma once
#include <stdint.h>

namespace soundings {

// ISampler — one channel's worth of reading, with the sensor's type erased.
//
// The run cycle walks a SET of sensors and must not know what any of them are: a tank node
// carries a distance sensor, a bed node a Watermark and a soil probe, and the cycle is the
// same cycle. Every sensor seam already agrees on the shape ({raw value, did it answer}),
// so this is the one interface the cycle needs to be node-type-agnostic. Phase 3.5 fills
// the slot array from a declared manifest; this task takes it pre-built.
struct ISampler {
    struct Sample {
        uint16_t raw;   // whatever this channel's registry entry means (contracts/packet-v1.md)
        bool     ok;    // false => declared but did not answer => a fault, not a gap (DEC-002)
    };
    virtual Sample sample() = 0;
    virtual ~ISampler() = default;
};

} // namespace soundings
