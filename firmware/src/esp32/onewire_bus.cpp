#include <Arduino.h>
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "onewire_bus.h"

namespace soundings {

// ⚠ WHY THE BIT SLOTS DO NOT USE pinMode()/digitalWrite().
//
// Measured on an ESP32-S3 with `pio run -e ds18probe` (issue #94, 2026-09-09):
//
//     pinMode(OUTPUT)+digitalWrite(LOW) : 14.592 us
//     pinMode(INPUT)                    : 14.148 us
//     write-1 low period (budgeted 6 us): 35.716 us   [datasheet max 15]
//
// A write-1 may hold the line low for 1-15 us. At 35.7 us every 1 bit was delivered as a 0,
// so SkipRom (0xCC) arrived as 0x00 and no command this driver sent was ever understood.
// The failure was invisible from above: reset() holds low for 500 us, where 15 us of
// overhead rounds to nothing, so the bus reported a healthy presence pulse and then returned
// nine bytes of 0xFF — the pull-up, with nothing answering.
//
// Those Arduino calls are not slow by accident. They walk the GPIO matrix, deselect
// peripherals and set drive strength, which is correct work and belongs in begin(). It just
// cannot happen inside a 15 us window.
//
// The register defines rather than the `GPIO` struct: `soc/gpio_struct.h` has changed shape
// across IDF versions (plain fields vs `.val` sub-structs) while `GPIO_ENABLE_W1TS_REG` and
// friends in `soc/gpio_reg.h` have not. This file is pinned to a framework it does not
// control, so it targets the stable one.

void OneWireBus::begin() {
    // The slow, correct path — once, at setup, where 14 us costs nothing. pinMode() is what
    // routes the pad to plain GPIO and enables its input buffer; hand-rolling that would buy
    // nothing and is where this kind of change usually goes wrong.
    pinMode(pin_, INPUT);

    // Park the output latch LOW and leave it there for the life of the program. Every slot
    // below then toggles only the output ENABLE, which is what makes this bus open-drain by
    // construction rather than by care: there is no sequence of calls that can drive the
    // line high into an unpowered probe, because the register that would do it is never
    // written again.
    REG_WRITE(GPIO_OUT_W1TC_REG, mask_);
}

void OneWireBus::driveLow() {
    // Output enable on. The latch is already 0, so the pad goes low. One register write.
    REG_WRITE(GPIO_ENABLE_W1TS_REG, mask_);
}

void OneWireBus::release() {
    // Output enable off — the pad reverts to input and the external 4.7 kOhm on Ve takes the
    // line high. Never INPUT_PULLUP: the internal pull-up is tens of kilohms, too weak for a
    // 5 m lead's capacitance, and it would keep pulling the line up after the rail drops,
    // which is the back-powering path this design exists to avoid (onewire_bus.h:14-16).
    REG_WRITE(GPIO_ENABLE_W1TC_REG, mask_);
}

bool OneWireBus::sample() {
    return (REG_READ(GPIO_IN_REG) & mask_) != 0;
}

bool OneWireBus::reset() {
    // The reset pulse is long enough that it does not need the interrupt guard the bit slots
    // do: a late release makes the low period longer than 480 us, which is legal.
    driveLow();
    delayMicroseconds(t_.resetLowUs);
    release();
    delayMicroseconds(t_.presenceWaitUs);

    // A device answering pulls the line LOW. No device, or an unpowered one, leaves the
    // pull-up holding it high.
    //
    // ⚠ This cannot distinguish a presence pulse from a line with no working pull-up, and
    // with the pull-up on the SWITCHED rail it reports presence unconditionally whenever Ve
    // is down. Callers must not read it as "a probe is fitted" — only Ds18b20Temp's CRC
    // check separates a real part from a bus talking to itself.
    const bool present = !sample();

    delayMicroseconds(t_.resetRecoverUs);
    return present;
}

void OneWireBus::writeBit(bool bit) {
    // ⚠ Interrupts off across the slot. A WiFi or timer interrupt landing between the drive
    // and the release stretches a write-1's low period past 15 us, and the part reads it as
    // a 0. That is a silent corruption — the byte is delivered, it is simply the wrong byte
    // — and it is the classic bit-banged 1-Wire failure on a chip with a radio on it.
    //
    // ⚠ This is a mitigation, not a guarantee: a non-maskable interrupt or a cache miss on
    // flash can still stretch the slot. The guarantee would be the RMT peripheral generating
    // the waveform in hardware, which is a larger change than this one.
    noInterrupts();
    driveLow();
    if (bit) {
        delayMicroseconds(t_.writeSlotLowUs);
        release();
        interrupts();
        delayMicroseconds(t_.writeSlotRestUs);
    } else {
        delayMicroseconds(t_.writeZeroLowUs);
        release();
        interrupts();
        delayMicroseconds(t_.writeZeroRestUs);
    }
}

bool OneWireBus::readBit() {
    noInterrupts();
    driveLow();
    delayMicroseconds(t_.readSlotLowUs);
    release();
    delayMicroseconds(t_.readSampleUs);
    const bool bit = sample();
    interrupts();
    delayMicroseconds(t_.readRestUs);
    return bit;
}

void OneWireBus::writeByte(uint8_t b) {
    // Least-significant-bit first, per the datasheet. Handled here so nothing above the
    // seam has to think about bit order.
    for (uint8_t i = 0; i < 8; ++i) {
        writeBit((b & 0x01) != 0);
        b = (uint8_t)(b >> 1);
    }
}

uint8_t OneWireBus::readByte() {
    uint8_t b = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        if (readBit()) b = (uint8_t)(b | (uint8_t)(1u << i));
    }
    return b;
}

} // namespace soundings
