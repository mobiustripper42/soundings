#pragma once
#include <stdint.h>
#include "ipowerrail.h"

namespace soundings {

// Vext — the Wireless Stick Lite V3's switched sensor rail, behind IPowerRail.
//
// GPIO36, **active LOW**, measured 2026-08-20: 3.3 V on, 3.0 mV off
// (HARDWARE_BUILD_PLAN.md §6, HW-18). The polarity is a fact about this board and it lives
// here rather than in the seam, which is what ipowerrail.h asks for: the F-13 fallback puts
// an AO3401 on VBAT with the opposite sense, and a seam that had leaked the polarity would
// have to be rewritten the day that happens.
//
// The rail is off at reset with no help from us — GPIO36 comes up high-impedance, the
// AO3401's 10K gate pull-up ties its gate to its own source, Vgs = 0 and the FET is off.
// That is the state HW-18 metered at 3.0 mV (HARDWARE_BUILD_PLAN.md:733-736), so nothing
// has to run before the sensor is safely unpowered.
constexpr int8_t kPinVextCtrl = 36;

class VextRail : public IPowerRail {
public:
    // Both calls set pinMode, so neither depends on the other having run first. off() on a
    // freshly-booted node is a legal call and has to leave the pad driven HIGH rather than
    // wherever reset left it.
    void on()  override;
    void off() override;
};

} // namespace soundings
