#include <Arduino.h>
#include "onewire_bus.h"

namespace soundings {

void OneWireBus::begin() {
    release();
}

void OneWireBus::driveLow() {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
}

void OneWireBus::release() {
    // INPUT, not INPUT_PULLUP. The external 4.7 kOhm on Ve is the pull-up the bus is
    // specified around; the ESP32's internal one is tens of kilohms and too weak to meet
    // the rise time a 5 m lead's capacitance imposes. Enabling both would also mean the pad
    // keeps pulling the line up after the rail drops, which is the back-powering path this
    // design goes out of its way to avoid.
    pinMode(pin_, INPUT);
}

bool OneWireBus::sample() {
    return digitalRead(pin_) != 0;
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
    const bool present = !sample();

    delayMicroseconds(t_.resetRecoverUs);
    return present;
}

void OneWireBus::writeBit(bool bit) {
    // ⚠ Interrupts off across the slot. A WiFi or timer interrupt landing between the drive
    // and the release stretches a write-1's low period past 15 us, and the part reads it as
    // a 0. That is a silent corruption — the byte is delivered, it is simply the wrong byte
    // — and it is the classic bit-banged 1-Wire failure on a chip with a radio on it.
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
