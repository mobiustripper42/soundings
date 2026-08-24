#include <Arduino.h>
#include "sx1262_radio.h"
#include "serial_framing.h"
#include "packet.h"
#include "downlink.h"

// The gateway radio — the second Heltec, tethered to bee-grace over USB (DEC-009).
//
// It is a bridge and nothing else. LoRa in, framed bytes out to USB; framed bytes in from
// USB, LoRa out. **It does not parse a packet**, and must never be taught to: the decoder
// lives in exactly one place and that place is the Python parser (DEC-003). A second
// decoder on an ESP32 is a second thing to keep in step with the contract, in the least
// convenient place to change it.
//
// Build: pio run -e gateway. Flash the board that is NOT the node.

using namespace soundings;

namespace {

// Power comes from the build target, not from editing this line: gateway_bench compiles
// at 2 dBm, gateway at 17 (sx1262_radio.h). Both ends need turning down for a desk test,
// not just the node — a saturated receiver is symmetrical.
Sx1262Radio g_radio(kTxPowerDbm);

// One frame each way. kMaxPacketLen (46) is the larger of the two payload types.
uint8_t g_rx[kMaxPacketLen];
uint8_t g_tx[kMaxPacketLen + kSerialFrameOverhead];

// Inbound from the daemon: a downlink, wrapped in the same serial envelope the outbound
// direction uses (contracts/serial-framing-v1.md, amended in 3.9b to run both ways).
// Hand-rolled rather than shared with the Python reader because it is fifteen lines and
// the alternative is a second implementation of the reader in C++ that nothing else uses.
uint8_t g_inBuf[kMaxPacketLen + kSerialFrameOverhead];
size_t  g_inLen = 0;

// Listen this long between checks of the serial port. Short enough that a downlink queued
// by the daemon goes out on the next node window rather than the one after.
constexpr uint32_t kRxSliceMs = 200;

void pumpSerial() {
    while (Serial.available() > 0) {
        const int b = Serial.read();
        if (b < 0) break;

        if (g_inLen == 0 && (uint8_t)b != kSerialSync0) continue;   // hunting
        if (g_inLen == 1 && (uint8_t)b != kSerialSync1) {
            // Not a sync pair. Keep the byte only if it could START one — the same
            // discard-exactly-one rule the Python reader follows, for the same reason.
            g_inLen = ((uint8_t)b == kSerialSync0) ? 1 : 0;
            if (g_inLen == 1) g_inBuf[0] = (uint8_t)b;
            continue;
        }

        g_inBuf[g_inLen++] = (uint8_t)b;

        if (g_inLen == kSerialFrameOverhead) {
            const uint8_t len = g_inBuf[2];
            if (len < kMinFramedPayload || len > kMaxPacketLen) {
                g_inLen = 0;   // impossible length: not a header after all
            }
            continue;
        }

        if (g_inLen > kSerialFrameOverhead &&
            g_inLen == (size_t)g_inBuf[2] + kSerialFrameOverhead) {
            // Relayed verbatim. Whether it is a well-formed downlink is the node's
            // question, not ours — this board does not read payloads.
            g_radio.send(g_inBuf + kSerialFrameOverhead, g_inBuf[2]);
            g_inLen = 0;
        }
    }
}

} // namespace

void setup() {
    // 115200 to match the node's monitor speed; this is the USB link, not the 9600 the
    // A02YYUW uses on its own UART.
    Serial.begin(115200);
    // ⚠ No retry loop and no abort. If the radio does not come up, the sketch still runs
    // and still pumps serial — a bridge that refuses to boot is indistinguishable, from
    // the daemon's side, from an unplugged cable.
    g_radio.begin();
}

void loop() {
    pumpSerial();

    const size_t n = g_radio.receive(g_rx, sizeof(g_rx), kRxSliceMs);
    if (n == 0) return;    // silence between node wakes: the ordinary case, 15 min of it

    const size_t framed = frameForSerial(g_rx, n, g_tx, sizeof(g_tx));
    // frameForSerial refuses a length outside [6, 46] rather than truncating. A refusal
    // here means the radio handed us something that cannot be either payload type, so
    // dropping it is correct — and it never reaches the daemon to confuse the parser.
    if (framed > 0) Serial.write(g_tx, framed);
}
