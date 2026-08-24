#include <Arduino.h>
#include <esp_sleep.h>
#include "sx1262_radio.h"

// Park — firmware for a board that should be quiet.
//
// It brings the radio up only far enough to put it back to sleep, then deep-sleeps the
// MCU **with no wake source at all**. Nothing transmits, nothing listens, nothing prints.
// The board stays that way until something resets it, which is exactly what flashing it
// does — so recovery is the same one-line upload that put anything else on it.
//
//   pio run -e park -t upload --upload-port /dev/ttyUSB1
//
// Why this exists as its own firmware rather than a flag on the node build: these two
// boards live permanently cabled to bee-grace (DEC-009), so "unplug it" is not available
// and "leave it transmitting because we might want it later" is a bad trade when
// reflashing takes ten seconds from the machine they are attached to. It is also the
// honest way to leave hardware between sessions — a board doing something pointless for
// a week is a board whose state nobody can describe when they come back to it.
//
// ⚠ It does NOT reduce current draw to the deep-sleep figure the power budget assumes.
// That number belongs to the full disable list in HARDWARE_BUILD_PLAN.md §6 (issue #49),
// which this does not implement. What it guarantees is silence, not microamps — and
// these boards are USB-powered, where silence is the thing that matters.

using namespace soundings;

void setup() {
    Serial.begin(115200);
    delay(200);   // let the USB bridge settle, or the one line below is lost

    // The radio does not sleep just because the MCU did. Bringing it up to shut it down
    // is the only way to leave it in a state anyone can name (HARDWARE_BUILD_PLAN.md:508).
    Sx1262Radio radio;
    const bool up = radio.begin();
    if (up) radio.sleep();

    Serial.printf("soundings: parked (radio %s). Reflash to wake.\n",
                  up ? "asleep" : "did not come up");
    Serial.flush();

    // No timer, no GPIO, no wake source. esp_deep_sleep_start() does not return, and with
    // nothing armed there is nothing to return TO — the board is done until it is reset.
    esp_deep_sleep_start();
}

void loop() {}   // never reached
