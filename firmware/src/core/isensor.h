#pragma once
#include <stdint.h>

namespace soundings {

// Sensor adapter seam (representative). Every sensor that touches the world sits behind
// an interface like this (the full set — ISoilMoisture, ITemp, IHumidity, IRadio —
// arrives with the node firmware core in Phase 2; CLAUDE.md "adapters everywhere"). A
// real driver implements it at the bench; a fake drives it in host tests and Wokwi.
// Phase 1.1 lands one representative seam to prove the harness.
//
// Raw values cross the seam: the Watermark's raw AC-excitation reading, NOT computed
// kPa. Deriving downstream keeps the math re-revisable against stored raw data (D1).
struct ISoilMoisture {
    // A raw soil-moisture-tension reading. ok=false means the read failed — and because
    // sensors are declared, not auto-detected, a declared sensor that doesn't answer is
    // a fault, not a silent gap (DEC-002).
    struct Reading {
        uint16_t raw;   // raw excitation counts; converted to kPa downstream
        bool     ok;
    };
    virtual Reading read() = 0;
    virtual ~ISoilMoisture() = default;
};

} // namespace soundings
