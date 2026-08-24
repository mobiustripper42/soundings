#pragma once
#include "downlink.h"

namespace soundings {

// IDownlinkHandler — what the node DOES about a downlink it heard.
//
// The seam runcycle.h has been pointing at since 3.9b: *"the OTA path that issue #76 will
// add inside the cycle itself."* Inside is the operative word. The cycle is terminal on
// hardware — deep sleep resets the MCU (isleeper.h) — so there is no "after runOnce()" in
// the field, and anything that acts on a downlink has to act before the sleep or never.
//
// Kept as a seam rather than an if-branch in RunCycle for the usual reason: the real
// implementation drags in WiFi, HTTP and Update, none of which compile for the native
// test runner, and all of which would make the run cycle untestable on the host if they
// were reachable from it. The cycle stays platform-independent; the OTA client is a
// binding, and a fake stands in for it in tests and at the bench.
struct IDownlinkHandler {
    // Called once, with a downlink already known to be valid and addressed to this node
    // (decodeDownlink has run). Not called at all for a quiet window, which is the
    // ordinary case — silence is a legal answer (contracts/downlink-v1.md).
    //
    // ⚠ This runs INSIDE the wake, before deep sleep, on a node whose whole power budget
    // is ~2 s of awake time (HARDWARE_BUILD_PLAN.md §6). An implementation that blocks
    // is spending the pack. Whatever it does must bound itself.
    virtual void onDownlink(const Downlink& d) = 0;
    virtual ~IDownlinkHandler() = default;
};

} // namespace soundings
