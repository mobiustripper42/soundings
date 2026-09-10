#pragma once
#include <stdint.h>
#include <stddef.h>
#include "itemp.h"
#include "ionewirebus.h"
#include "ipowerrail.h"
#include "iclock.h"
#include "ihealattemptflag.h"

namespace soundings {

// The DS18B20's wire protocol, and the driver that turns it into one ITemp reading.
//
// Everything here is read from the Analog Devices DS18B20 datasheet supplied 2026-09-05,
// NOT inferred from the part's behaviour and NOT written from memory (issue #94 makes the
// datasheet read its own first step, for exactly this reason).
//
//   bus         : 1-Wire, single-drop. Skip ROM throughout — no ROM search, no addressing.
//   transaction : initialization (reset + presence) -> ROM command -> function command
//   bit order   : least-significant-bit first, handled below the IOneWireBus seam
//   scratchpad  : 9 bytes, byte 8 is a CRC-8 over bytes 0-7
//   temperature : signed 16-bit, 1/16 C per count, two's complement
//
// Power: 3-wire external supply off the switched Ve rail, never parasite power. That is
// what lets the conversion wait poll the bus instead of timing it blind, and it is why no
// strong-pullup MOSFET appears on the BOM (datasheet Figure 6 vs Figure 7).

// ---- Commands ---------------------------------------------------------------

constexpr uint8_t kDsSkipRom        = 0xCC;
constexpr uint8_t kDsConvertT       = 0x44;
constexpr uint8_t kDsWriteScratch   = 0x4E;
constexpr uint8_t kDsReadScratch    = 0xBE;
// ⚠ Copy Scratchpad writes the EEPROM-backed configuration register, which is rated 50,000
// writes. It is issued at most ONCE per sensor by the heal path, and at most once per boot
// on a part that refuses it (ihealattemptflag.h). It must never appear in the ordinary read
// path — at a fifteen-minute cadence that is ~35,000 writes a year and a dead part inside
// seventeen months.
constexpr uint8_t kDsCopyScratch    = 0x48;

constexpr size_t kDsScratchpadLen = 9;

// ---- Configuration register -------------------------------------------------

// Byte 4 of the scratchpad. Bit 7 is 0, bits 6-5 are R1:R0, bits 4-0 read as 1.
// 9-bit is R1:R0 = 0:0, so 0b0001'1111.
constexpr uint8_t kDsConfig9Bit  = 0x1F;
constexpr uint8_t kDsConfig10Bit = 0x3F;
constexpr uint8_t kDsConfig11Bit = 0x5F;
constexpr uint8_t kDsConfig12Bit = 0x7F;   // the factory power-up default

// How many low bits of the temperature register are UNDEFINED at a given resolution.
//
// ⚠ Undefined, not zero. The datasheet: "For 11-bit resolution, bit 0 is undefined. For
// 10-bit resolution, bits 1 and 0 are undefined, and for 9-bit resolution bits 2, 1, and 0
// are undefined." The gateway divides the raw value by TEMP_COUNTS_PER_C = 16.0 and trusts
// it (derive.py:147), so garbage left in those bits is up to 0.4375 C of invented
// temperature riding the wire as though it had been measured. The node masks them so the
// gateway can go on assuming what it already assumed.
uint8_t dsUndefinedLowBits(uint8_t configByte);

// Maximum conversion time at a given resolution, in whole milliseconds, from the datasheet's
// R1/R0 table: 93.75 / 187.5 / 375 / 750 ms.
//
// ⚠ This is a FLOOR the driver must wait, not an estimate it may skip. The datasheet says an
// externally-powered part answers read slots with 0 until conversion completes, and the
// clones this project buys on purpose (HARDWARE_BUILD_PLAN.md:278) frequently do not: one
// measured on the bench 2026-09-09 reported "done" on the first read slot, 0 ms in, while it
// was genuinely still converting (issue #94). Believing it means reading the scratchpad
// before the conversion has written anything into it — which returns the +85 C power-on
// value, and the sentinel check then faults a working probe forever.
//
// The unrecognised-byte arm returns 750, the longest. Waiting too long costs awake time;
// waiting too little costs every reading.
uint32_t dsConversionMs(uint8_t configByte);

// ---- CRC-8 ------------------------------------------------------------------

// Dallas/Maxim CRC-8: polynomial X^8 + X^5 + X^4 + 1, shifted least-significant-bit first,
// shift register initialised to 0. Scratchpad byte 8 covers bytes 0-7.
uint8_t ds18b20Crc8(const uint8_t* data, size_t len);

// ---- Read strategy ----------------------------------------------------------

struct Ds18b20Config {
    // The resolution this node wants, as a configuration-register byte.
    uint8_t resolution = kDsConfig9Bit;

    // ⚠ THIS MUST COVER 750 ms, NOT 9-bit's 93.75 ms, and the reason is not margin.
    //
    // A sensor fresh out of the bag is at its 12-bit factory default, and 12-bit conversion
    // takes up to 750 ms. A deadline tuned to the resolution we WANT would time out that
    // very first wake, return a fault, and never reach the heal step below — so the node
    // would fault forever on a working sensor and never fix the thing that was wrong.
    //
    // It costs nothing in steady state. The part answers a read slot with 1 the moment
    // conversion ends, so this is a backstop against a bus that has stopped answering, not
    // a timer anything waits out.
    uint32_t conversionDeadlineMs = 900;

    // TH and TL. The alarm function is unused, but Write Scratchpad takes THREE bytes and
    // there is no way to send the configuration byte without them. These are the datasheet's
    // own power-up values, so a healed part keeps the alarm thresholds it shipped with.
    uint8_t alarmHigh = 0x4B;   // +75 C
    uint8_t alarmLow  = 0x46;   // +70 C

    // The SENSOR's own range, -55 to +125 C, in raw 1/16 C counts (datasheet Table 1:
    // 0xFC90 and 0x07D0).
    //
    // ⚠ This is NOT a tank-plausibility check. Whether 4 C is a believable headspace for
    // THIS tank depends on the tank, and that derivation lives gateway-side in Python
    // (DEC-004) so it stays re-revisable against stored raw data without reflashing a node
    // — the same rule a02yyuw.h:84 already states for distance.
    int16_t minRaw = -880;
    int16_t maxRaw = 2000;

    // The power-on reset value of the temperature register, +85.0 C (datasheet: bytes 0
    // and 1 power up to 0x0550). A scratchpad carrying it means no conversion completed,
    // not that the tank is at 85 C.
    //
    // ⚠ The tradeoff, stated rather than hidden: a genuine +85.0000 C reading is
    // indistinguishable from the sentinel and would be rejected. That is 185 F in a tank
    // headspace, so the false rejection is not a real case, while the false ACCEPTANCE it
    // prevents — a node reporting a plausible-looking number after a conversion that never
    // ran — is exactly the kind of silent wrong data the fault mask exists to avoid.
    int16_t sentinelRaw = 0x0550;
};

// ---- The driver -------------------------------------------------------------

class Ds18b20Temp : public ITemp {
public:
    Ds18b20Temp(IOneWireBus& bus, IPowerRail& rail, IClock& clock, IHealAttemptFlag& flag,
                const Ds18b20Config& cfg = Ds18b20Config())
        : bus_(bus), rail_(rail), clock_(clock), flag_(flag), cfg_(cfg) {}

    // One call is one complete cycle: rail on, convert, read, validate, heal if the part is
    // at the wrong resolution, rail off. Whole rather than a start/poll/finish trio, for the
    // same reason A02yyuwDistance::read() is (a02yyuw.h:102-106) — the run cycle walks a set
    // of sensors it knows nothing about through ISampler::sample().
    //
    // ⚠ A reading taken at the WRONG resolution is still returned ok. A 12-bit value is
    // more precise than the 9-bit one we asked for, so it is good data; the problem it
    // signals is power, not measurement. Faulting the channel would stop derive.py
    // producing gallons in order to report something that is not a data fault.
    Reading read() override;

    // Did the last read() find the part at the wrong resolution and try to fix it? A bench
    // diagnostic — nothing in the run path consumes it, because a failed heal has no way to
    // reach the gateway yet (issue #98).
    bool healAttemptedLastRead() const { return healAttempted_; }

private:
    // Reads the whole scratchpad into `out`, CRC-checked. Assumes the rail is already on.
    bool readScratchpad(uint8_t* out);

    // Write the wanted configuration to the scratchpad and copy it to EEPROM.
    //
    // ⚠ IT DOES NOT VERIFY, AND CANNOT. A DS18B20 accepts Copy Scratchpad, keeps nothing and
    // reports no error, so the only way to tell is a power cycle — the scratchpad reloads
    // from EEPROM on power-up. This used to do exactly that, and it never worked: a bench
    // sweep on 2026-09-09 found Ve does not fall below the part's reset threshold until
    // between 50 and 100 ms, against the 50 ms the verify allowed. The part never restarted,
    // handed back the RAM copy it had just been given, and reported success unconditionally.
    //
    // Nothing is lost by removing it, because nothing depended on it. The 50,000-write bound
    // is structural — IHealAttemptFlag is marked on ATTEMPT, so a refusing part is written at
    // most once per boot whatever it claims. Returning void rather than a bool nobody could
    // trust is the honest shape.
    void healResolution();

    IOneWireBus&      bus_;
    IPowerRail&       rail_;
    IClock&           clock_;
    IHealAttemptFlag& flag_;
    Ds18b20Config     cfg_;
    bool healAttempted_ = false;
};

} // namespace soundings
