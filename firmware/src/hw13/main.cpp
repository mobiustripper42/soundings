#include <Arduino.h>
#include "a02yyuw.h"
#include "uart_bytesource.h"
#include "vext_rail.h"

// HW-13 — the bench sitting that answers the last open question in the build sequence:
// does the A02YYUW count checksum-valid frames reliably on a 3.3 V rail?
// (HARDWARE_BUILD_PLAN.md §8 step 5 check 4, §9.)
//
//   pio run -e hw13 -t upload --upload-port /dev/ttyUSB1
//
// ⚠ Run it on BOTH sensors. Two were bought precisely so a marginal unit can be told apart
// from a marginal rail (HARDWARE_BUILD_PLAN.md:277); one sensor answers a different and
// much weaker question.
//
// Why this is its own firmware rather than a flag on the node build: the node's setup()
// ends in deep sleep, so a multi-minute frame-rate count has nowhere to run. This program
// holds the rail on and never sleeps, which is the opposite of the node's whole design —
// so do not leave a board sitting in it. `park` is how a board is left quiet.
//
// ⚠ A bad result here is a BOM change, not a firmware one (F-13). Vext outputs 3.3 V and
// cannot supply more, so the fallback is an AO3401 sourced from VBAT — the part
// HARDWARE_BUILD_PLAN.md:285 closed but deliberately did not delete.

using namespace soundings;

namespace {

UartByteSource      g_bytes;
VextRail            g_rail;
A02yyuwFrameParser  g_parser;

constexpr uint32_t kFirstReportMs = 10000;   // an early look, so a dead cable shows in 10 s
constexpr uint32_t kReportMs      = 60000;

// Per-interval, reset at each report — this is the RATE, which is what HW-13 asks about.
uint32_t g_bytesRead = 0;
uint32_t g_valid     = 0;
uint16_t g_minMm     = 0xFFFF;
uint16_t g_maxMm     = 0;
uint16_t g_lastMm    = 0;

// Cumulative, never reset. A rate that holds for one minute and collapses over ten is the
// failure this test exists to find, and an interval-only view cannot show it.
uint32_t g_totalValid = 0;

uint32_t g_lastReport = 0;
uint16_t g_lastBadTotal = 0;
uint32_t g_interval = 0;

void report(uint32_t nowMs) {
    // badChecksums() is cumulative over the parser's lifetime by design (a02yyuw.h:42-46),
    // so the per-interval figure is a subtraction. Not a repurposing of the counter — the
    // header asks for exactly this.
    const uint16_t badTotal = g_parser.badChecksums();
    const uint16_t badThis  = (uint16_t)(badTotal - g_lastBadTotal);
    g_lastBadTotal = badTotal;

    Serial.printf(
        "[%lu s] valid=%lu bad=%u bytes=%lu | mm last=%u min=%u max=%u | total valid=%lu bad=%u\n",
        (unsigned long)(nowMs / 1000), (unsigned long)g_valid, (unsigned)badThis,
        (unsigned long)g_bytesRead, (unsigned)g_lastMm,
        (unsigned)(g_minMm == 0xFFFF ? 0 : g_minMm), (unsigned)g_maxMm,
        (unsigned long)g_totalValid, (unsigned)badTotal);

    // The two silent failures are different faults and want different cables checked.
    // Saying so here beats leaving it to whoever reads the log to remember.
    if (g_bytesRead == 0) {
        Serial.println("  ^ NO BYTES AT ALL — check Ve, GND, and the TX wire into GPIO6.");
    } else if (g_valid == 0) {
        Serial.println("  ^ bytes but no valid frames — check the baud rate and the "
                       "sensor's RX-to-GND mode strap.");
    }

    g_bytesRead = 0;
    g_valid     = 0;
    g_minMm     = 0xFFFF;
    g_maxMm     = 0;
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(200);                  // let the USB bridge settle, or the banner is lost
    Serial.println("\nsoundings HW-13: A02YYUW frame-rate sitting on the 3.3 V Vext rail");
    Serial.println("expect a frame roughly every 100 ms; the rail stays on until reset");

    g_bytes.begin();
    g_rail.on();                 // and stays on — see the file comment
    g_lastReport = millis();
}

void loop() {
    uint8_t b;
    // Drain everything available this pass. The sensor emits four bytes at a time and the
    // UART FIFO is deep enough to hold several frames, so a one-byte-per-loop read would
    // measure the loop rate rather than the sensor's.
    while (g_bytes.readByte(b)) {
        ++g_bytesRead;
        uint16_t mm;
        if (!g_parser.feed(b, mm)) continue;

        ++g_valid;
        ++g_totalValid;
        g_lastMm = mm;
        if (mm < g_minMm) g_minMm = mm;
        if (mm > g_maxMm) g_maxMm = mm;
    }

    // Unsigned subtraction, wrap-safe across the ~49.7-day millis() rollover — the same
    // shape a02yyuw.cpp:94 uses, and this program is meant to be left running.
    const uint32_t now = millis();
    const uint32_t due = (g_interval == 0) ? kFirstReportMs : kReportMs;
    if (now - g_lastReport >= due) {
        report(now);
        g_lastReport = now;
        ++g_interval;
    }
}
