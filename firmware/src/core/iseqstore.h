#pragma once
#include <stdint.h>

namespace soundings {

// ISeqStore — the sequence counter's home across a reset.
//
// This seam exists because deep sleep is terminal (isleeper.h): the MCU resets, so a
// `seq` held in a variable would be 0 in every packet the node ever sends. A constant
// sequence number is worse than none — it looks like a working field, and the gateway-side
// gap detector that consumes it (issue #30, telling a drydown hole from a plateau) would
// be built on a number that never moves.
//
// The ESP32 binding is RTC slow memory (`RTC_DATA_ATTR`), which survives deep sleep but
// not a power cycle — a fresh battery legitimately restarts the sequence, and the gateway
// must treat a backwards jump as a restart rather than as loss. The fake keeps it in RAM.
struct ISeqStore {
    virtual uint16_t load() const = 0;
    virtual void store(uint16_t seq) = 0;
    virtual ~ISeqStore() = default;
};

} // namespace soundings
