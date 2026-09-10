#pragma once
#include <stdint.h>
#include "ionewirebus.h"

namespace soundings {

// Bit-banged 1-Wire on GPIO7, behind IOneWireBus.
//
// GPIO7 is J3 pin 17 on the HTIT-WSL V3 Rev 1.1, listed in the datasheet's Table 2.2-2 as
// `GPIO7, ADC1_CH6, TOUCH7` — no committed function and, load-bearingly, NOT a strapping
// pin. A 4.7 kOhm pull-up holding a strapping pin at the wrong level through reset stops the
// board booting, and it presents as a dead board rather than as a wiring mistake.
//
// ⚠ THE PULL-UP GOES TO Ve, THE SWITCHED RAIL — NOT TO 3V3. On the always-on rail it
// back-powers the DS18B20 through DQ, and the rail-off state stops being off, which defeats
// the whole reason VextRail exists (HARDWARE_BUILD_PLAN.md §8).
//
// ⚠ NOTHING BELOW THIS SEAM IS HOST-TESTED, and it cannot be: every number here is
// microseconds, and the native test runner has no pin to toggle. That is precisely why the
// seam is at byte level (ionewirebus.h) — the scratchpad decode, the CRC-8, the sentinel,
// the masking and the whole heal state machine live above it and ARE tested. What is
// unproven until it sits on a bench is this file and this file only.
constexpr int8_t kPinOneWireDq = 7;

// Timings, from the datasheet's AC Electrical Characteristics: reset low >= 480 us, reset
// high >= 480 us, presence pulse 60-240 us low appearing 15-60 us after release; time slot
// 60-120 us, recovery >= 1 us, write-0 low 60-120 us, write-1 low 1-15 us, read data valid
// within 15 us of slot start.
//
// The values chosen sit comfortably inside those windows rather than at their edges, since
// the only thing on the other side of a missed window is a probe on a 5 m lead in a tank.
struct OneWireTiming {
    uint16_t resetLowUs      = 500;
    uint16_t presenceWaitUs  = 70;    // sample here: 15-60 us after release, pulse is 60-240
    uint16_t resetRecoverUs  = 430;   // 70 + 430 = 500 >= the 480 us high requirement
    uint8_t  writeSlotLowUs  = 6;     // write-1: 1-15 us low
    uint8_t  writeSlotRestUs = 64;    // 6 + 64 = 70 us, inside the 60-120 us slot
    uint8_t  writeZeroLowUs  = 60;    // write-0: 60-120 us low
    uint8_t  writeZeroRestUs = 10;
    uint8_t  readSlotLowUs   = 6;
    // 6 + 6 = 12 us. Was 9, putting the sample at exactly 15 — the limit itself, in a struct
    // whose own rule two paragraphs up is to sit comfortably inside the windows rather than
    // at their edges. That never bit, because ~28 us of Arduino GPIO overhead pushed the real
    // sample out to ~43 us, inside a read-0's 60 us hold by luck. onewire_bus.cpp now hits
    // these numbers to a fraction of a microsecond, so 15 stops being luck and starts being
    // a limit (issue #94).
    uint8_t  readSampleUs    = 6;
    uint8_t  readRestUs      = 55;    // 6 + 6 + 55 = 67 us, inside the 60-120 us slot
};

// ⚠ CORRECT ONLY FOR A PIN BELOW 32. The bit slots write GPIO_ENABLE_W1TS_REG and read
// GPIO_IN_REG, which cover GPIO0-31; pins 32 and up live in a second bank behind different
// registers (GPIO_ENABLE1_*, GPIO_IN1_REG) and would silently address the wrong pin rather
// than fail. kPinOneWireDq is 7, and this file is board-specific already — it names J3 pin 17
// — so the constraint is documented rather than abstracted over.
class OneWireBus : public IOneWireBus {
public:
    explicit OneWireBus(int8_t pin = kPinOneWireDq, const OneWireTiming& t = OneWireTiming())
        : pin_(pin), t_(t), mask_(1u << (pin & 31)) {}

    // Idles the line released (input, external pull-up holds it high). Safe to call before
    // the rail is up: an input pin cannot back-power anything.
    void begin();

    bool reset() override;
    void writeByte(uint8_t b) override;
    uint8_t readByte() override;
    bool readBit() override;

private:
    // Open-drain, always. The line is driven LOW by making the pad an output at 0, and
    // released by making it an input — never driven high. A push-pull high would fight the
    // part's own pull-down during a presence pulse and could source current into an
    // unpowered sensor through its protection diodes.
    void driveLow();
    void release();
    bool sample();

    void writeBit(bool bit);

    int8_t        pin_;
    OneWireTiming t_;
    uint32_t      mask_;   // 1 << pin, precomputed: a shift per bit slot is a shift too many
};

} // namespace soundings
