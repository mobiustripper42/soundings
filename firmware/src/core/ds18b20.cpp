#include "ds18b20.h"

namespace soundings {

uint8_t dsUndefinedLowBits(uint8_t configByte) {
    // Datasheet: "For 11-bit resolution, bit 0 is undefined. For 10-bit resolution, bits 1
    // and 0 are undefined, and for 9-bit resolution bits 2, 1, and 0 are undefined."
    //
    // The default arm is 12-bit — every bit real, nothing masked. It is also where an
    // unrecognised configuration byte lands, and that is the safe direction: masking bits
    // that turned out to be meaningful loses precision, where NOT masking bits that turned
    // out to be undefined invents temperature the sensor never measured.
    switch (configByte) {
        case kDsConfig9Bit:  return 0x07;
        case kDsConfig10Bit: return 0x03;
        case kDsConfig11Bit: return 0x01;
        default:             return 0x00;
    }
}

uint8_t ds18b20Crc8(const uint8_t* data, size_t len) {
    // X^8 + X^5 + X^4 + 1, shifted least-significant-bit first, register initialised to 0.
    // 0x8C is that polynomial reflected, which is what an LSB-first shift wants.
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint8_t mix = (uint8_t)((crc ^ b) & 0x01);
            crc = (uint8_t)(crc >> 1);
            if (mix) crc = (uint8_t)(crc ^ 0x8C);
            b = (uint8_t)(b >> 1);
        }
    }
    return crc;
}

bool Ds18b20Temp::readScratchpad(uint8_t* out) {
    if (!bus_.reset()) return false;
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsReadScratch);
    for (size_t i = 0; i < kDsScratchpadLen; ++i) out[i] = bus_.readByte();
    // Byte 8 covers bytes 0-7. A mismatch is a bad read, not a bad sensor — but the driver
    // has no way to tell those apart and no business guessing, so it fails the read and
    // lets the fault mask say so (DEC-002).
    return ds18b20Crc8(out, 8) == out[8];
}

bool Ds18b20Temp::healResolution() {
    // Write Scratchpad takes exactly three bytes. TH and TL are unused by this project but
    // cannot be omitted, so the datasheet's own factory values go back in.
    if (!bus_.reset()) return false;
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsWriteScratch);
    bus_.writeByte(cfg_.alarmHigh);
    bus_.writeByte(cfg_.alarmLow);
    bus_.writeByte(cfg_.resolution);

    // The one EEPROM write in this sensor's life.
    if (!bus_.reset()) return false;
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsCopyScratch);

    // ⚠ The verify, and the reason it is a POWER CYCLE rather than a read.
    //
    // Reading the scratchpad back here would return the RAM copy we just wrote, which says
    // nothing about whether the EEPROM took it — a refusing part accepts Copy Scratchpad in
    // silence and reports no error. The scratchpad reloads from EEPROM on power-up, so
    // cycling the rail we already own is what turns "we sent the command" into "the part
    // kept it". It costs one extra rail cycle, on one wake, once per sensor.
    rail_.off();
    rail_.on();

    uint8_t pad[kDsScratchpadLen];
    if (!readScratchpad(pad)) return false;
    return pad[4] == cfg_.resolution;
}

ITemp::Reading Ds18b20Temp::read() {
    healAttempted_ = false;
    healSucceeded_ = false;

    rail_.on();

    if (!bus_.reset()) {
        // No presence pulse: an unplugged probe, a cut lead, a dead part. The rail comes
        // down on every failure path — a stuck rail is invisible from the serial monitor and
        // is the failure that flattens the pack (DEC-006).
        rail_.off();
        return Reading{0, false};
    }
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsConvertT);

    // Externally powered, so the part answers read slots with 0 while converting and 1 when
    // done. Polling that beats timing 93.75 ms blind, and it is what lets one deadline cover
    // both 9-bit and the 12-bit factory default without waiting out the difference.
    const uint32_t start = clock_.millis();
    for (;;) {
        if (bus_.readBit()) break;
        // Unsigned subtraction, wrap-safe across the ~49.7-day millis() rollover — same as
        // Elapsed and as A02yyuwDistance::read().
        if (clock_.millis() - start >= cfg_.conversionDeadlineMs) {
            rail_.off();
            return Reading{0, false};
        }
    }

    uint8_t pad[kDsScratchpadLen];
    if (!readScratchpad(pad)) {
        rail_.off();
        return Reading{0, false};
    }

    // Byte 4 is the configuration register, and it arrives free inside the read we had to do
    // anyway. It does two jobs: it says which low bits to mask, and it says whether this part
    // needs healing.
    const uint8_t liveConfig = pad[4];

    // Masked against the resolution the part is ACTUALLY at, not the one we asked for. A
    // sensor still at 12-bit has every bit real, and masking it would throw away precision
    // to enforce a setting that has not taken yet.
    const uint16_t undefined = (uint16_t)dsUndefinedLowBits(liveConfig);
    uint16_t bits = (uint16_t)(((uint16_t)pad[1] << 8) | (uint16_t)pad[0]);
    bits = (uint16_t)(bits & (uint16_t)~undefined);
    const int16_t raw = (int16_t)bits;

    if (raw == cfg_.sentinelRaw) {
        rail_.off();
        return Reading{0, false};
    }
    if (raw < cfg_.minRaw || raw > cfg_.maxRaw) {
        rail_.off();
        return Reading{0, false};
    }

    // ⚠ The heal runs AFTER the reading has been validated, and the reading is returned ok
    // either way. A part at the wrong resolution produces good data — finer data, in fact —
    // and the problem it signals is awake time, not measurement. Faulting the channel would
    // stop derive.py producing gallons in order to report a power problem.
    if (liveConfig != cfg_.resolution && !flag_.attempted()) {
        healAttempted_ = true;
        healSucceeded_ = healResolution();
        // Only a FAILED heal is remembered. A part that took the write is correct from here
        // on and will never match this branch again, so there is nothing to store.
        if (!healSucceeded_) flag_.markAttempted();
    }

    rail_.off();
    return Reading{raw, true};
}

} // namespace soundings
