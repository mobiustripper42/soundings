#pragma once
#include <stdint.h>

namespace soundings {

// IBattery — the pack-voltage seam. On the real node this is an ADC read gated by
// ADC_Ctrl (Phase 3.10); a fake sets the voltage directly in host tests.
//
// Battery voltage is a packet HEADER field (packet.h `battery_mv`), not a channel — every
// packet carries it, so it is never declared and never faulted through channel_mask. A
// failed read is still worth signalling, so the caller can decide what to send rather
// than having a plausible-looking zero invented for it down here.
struct IBattery {
    struct Reading {
        uint16_t millivolts;   // pack voltage; rides the header as battery_mv
        bool     ok;
    };
    virtual Reading read() = 0;
    virtual ~IBattery() = default;
};

} // namespace soundings
