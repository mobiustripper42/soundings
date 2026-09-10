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

uint32_t dsConversionMs(uint8_t configByte) {
    switch (configByte) {
        case kDsConfig9Bit:  return 94;    // 93.75 ms, rounded up to whole ms
        case kDsConfig10Bit: return 188;   // 187.5
        case kDsConfig11Bit: return 375;
        default:             return 750;   // 12-bit, and the safe arm for anything unknown
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

void Ds18b20Temp::healResolution() {
    // Write Scratchpad takes exactly three bytes. TH and TL are unused by this project but
    // cannot be omitted, so the datasheet's own factory values go back in.
    if (!bus_.reset()) return;
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsWriteScratch);
    bus_.writeByte(cfg_.alarmHigh);
    bus_.writeByte(cfg_.alarmLow);
    bus_.writeByte(cfg_.resolution);

    // The one EEPROM write in this sensor's life. Whether it takes is not knowable from here
    // — see the header. The caller has already marked the attempt, so this happens at most
    // once per boot regardless.
    if (!bus_.reset()) return;
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsCopyScratch);
}


ITemp::Reading Ds18b20Temp::read() {
    healAttempted_ = false;

    rail_.on();

    // ⚠ The configuration byte is read BEFORE the conversion, and that ordering is the fix.
    //
    // It says how long this part's conversion actually takes — 94 ms at 9-bit, 750 ms at the
    // 12-bit factory default — and that is a number the driver cannot get any other way. The
    // status bit was supposed to supply it and cannot be trusted to (dsConversionMs). One
    // extra scratchpad read costs about 6 ms against a 94 ms conversion, which is the
    // cheapest of the available wrong answers.
    //
    // This is also the presence check, and there is deliberately no separate bus_.reset()
    // ahead of it: readScratchpad() opens with its own reset, and a standalone one would fail
    // into this identical branch having bought nothing but another ~1 ms pulse per wake.
    // An unplugged probe, a cut lead and a dead part all land here. The rail comes down on
    // every failure path — a stuck rail is invisible from the serial monitor and is the
    // failure that flattens the pack (DEC-006).
    uint8_t pre[kDsScratchpadLen];
    if (!readScratchpad(pre)) {
        rail_.off();
        return Reading{0, false};
    }
    const uint32_t minConvMs = dsConversionMs(pre[4]);

    if (!bus_.reset()) {
        rail_.off();
        return Reading{0, false};
    }
    bus_.writeByte(kDsSkipRom);
    bus_.writeByte(kDsConvertT);

    // Wait, floored, then polled — and the deadline bounds both.
    //
    // The floor is the fix: nothing is asked of the bus until the part has had its full
    // conversion time, because a clone with no busy-signalling answers the very first read
    // slot with 1 while the scratchpad still holds +85 C. The poll still earns its place
    // after that — it catches a part running slower than its nominal time, and on an honest
    // one it costs a single read slot.
    //
    // ⚠ The deadline is checked FIRST, so it still means what its comment says: a deadline
    // that does not cover this part's conversion time is a misconfiguration and fails the
    // read rather than being silently overridden by the floor.
    //
    // ONE clock reading per pass, holding a single instant that the deadline and the floor
    // are both judged against — the same shape and the same reason as A02yyuwDistance::read()
    // (a02yyuw.cpp:87-94). Unsigned subtraction is wrap-safe across the ~49.7-day millis()
    // rollover.
    const uint32_t start = clock_.millis();
    for (;;) {
        const uint32_t since = clock_.millis() - start;

        if (since >= cfg_.conversionDeadlineMs) {
            rail_.off();
            return Reading{0, false};
        }
        if (since < minConvMs) continue;     // still converting, whatever it would claim
        if (bus_.readBit()) break;
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
        // ⚠ MARKED BEFORE THE ATTEMPT, and this is the ENTIRE endurance guarantee — there is
        // no longer a verify standing behind it.
        //
        // A DS18B20 accepts Copy Scratchpad, keeps nothing, and reports no error. The driver
        // cannot tell. An earlier version set the flag only when a verify reported failure,
        // which made the 50,000-write budget depend on that verify telling the truth — and it
        // could not, because it rested on a rail cycle whose voltage collapse this code cannot
        // confirm. A refusing part on a rail that does not fall far enough reported SUCCESS,
        // left the flag clear, and got rewritten on every genuine wake from deep sleep. At a
        // fifteen-minute cadence that is ~35,000 writes a year, through the mechanism meant to
        // prevent it. The verify has since been removed outright (see healResolution).
        //
        // Marking first makes the bound hold whatever the part does. It costs nothing: a part
        // that genuinely took the write comes up 9-bit forever and never enters this branch
        // again, so there is no second attempt to lose.
        flag_.markAttempted();
        healResolution();
    }

    rail_.off();
    return Reading{raw, true};
}

} // namespace soundings
