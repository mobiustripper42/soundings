#pragma once

namespace soundings {

// IPowerRail — the switched sensor rail. The A02YYUW free-runs whenever it has power, so
// leaving it energised between wakes swamps the power budget (HARDWARE_BUILD_PLAN.md §6).
// The rail goes on for the read and off again, and this seam is what the driver holds.
//
// On the Heltec V3 that rail is Vext, driven by GPIO36 and **active LOW** (measured
// 2026-08-20: 3.3 V on, 3.0 mV off — HARDWARE_BUILD_PLAN.md §6, HW-18). That polarity is
// a fact about one board and lives in the ESP32 binding, NOT here: a seam that leaked it
// would have to be rewritten the moment the fallback in F-13 puts an AO3401 back on the
// BOM sourced from VBAT, which is the same rail with the opposite sense.
struct IPowerRail {
    virtual void on()  = 0;
    virtual void off() = 0;
    virtual ~IPowerRail() = default;
};

} // namespace soundings
