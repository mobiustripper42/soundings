#pragma once
#include <stdint.h>

namespace soundings {

// IDistance — the ultrasonic distance seam. The tank node's only sensor sits behind it:
// an A02YYUW that free-runs frames over UART (Phase 3.8 implements the driver). A fake
// drives it in host tests and Wokwi.
//
// Raw values cross the seam: millimetres, exactly as the sensor reports them, NOT
// computed gallons. The distance→volume curve lives gateway-side in Python (DEC-004),
// with the speed-of-sound temperature correction applied there too (DEC-007), so the
// math stays re-revisable against stored raw data without reflashing a node (D1).
struct IDistance {
    // A raw distance reading. ok=false means the read failed — and because sensors are
    // declared, not auto-detected, a declared sensor that doesn't answer is a fault, not
    // a silent gap (DEC-002).
    struct Reading {
        uint16_t raw;   // millimetres; rides the wire as channel 8 TANK_DISTANCE (u16)
        bool     ok;
    };
    virtual Reading read() = 0;
    virtual ~IDistance() = default;
};

} // namespace soundings
