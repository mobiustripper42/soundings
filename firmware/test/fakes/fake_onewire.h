#pragma once
#include "ionewirebus.h"
#include "ds18b20.h"
#include "fake_powerrail.h"
#include "fake_clock.h"

namespace soundings {

// FakeOneWireBus — a DS18B20 on a bench, not a recording of one driver run.
//
// ⚠ Scripted from the datasheet's own byte sequences and timings, deliberately, NOT from
// whatever makes the driver pass. A fake is only ever checked once something consumes it,
// and this project has already been bitten: the emitter's Watermark drydown was wrong from
// the day it was written and nothing could notice until a later task ran a real curve
// through it.
//
// Three properties are modelled because the driver's correctness turns on them, and a
// simpler stand-in would let real bugs through:
//
//   1. POWER. The part is unpowered when the rail is off, and answers no presence pulse.
//      On power-up the scratchpad RELOADS FROM EEPROM — which is the entire mechanism the
//      heal path's verify step depends on, and the reason a driver that forgets rail.on()
//      fails here rather than on silicon.
//
//   2. CONVERSION TIME FOLLOWS RESOLUTION. 9-bit is 93.75 ms, 12-bit is 750 ms
//      (datasheet, R1/R0 table). A part at its factory default genuinely takes 750 ms, so
//      a deadline tuned to 9-bit times out against this fake exactly as it would in a tank.
//
//   3. UNDEFINED BITS CARRY GARBAGE. At 9-bit the datasheet says bits 2, 1 and 0 are
//      undefined; this fake fills them with 1s, the worst case. A driver that fails to
//      mask reads 0.4375 C too high and the test says so.
class FakeOneWireBus : public IOneWireBus {
public:
    FakeOneWireBus(FakePowerRail& rail, FakeClock& clock) : rail_(rail), clock_(clock) {}

    // ---- Bench controls ----------------------------------------------------

    // What the part's EEPROM holds, and therefore what it powers up at. A sensor out of the
    // bag is 12-bit (datasheet: power-up default).
    void setEepromConfig(uint8_t cfg) { eepromConfig_ = cfg; reloadFromEeprom(); }

    // The temperature a completed conversion produces, in raw 1/16 C counts, before the
    // undefined low bits are filled in.
    void setTemperature(int16_t raw) { trueRaw_ = raw; }

    void setPresent(bool present) { present_ = present; }

    // The EEPROM silently refuses to take a Copy Scratchpad — a worn or faulty part. This
    // is the condition IHealAttemptFlag exists to bound.
    void refuseEepromWrite(bool refuse) { refuseEeprom_ = refuse; }

    // Emit a scratchpad whose CRC byte does not match its contents.
    void corruptCrc(bool corrupt) { corruptCrc_ = corrupt; }

    // The bus stops answering mid-conversion — a cut lead, a dead part. Read slots never
    // return 1, so only the deadline ends the wait.
    void neverFinishConversion(bool never) { neverFinish_ = never; }

    int  copyScratchpadCount()  const { return copyCount_; }
    int  writeScratchpadCount() const { return writeCount_; }
    uint8_t eepromConfig()      const { return eepromConfig_; }
    uint8_t liveConfig()        const { return pad_[4]; }

    // ---- IOneWireBus -------------------------------------------------------

    bool reset() override {
        syncPower();
        state_ = powered_ ? kAwaitRom : kIdle;
        readIndex_ = 0;
        return powered_ && present_;
    }

    void writeByte(uint8_t b) override {
        syncPower();
        if (!powered_) return;

        switch (state_) {
            case kAwaitRom:
                // Single-drop bus: Skip ROM throughout. Anything else is a driver bug, and
                // silently ignoring it would let a driver that never addresses the part
                // pass — so the part simply stops responding, as a real one would.
                state_ = (b == kDsSkipRom) ? kAwaitFunction : kIdle;
                return;

            case kAwaitFunction:
                switch (b) {
                    case kDsConvertT:
                        convertEndsAtMs_ = clock_.millis() + conversionMsFor(pad_[4]);
                        state_ = kConverting;
                        return;
                    case kDsReadScratch:
                        refreshTemperatureBytes();
                        state_ = kReadingScratch;
                        return;
                    case kDsWriteScratch:
                        ++writeCount_;
                        state_    = kWritingScratch;
                        writeIdx_ = 0;
                        return;
                    case kDsCopyScratch:
                        ++copyCount_;
                        // The one EEPROM write. A refusing part takes the command and does
                        // nothing — no error, no signal. That silence is the whole reason
                        // the driver has to verify by power-cycling rather than assuming.
                        if (!refuseEeprom_) eepromConfig_ = pad_[4];
                        state_ = kIdle;
                        return;
                    default:
                        state_ = kIdle;
                        return;
                }

            case kWritingScratch:
                // Write Scratchpad sends exactly three bytes: TH, TL, configuration. The
                // alarm bytes are unused by this project but cannot be skipped.
                if (writeIdx_ == 0)      pad_[2] = b;
                else if (writeIdx_ == 1) pad_[3] = b;
                else if (writeIdx_ == 2) pad_[4] = b;
                if (++writeIdx_ >= 3) state_ = kIdle;
                return;

            default:
                return;
        }
    }

    uint8_t readByte() override {
        syncPower();
        if (!powered_ || state_ != kReadingScratch) return 0xFF;
        if (readIndex_ >= kDsScratchpadLen) return 0xFF;
        return pad_[readIndex_++];
    }

    bool readBit() override {
        syncPower();
        if (!powered_) return false;
        if (state_ != kConverting) return true;
        if (neverFinish_) return false;
        return clock_.millis() >= convertEndsAtMs_;
    }

private:
    enum State { kIdle, kAwaitRom, kAwaitFunction, kConverting, kReadingScratch, kWritingScratch };

    // Max conversion time per resolution, datasheet R1/R0 table.
    static uint32_t conversionMsFor(uint8_t cfg) {
        switch (cfg) {
            case kDsConfig9Bit:  return 94;    // 93.75 ms, rounded up to whole ms
            case kDsConfig10Bit: return 188;
            case kDsConfig11Bit: return 375;
            default:             return 750;
        }
    }

    // Which low bits the datasheet declares undefined at this resolution.
    static uint8_t undefinedMaskFor(uint8_t cfg) {
        switch (cfg) {
            case kDsConfig9Bit:  return 0x07;
            case kDsConfig10Bit: return 0x03;
            case kDsConfig11Bit: return 0x01;
            default:             return 0x00;
        }
    }

    void syncPower() {
        // ⚠ Watches the rail's off COUNT, not its level, and that distinction is the whole
        // fidelity of this fake.
        //
        // The heal path's verify does rail.off() immediately followed by rail.on(), with no
        // bus traffic in between. A fake that only sampled the level would see "on" both
        // times it was asked, conclude nothing had happened, and hand back the scratchpad
        // RAM the driver had just written — reporting a successful EEPROM write on a part
        // that silently refused one. That is precisely the failure the verify step exists to
        // catch, and the first version of this fake could not see it.
        //
        // A real part loses power whether or not anyone is mid-conversation with it.
        const int offs = rail_.offCount();
        if (offs != lastOffCount_) {
            lastOffCount_ = offs;
            powered_ = false;
            state_   = kIdle;
        }
        const bool nowOn = rail_.isOn();
        if (nowOn && !powered_) reloadFromEeprom();   // power-up: scratchpad <- EEPROM
        if (!nowOn) state_ = kIdle;
        powered_ = nowOn;
    }

    void reloadFromEeprom() {
        pad_[2] = 0x4B;          // TH, factory
        pad_[3] = 0x46;          // TL, factory
        pad_[4] = eepromConfig_;
        pad_[5] = 0xFF;
        // ⚠ Bytes 5-7 are reserved. The datasheet gives power-up values for 5 and 7 only;
        // 0x0C for byte 6 is this fake's own choice, not a datasheet fact. The driver must
        // never read bytes 5-7 for anything but the CRC, and if one ever does, that is the
        // bug this comment exists to make obvious.
        pad_[6] = 0x0C;
        pad_[7] = 0x10;
        refreshTemperatureBytes();
    }

    void refreshTemperatureBytes() {
        const uint8_t undef = undefinedMaskFor(pad_[4]);
        // Fill the undefined bits with 1s — the worst case, and the one that catches a
        // driver that forgets to mask.
        const uint16_t emitted = (uint16_t)((((uint16_t)trueRaw_) & (uint16_t)~(uint16_t)undef)
                                            | (uint16_t)undef);
        pad_[0] = (uint8_t)(emitted & 0xFF);
        pad_[1] = (uint8_t)(emitted >> 8);
        pad_[8] = ds18b20Crc8(pad_, 8);
        if (corruptCrc_) pad_[8] = (uint8_t)(pad_[8] ^ 0xFF);
    }

    FakePowerRail& rail_;
    FakeClock&     clock_;

    uint8_t  pad_[kDsScratchpadLen] = {};
    uint8_t  eepromConfig_ = kDsConfig12Bit;   // out of the bag
    int16_t  trueRaw_      = 0;
    bool     present_      = true;
    bool     powered_      = false;
    bool     refuseEeprom_ = false;
    bool     corruptCrc_   = false;
    bool     neverFinish_  = false;

    State    state_    = kIdle;
    int      lastOffCount_ = 0;
    size_t   readIndex_ = 0;
    uint8_t  writeIdx_  = 0;
    uint32_t convertEndsAtMs_ = 0;
    int      copyCount_  = 0;
    int      writeCount_ = 0;
};

} // namespace soundings
