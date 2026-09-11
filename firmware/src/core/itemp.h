#pragma once
#include <stdint.h>

namespace soundings {

// ITemp — the headspace temperature seam. A DS18B20 on the tank lid sits behind it
// (Phase 3.8c implements the driver); a fake drives it in host tests.
//
// Raw values cross the seam: the sensor's own register units, 1/16 °C, exactly as it
// reports them — NOT degrees. The conversion lives gateway-side (`derive.py:147` divides
// by TEMP_COUNTS_PER_C = 16.0), same reasoning as IDistance: the math stays re-revisable
// against stored raw data without reflashing a node (D1, DEC-004).
//
// ⚠ `raw` is SIGNED where IDistance::Reading::raw is unsigned, and that is deliberate
// rather than an inconsistency. The sensor's value genuinely goes below zero — the
// datasheet's Table 1 runs to −55 °C — and an honest seam confines the int16_t→uint16_t
// cast the wire needs to one line in one adapter (temp_sampler.h) instead of spreading a
// reinterpretation through the driver, its tests and its fake.
struct ITemp {
    // A raw temperature reading. ok=false means the read failed — and because sensors are
    // declared, not auto-detected, a declared sensor that doesn't answer is a fault, not a
    // silent gap (DEC-002).
    struct Reading {
        int16_t raw;   // 1/16 °C; rides the wire as channel 4 SOIL_TEMP_0 (i16)
        bool    ok;
    };
    virtual Reading read() = 0;
    virtual ~ITemp() = default;
};

} // namespace soundings
