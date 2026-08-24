#include <Arduino.h>
#include "sx1262_radio.h"
#include "serial_framing.h"
#include "gateway_bridge.h"
#include "iclock.h"
#include "ibytesource.h"
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
//
// The reader is the one in src/core, NOT a copy. It was hand-rolled here first because it
// was "only fifteen lines", and the fifteen lines got the invalid-length resync wrong in a
// way nothing in this file could ever have caught — this sketch is not compiled by any
// test env. The encoder was put in core one task earlier for exactly this reason; the
// reader had a better claim and got the worse treatment.
SerialFrameReader g_reader;
uint8_t g_inPayload[kMaxPacketLen];

// ⚠ THE GATEWAY DOES NOT USE A RECEIVE TIMEOUT AT ALL. It calls startReceive() once and
// polls (iradio.h). There is no slice, so there is no slice length to tune.
//
// It used to run receive(200 ms) in a loop — the NODE-shaped call doing a gateway-shaped
// job. RadioLib enforces that timeout in software (SX126x.cpp:300) and then forces standby
// (:307), aborting any packet still in flight; the chip alone would have finished it. A
// 16-byte packet is 165 ms at SF10 (DEC-010), so only packets starting in the first ~35 ms
// of a slice survived. Predicted 17.5%, measured 2 of 9 twice, at RSSI -60 dBm and
// SNR +6 dB — a link with twenty dB of margin, losing four packets in five to a receiver
// that was not listening.
//
// Continuous receive is how a LoRa gateway is normally built. It is not an optimisation,
// and the previous arrangement was not a conservative default — it was the wrong API.

// How long to poll serial for the daemon's reply after relaying a packet up to it.
//
// ⚠ THIS IS NOT THE NODE'S RECEIVE WINDOW and must be at least as long as it. The node
// holds ~250 ms (runcycle.h kDefaultRxWindowMs); if this were shorter, the board would
// stop listening for a reply while the node was still listening for one.
//
// It replaces the bug this file used to have: after writing a packet to USB, the board
// re-entered a 200 ms BLOCKING radio receive, so the daemon's answer sat unread for up
// to 200 ms of the node's 250 ms budget. Nothing else transmits during this window — the
// node that just sent is asleep for fifteen minutes afterwards — so there is no cost to
// spending it on the serial port instead of the radio.
constexpr uint32_t kReplyWindowMs = 400;

struct ArduinoClock : IClock {
    uint32_t millis() const override { return ::millis(); }
};

// Serial.read() as an IByteSource. Non-blocking by contract: -1 from a UART with nothing
// in it is "nothing right now", which is exactly what the seam means by false.
struct SerialByteSource : IByteSource {
    bool readByte(uint8_t& out) override {
        const int b = Serial.read();
        if (b < 0) return false;
        out = (uint8_t)b;
        return true;
    }
};

ArduinoClock     g_clock;
SerialByteSource g_serialBytes;

// Put one payload on the air. Relayed verbatim — whether it is a well-formed downlink is
// the node's question, not ours; this board does not read payloads.
void relay(size_t len) {
    const IRadio::TxResult r = g_radio.send(g_inPayload, len);
#ifdef SOUNDINGS_BENCH
    if (r != IRadio::TxResult::Ok) {
        // Otherwise a downlink that never left the board is indistinguishable, from the
        // daemon's side, from a node that simply had a quiet window.
        Serial.printf("downlink relay FAILED (%d bytes)\n", (int)len);
    }
#else
    (void)r;
#endif
}

// Idle drain, between node wakes. Nothing is expected here; this exists so a downlink
// the daemon queues early is not left sitting in the UART buffer for a whole slice.
void pumpSerial() {
    while (Serial.available() > 0) {
        const int b = Serial.read();
        if (b < 0) break;

        const size_t len = g_reader.feed((uint8_t)b, g_inPayload, sizeof(g_inPayload));
        if (len > 0) relay(len);
    }
}

// The reply window: a packet has just gone up to the daemon, so the node that sent it is
// holding its receive window RIGHT NOW. Poll the port instead of going back to the radio.
// Logic and tests live in src/core/gateway_bridge.{h,cpp} — nothing in this file is
// compiled by any test env, which is how 3.9b shipped a broken reader from here.
void awaitAndRelayReply() {
    const size_t len = awaitFramedPayload(g_serialBytes, g_reader, g_clock,
                                          kReplyWindowMs, g_inPayload, sizeof(g_inPayload));
    if (len > 0) relay(len);
#ifdef SOUNDINGS_BENCH
    else Serial.println("no downlink from daemon this window");
#endif
}

} // namespace

void setup() {
    // 115200 to match the node's monitor speed; this is the USB link, not the 9600 the
    // A02YYUW uses on its own UART.
    Serial.begin(115200);
    // ⚠ No retry loop and no abort. If the radio does not come up, the sketch still runs
    // and still pumps serial — a bridge that refuses to boot is indistinguishable, from
    // the daemon's side, from an unplugged cable.
    const bool radioUp = g_radio.begin();
#ifdef SOUNDINGS_BENCH
    // The node has said this since 3.9b and the gateway never did, which made a silent
    // board unattributable: no bytes on the port is what a dead radio, a crashed sketch
    // and an unplugged cable ALL look like. One line tells the three apart, and it cost a
    // bench cycle to notice it was missing.
    delay(200);   // let the USB bridge settle before the first line
    Serial.printf("\nsoundings gateway: radio.begin -> %s (status %d)\n",
                  radioUp ? "up" : "DOWN", (int)g_radio.lastStatus());
#else
    (void)radioUp;
#endif

    // Enter receive and stay there for the life of the program. This is the ONLY call —
    // send() re-arms itself (sx1262_radio.cpp), which is what keeps the invariant out of
    // this file, the one file no test env compiles.
    g_radio.startReceive();
}

void loop() {
    pumpSerial();

    // Non-blocking. Returns 0 on almost every call — that is fifteen minutes of silence
    // between node wakes, not a fault. The radio is listening the entire time, including
    // during this call and every one that returns nothing.
    const size_t n = g_radio.poll(g_rx, sizeof(g_rx));
    if (n == 0) return;

    const size_t framed = frameForSerial(g_rx, n, g_tx, sizeof(g_tx));
    // frameForSerial refuses a length outside [6, 46] rather than truncating. A refusal
    // here means the radio handed us something that cannot be either payload type, so
    // dropping it is correct — and it never reaches the daemon to confuse the parser.
    if (framed == 0) return;

    Serial.write(g_tx, framed);
    // Immediately, with no radio receive in between. The node's window is open now and
    // every millisecond spent elsewhere is one it spends listening to nothing.
    awaitAndRelayReply();
}
