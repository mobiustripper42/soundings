#include <Arduino.h>
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "ds18b20.h"
#include "onewire_bus.h"
#include "vext_rail.h"

// ds18probe — how long after Ve comes up does the DS18B20 answer a presence pulse?
// A bench instrument for issue #94, written because the headspace channel faults while the
// A02YYUW on the same rail reads fine.
//
//   pio run -e ds18probe -t upload --upload-port /dev/ttyUSB1
//
// ⚠ The port is the NODE's. Both CP2102s report USB serial 0001 and nothing in software
// tells them apart — see platformio.ini. Re-measure by unplugging one.
//
// ⚠ IT EXISTS BECAUSE A FAULT BIT IS NOT A DIAGNOSIS. The headspace channel faulted while
// the A02YYUW on the same rail read fine, and `fault_mask` cannot say whether that is a cut
// lead, a dead part, a bus that never answers, or a driver that cannot talk. Three separate
// theories were wrong before this program was written; each control below is one of them,
// kept as an assertion rather than a memory.
//
//   CONTROL 1 — rail on, wait two full seconds, then ask ten times. The part has had all the
//   time it could want, so this cannot be a settle-window problem. Zero presence pulses here
//   means the bus is broken and no firmware change will help.
//
//   CONTROL 2 — how long a write-1 actually holds the line low, measured both ways. This is
//   the one that found the real bug: pinMode()/digitalWrite() cost ~14 us each, so a slot
//   budgeted at 6 us ran 35.7 us against a 15 us datasheet limit, and every 1 bit went out as
//   a 0. Direct register access puts it at 6.9 us.
//
//   CONTROL 3 — whether Ve genuinely collapses when the rail is switched off, and for how
//   long it has to be off. This is what removed the heal's verify.
//
//   LATENCY + WALK — power-cycle, time the first presence pulse, then run the exact sequence
//   Ds18b20Temp::read() runs and report which stage loses it. The blind-wait control inside
//   it is what proved the part lies about conversion-complete.
//
// ⚠ Do not leave a board in this program. It holds a sensor rail on and never sleeps, which
// is the opposite of the node's design. `park` is how a board is left quiet.

using namespace soundings;

namespace {

OneWireBus g_bus;
VextRail   g_rail;

constexpr uint32_t kDqMask     = 1u << kPinOneWireDq;   // GPIO7, low bank
constexpr uint32_t kDeadlineMs = 3000;   // generous: we are measuring, not budgeting
constexpr uint32_t kOffMs      = 250;    // let Ve fall properly between cycles
constexpr uint32_t kGapMs      = 2000;   // breathing room between measurements

uint32_t g_cycle     = 0;
bool     g_dumpedPad = false;

// The scratchpad is worth printing once: byte 4 is the config register, which says whether
// this part is at the 12-bit factory default or has already been healed to 9-bit, and the
// whole nine bytes distinguish "answering with plausible data" from "answering with 0xFF",
// which is what a floating line reads as.
void dumpScratchpad(const char* why) {
    if (!g_bus.reset()) {
        Serial.printf("    scratchpad (%s): no presence pulse\n", why);
        return;
    }
    g_bus.writeByte(kDsSkipRom);
    g_bus.writeByte(kDsReadScratch);

    uint8_t pad[kDsScratchpadLen];
    for (size_t i = 0; i < kDsScratchpadLen; ++i) pad[i] = g_bus.readByte();

    Serial.printf("    scratchpad (%s):", why);
    for (size_t i = 0; i < kDsScratchpadLen; ++i) Serial.printf(" %02X", (unsigned)pad[i]);

    const char* res = "unknown";
    if (pad[4] == kDsConfig9Bit)       res = "9-bit";
    else if (pad[4] == kDsConfig10Bit) res = "10-bit";
    else if (pad[4] == kDsConfig11Bit) res = "11-bit";
    else if (pad[4] == kDsConfig12Bit) res = "12-bit (factory default)";
    Serial.printf("  | config=0x%02X %s\n", (unsigned)pad[4], res);

    // Raw temperature, printed WITHOUT the driver's masking or band check, because this
    // program is asking whether the part talks — not whether the driver decodes it.
    const int16_t raw = (int16_t)(((uint16_t)pad[1] << 8) | (uint16_t)pad[0]);
    Serial.printf("    raw=%d counts -> %d.%02d C (unmasked)\n",
                  (int)raw, (int)(raw / 16), (int)(abs(raw % 16) * 100 / 16));

    bool allOnes = true;
    for (size_t i = 0; i < kDsScratchpadLen; ++i) if (pad[i] != 0xFF) allOnes = false;
    if (allOnes) {
        Serial.println("    ^ ALL 0xFF — that is a released line with a pull-up and nothing "
                       "answering, not a reading.");
    }
}

// Read the scratchpad into `out`. Returns false on no presence pulse or a CRC mismatch.
bool readPad(uint8_t* out) {
    if (!g_bus.reset()) return false;
    g_bus.writeByte(kDsSkipRom);
    g_bus.writeByte(kDsReadScratch);
    for (size_t i = 0; i < kDsScratchpadLen; ++i) out[i] = g_bus.readByte();
    return ds18b20Crc8(out, 8) == out[8];
}

// ---- Does Ve actually collapse? (HARDWARE_BUILD_PLAN.md §8 check 3b) --------
//
// ⚠ THIS IS THE MEASUREMENT THAT DELETED THE HEAL'S VERIFY, kept because the verify is the
// kind of thing that gets reinvented by someone who has not seen these numbers.
//
// Ds18b20Temp::healResolution() used to confirm its EEPROM write by cycling the rail and
// reading the scratchpad back: the part reloads RAM from EEPROM on power-up, so a value
// surviving the cycle meant the write took. That rested entirely on Ve falling below the
// part's power-on-reset threshold within verifyDischargeMs, which was 50, which was a seed
// nobody had measured. It is between 50 and 100 ms — so the verify never reset the part, read
// back the RAM copy it had just written, and reported success unconditionally.
//
// This measures it without the scope §8 asks for. Write Scratchpad touches RAM ONLY — no
// Copy Scratchpad, so no EEPROM write and no wear — which makes a throwaway TH value a
// perfect marker. Power-cycle, read TH back:
//
//   TH still the marker  -> the part never lost power. Ve did not collapse.
//   TH back to its old value -> the part reset and reloaded from EEPROM. Ve did collapse.
void verifyDischargeSweep() {
    Serial.println("\ncontrol 3: does Ve collapse? — the measurement that removed the verify");

    g_rail.on();
    delay(300);

    uint8_t pad[kDsScratchpadLen];
    if (!readPad(pad)) {
        Serial.println("  cannot read the scratchpad — skipping the sweep");
        g_rail.off();
        return;
    }
    const uint8_t liveConfig = pad[4];
    const uint8_t baseTh     = pad[2];
    const uint8_t baseTl     = pad[3];
    Serial.printf("  live config = 0x%02X (%s), TH = 0x%02X\n", (unsigned)liveConfig,
                  liveConfig == kDsConfig9Bit  ? "9-bit — HEALED" :
                  liveConfig == kDsConfig12Bit ? "12-bit — factory default, NOT healed" :
                                                 "other",
                  (unsigned)baseTh);

    constexpr uint8_t  kMarker = 0x11;      // not a plausible TH, so it cannot be mistaken
    const uint32_t discharges[] = {10, 50, 100, 250, 500};

    Serial.println("  off_ms | reset | TH after | verdict");
    for (size_t i = 0; i < sizeof(discharges) / sizeof(discharges[0]); ++i) {
        const uint32_t offMs = discharges[i];

        // Park the marker in RAM. ⚠ The config byte is written back unchanged — Write
        // Scratchpad takes all three bytes, and sending anything else here would silently
        // change the part's resolution as a side effect of a diagnostic.
        g_rail.on();
        delay(300);
        if (!g_bus.reset()) { Serial.printf("  %6lu | no presence before write\n",
                                            (unsigned long)offMs); continue; }
        g_bus.writeByte(kDsSkipRom);
        g_bus.writeByte(kDsWriteScratch);
        g_bus.writeByte(kMarker);
        g_bus.writeByte(baseTl);
        g_bus.writeByte(liveConfig);

        if (!readPad(pad) || pad[2] != kMarker) {
            Serial.printf("  %6lu | marker did not land in RAM — sweep is meaningless\n",
                          (unsigned long)offMs);
            continue;
        }

        g_rail.off();
        delay(offMs);
        g_rail.on();

        // Immediately, with no settle — exactly what healResolution() does.
        const bool present = g_bus.reset();
        delay(20);                       // then give it a moment and read properly
        const bool ok = readPad(pad);

        const char* verdict;
        if (!ok)                       verdict = "unreadable after the cycle";
        else if (pad[2] == kMarker)    verdict = "NO reset — Ve stayed up, a verify here LIES";
        else                           verdict = "reset — Ve collapsed, the verify is sound";

        Serial.printf("  %6lu | %-5s | %s | %s\n", (unsigned long)offMs,
                      present ? "yes" : "NO",
                      ok ? (pad[2] == kMarker ? "marker  " : "restored") : "  ??    ",
                      verdict);
    }

    // Leave the part as we found it.
    g_rail.on();
    delay(300);
    if (g_bus.reset()) {
        g_bus.writeByte(kDsSkipRom);
        g_bus.writeByte(kDsWriteScratch);
        g_bus.writeByte(baseTh);
        g_bus.writeByte(baseTl);
        g_bus.writeByte(liveConfig);
    }
    g_rail.off();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(200);                  // let the USB bridge settle, or the banner is lost
    Serial.println("\nsoundings ds18probe: DS18B20 presence-pulse latency after Ve rises");
    Serial.println("issue #94 — is the headspace fault a settle window, or a dead bus?");

    g_bus.begin();               // idle the line released before the rail ever comes up

    // ---- CONTROL: the part has two seconds to boot. Does it answer at all? ----
    g_rail.on();
    delay(2000);

    uint8_t hits = 0;
    for (uint8_t i = 0; i < 10; ++i) {
        if (g_bus.reset()) ++hits;
        delay(10);
    }
    Serial.printf("\ncontrol: Ve on for 2000 ms, then %u/10 presence pulses\n", (unsigned)hits);

    if (hits == 0) {
        Serial.println("  ^ NOTHING answers with the part fully powered. This is NOT a settle");
        Serial.println("    window — check DQ on GPIO7, the 4.7k from DQ to Ve, and the probe's");
        Serial.println("    GND. A settle window cannot fix a bus with nothing on it.");
    } else {
        Serial.println("  ^ the probe is alive and the bus is wired. Latency test below is the");
        Serial.println("    one that matters.");
        dumpScratchpad("control, rail settled");
    }

    // ---- CONTROL 2: how long does a write-1 ACTUALLY hold the line low? ----
    //
    // ⚠ There is no rail-off presence control here, and there deliberately cannot be. The
    // 4.7k pull-up goes to Ve, the SWITCHED rail (onewire_bus.h:14-16), so with the rail
    // down that resistor is a pull-DOWN to a dead rail: the line sits low, reset() samples
    // it low, and reports presence every single time. A test that cannot fail is not a
    // control, and an earlier version of this file shipped exactly that mistake.
    //
    // The real question. The datasheet allows a write-1 to hold the line low for 1-15 us;
    // longer and the part reads it as a 0. writeBit() budgets 6 us (onewire_bus.h:36) — but
    // that budget counts only the delayMicroseconds() call. pinMode() and digitalWrite() are
    // Arduino wrappers over the GPIO matrix, not register writes, and they cost real time
    // that nothing above this seam has ever accounted for. If drive + 6 us + release exceeds
    // 15 us, then EVERY 1 bit goes out as a 0, every command is garbage, no part ever
    // answers, and every read comes back 0xFF off the pull-up. Which is what we see.
    {
        constexpr uint32_t kIters = 2000;
        uint32_t t0;

        t0 = micros();
        for (uint32_t i = 0; i < kIters; ++i) {
            noInterrupts();
            pinMode(kPinOneWireDq, OUTPUT);
            digitalWrite(kPinOneWireDq, LOW);
            interrupts();
        }
        const uint32_t driveNs = ((micros() - t0) * 1000UL) / kIters;

        t0 = micros();
        for (uint32_t i = 0; i < kIters; ++i) {
            noInterrupts();
            pinMode(kPinOneWireDq, INPUT);
            interrupts();
        }
        const uint32_t releaseNs = ((micros() - t0) * 1000UL) / kIters;

        // The whole low period of a write-1, shaped exactly as writeBit() shapes it.
        t0 = micros();
        for (uint32_t i = 0; i < kIters; ++i) {
            noInterrupts();
            pinMode(kPinOneWireDq, OUTPUT);
            digitalWrite(kPinOneWireDq, LOW);
            delayMicroseconds(6);
            pinMode(kPinOneWireDq, INPUT);
            interrupts();
        }
        const uint32_t slotNs = ((micros() - t0) * 1000UL) / kIters;

        // And the same slot shaped the way onewire_bus.cpp shapes it now: output-enable
        // toggled by register, latch parked low at begin(). This is the number that has to
        // come in under 15 us, and printing it next to the Arduino one is what makes the
        // fix legible rather than asserted.
        pinMode(kPinOneWireDq, INPUT);
        REG_WRITE(GPIO_OUT_W1TC_REG, kDqMask);

        t0 = micros();
        for (uint32_t i = 0; i < kIters; ++i) {
            noInterrupts();
            REG_WRITE(GPIO_ENABLE_W1TS_REG, kDqMask);
            delayMicroseconds(6);
            REG_WRITE(GPIO_ENABLE_W1TC_REG, kDqMask);
            interrupts();
        }
        const uint32_t regSlotNs = ((micros() - t0) * 1000UL) / kIters;

        Serial.println("\ncontrol 2: write-1 low period, both ways");
        Serial.printf("  pinMode(OUTPUT)+digitalWrite(LOW) : %lu.%03lu us\n",
                      (unsigned long)(driveNs / 1000), (unsigned long)(driveNs % 1000));
        Serial.printf("  pinMode(INPUT)                    : %lu.%03lu us\n",
                      (unsigned long)(releaseNs / 1000), (unsigned long)(releaseNs % 1000));
        Serial.printf("  slot via Arduino calls            : %lu.%03lu us  [datasheet max 15]\n",
                      (unsigned long)(slotNs / 1000), (unsigned long)(slotNs % 1000));
        Serial.printf("  slot via registers  (the fix)     : %lu.%03lu us  [datasheet max 15]\n",
                      (unsigned long)(regSlotNs / 1000), (unsigned long)(regSlotNs % 1000));

        if (regSlotNs > 15000UL) {
            Serial.println("  ^ STILL OVER 15 us. The fix has not landed — every write-1 is");
            Serial.println("    delivered as a 0 and no command can be understood.");
        } else {
            Serial.println("  ^ inside the window. Write timing is no longer the fault; if the");
            Serial.println("    scratchpad below is still all 0xFF, suspect the probe itself.");
        }
    }

    verifyDischargeSweep();

    g_rail.off();
    delay(kOffMs);
    Serial.println("\ncycle | presence latency after Ve rises | reset attempts");
}

void loop() {
    ++g_cycle;

    // Down first, and long enough to matter. Measuring "latency after rail_.on()" on a rail
    // that never fell would measure nothing at all.
    g_rail.off();
    delay(kOffMs);

    const uint32_t t0      = micros();
    const uint32_t startMs = millis();
    g_rail.on();

    uint32_t attempts = 0;
    uint32_t firstUs  = 0;
    bool     answered = false;

    for (;;) {
        ++attempts;
        if (g_bus.reset()) {
            firstUs  = micros() - t0;
            answered = true;
            break;
        }
        // Each reset() is ~1 ms of its own timing (onewire_bus.h: 500 + 70 + 430 us), so the
        // attempt count is roughly the elapsed millisecond count. Both are printed anyway —
        // if they ever disagree, the timing constants are not what the header says.
        if (millis() - startMs >= kDeadlineMs) break;
    }

    if (answered) {
        Serial.printf("%5lu | %8lu us (%lu.%03lu ms) | %lu attempts\n",
                      (unsigned long)g_cycle, (unsigned long)firstUs,
                      (unsigned long)(firstUs / 1000), (unsigned long)(firstUs % 1000),
                      (unsigned long)attempts);
        if (!g_dumpedPad) {
            dumpScratchpad("first cycle after power-up");
            g_dumpedPad = true;
        }

        // ---- The full sequence Ds18b20Temp::read() runs, stage by stage ----
        // Walks the same steps the driver does and says which one loses it, rather than
        // leaving the fault bit to mean "something, somewhere".
        //
        // ⚠ The reset is not optional and is not the one above. Every 1-Wire transaction
        // starts with its own reset, and dumpScratchpad() may have just left the bus part-way
        // through a read. Without this, cycle 1 issued Convert T into a bus that was not
        // listening and reported "Convert T is not reaching the part" on a part that was
        // perfectly fine — a diagnostic lying in the one place a diagnostic must not.
        if (!g_bus.reset()) {
            Serial.println("    convert: no presence before Convert T");
            g_rail.off();
            delay(kGapMs);
            return;
        }
        g_bus.writeByte(kDsSkipRom);
        g_bus.writeByte(kDsConvertT);

        const uint32_t convStart = millis();
        uint32_t polls = 0;
        bool     done  = false;
        while (millis() - convStart < 900) {      // cfg_.conversionDeadlineMs
            ++polls;
            if (g_bus.readBit()) { done = true; break; }
        }
        const uint32_t convMs = millis() - convStart;

        Serial.printf("    convert: poll says %s after %lu ms (%lu read slots)\n",
                      done ? "done" : "TIMED OUT", (unsigned long)convMs,
                      (unsigned long)polls);

        // ⚠ THE DECISIVE CONTROL. The poll claims the conversion finished instantly, which
        // cannot be true for a 12-bit part. Either the Convert T command never landed, or it
        // landed and the poll is misreading the status. Waiting out the full 750 ms blind
        // separates them: if a real temperature appears here, the command lands and the POLL
        // is the bug; if it is still 0x0550, the command never arrived.
        delay(800);

        if (!g_bus.reset()) {
            Serial.println("    post-convert reset: NO PRESENCE");
        } else {
            g_bus.writeByte(kDsSkipRom);
            g_bus.writeByte(kDsReadScratch);
            uint8_t pad[kDsScratchpadLen];
            for (size_t i = 0; i < kDsScratchpadLen; ++i) pad[i] = g_bus.readByte();

            Serial.print("    post-convert scratchpad:");
            for (size_t i = 0; i < kDsScratchpadLen; ++i) Serial.printf(" %02X", (unsigned)pad[i]);

            const bool crcOk = (ds18b20Crc8(pad, 8) == pad[8]);
            const uint16_t undef = (uint16_t)dsUndefinedLowBits(pad[4]);
            const int16_t  raw   = (int16_t)((((uint16_t)pad[1] << 8) | pad[0]) & (uint16_t)~undef);

            Serial.printf("  | crc=%s raw=%d (%d.%02d C)\n", crcOk ? "OK" : "BAD",
                          (int)raw, (int)(raw / 16), (int)(abs(raw % 16) * 100 / 16));
            if (raw == (int16_t)0x0550) {
                Serial.println("    ^ STILL 0x0550 after waiting out a full conversion. The");
                Serial.println("      Convert T command is not reaching the part at all.");
            } else {
                Serial.println("    ^ REAL TEMPERATURE after a blind 800 ms wait. Convert T does");
                Serial.println("      land — the conversion-complete POLL is what is broken.");
            }
        }
    } else {
        Serial.printf("%5lu | NO PRESENCE within %lu ms | %lu attempts\n",
                      (unsigned long)g_cycle, (unsigned long)kDeadlineMs,
                      (unsigned long)attempts);
    }

    g_rail.off();
    delay(kGapMs);
}
