#pragma once
#include <stdint.h>

namespace soundings {

// IOneWireBus — the 1-Wire bus, at BYTE level.
//
// The line is drawn here, and not lower, because of where testability actually falls. Bit
// timing is microseconds (reset low >= 480 us, a time slot 60-120 us, read data valid
// within 15 us of slot start) and cannot be exercised on a host at all. Everything above
// it — the scratchpad decode, the CRC-8, the sentinel rejection, the resolution check and
// the whole conversion state machine — can be, and that is where the bugs live.
//
// Mirrors IByteSource deliberately: same granularity, same reason.
struct IOneWireBus {
    // Drive the reset pulse and listen for a presence pulse. Returns true when a device
    // answered. A false here means nothing is on the bus — an unplugged probe, a cut lead,
    // a dead part — and the driver treats it as a failed read rather than retrying blind.
    virtual bool reset() = 0;

    // All data moves LEAST-SIGNIFICANT-BIT FIRST on this bus. That is the binding's
    // problem, not the caller's: everything above this seam thinks in whole bytes.
    virtual void writeByte(uint8_t b) = 0;
    virtual uint8_t readByte() = 0;

    // One read time slot, as a bit. Used only for the conversion-in-progress poll: an
    // externally-powered DS18B20 answers read slots with 0 while converting and 1 when
    // done (datasheet, "DS18B20 Function Commands"). Exposed separately from readByte()
    // because issuing eight slots to learn one bit would consume seven the part has no
    // obligation to define.
    virtual bool readBit() = 0;

    virtual ~IOneWireBus() = default;
};

} // namespace soundings
