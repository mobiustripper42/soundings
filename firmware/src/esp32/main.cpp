#include <Arduino.h>
#include "iclock.h"
#include "elapsed.h"

// Phase 1.1 skeleton entry. Its only job right now is to prove src/core links clean
// under arduino-esp32 on the Heltec V3 — the real wake -> sample -> assemble packet ->
// transmit -> deep sleep run cycle lands in Phase 2. No delay() in the loop: timing runs
// through the IClock seam, the same way the run cycle will.

using namespace soundings;

namespace {

// The ESP32 binding of the millis() seam — real hardware behind the interface.
struct ArduinoClock : IClock {
    uint32_t millis() const override { return ::millis(); }
};

constexpr uint32_t kHeartbeatMs = 1000;   // skeleton liveness blink; real run-cycle intervals land in Phase 2

ArduinoClock g_clock;
Elapsed      g_heartbeat(g_clock);

} // namespace

void setup() {
    Serial.begin(115200);
    g_heartbeat.arm(kHeartbeatMs);
}

void loop() {
    if (g_heartbeat.expired()) {
        Serial.println("soundings: alive");
        g_heartbeat.arm(kHeartbeatMs);
    }
}
