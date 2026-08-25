#include "sx1262_radio.h"

namespace soundings {

bool Sx1262Radio::begin() {
    // The TCXO voltage is passed to begin() rather than set afterwards on purpose: on
    // these modules the crystal is powered from DIO3, so the radio cannot come up at all
    // without it, and a later call is a later call than the failure.
    const int16_t st = radio_.begin(kLoRaFreqMHz, kLoRaBwKHz, kLoRaSf, kLoRaCr,
                                    kLoRaSyncWord, txPower_, kLoRaPreamble,
                                    kTcxoVoltage);
    status_ = st;
    up_ = (st == RADIOLIB_ERR_NONE);
    return up_;
}

IRadio::TxResult Sx1262Radio::send(const uint8_t* buf, size_t len) {
    // A radio that never came up is Failed, not Busy. Busy invites a retry, and retrying
    // a chip that did not initialise spends the wake budget discovering the same thing.
    if (!up_) return TxResult::Failed;

    const int16_t st = radio_.transmit(buf, len);   // takes const; no cast needed
#ifdef SOUNDINGS_BENCH
    // Bench-only, and it earns its place: without it a transmit the radio REFUSED looks
    // identical, on the monitor, to one that went out and was missed by the gateway. The
    // 3.9c bench run saw 2 of 9 packets arrive and could not attribute the other 7 to
    // either end. RadioLib's status code is the attribution — 0 is sent, -706 is a TX
    // timeout, and anything else names its own fault.
    Serial.printf("radio.send %u bytes -> %s (status %d)\n",
                  (unsigned)len, st == RADIOLIB_ERR_NONE ? "OK" : "FAILED", (int)st);
#endif
    // ⚠ RE-ARM. transmit() dropped the chip to standby and left it there
    // (SX126x.cpp:214, :271), so a gateway in continuous mode is deaf right now. This is
    // the invariant iradio.h places on the implementation rather than the caller — the
    // only caller is src/gateway/main.cpp, which no test env compiles.
    //
    // Unconditionally after the transmit, whether it succeeded or not: a FAILED send drops
    // out of receive just the same, and a gateway that stops listening because a downlink
    // failed is a worse outcome than the failed downlink.
    if (continuous_) radio_.startReceive();

    if (st == RADIOLIB_ERR_NONE) return TxResult::Ok;

    // Everything else is Failed — including timeouts. See the header: this driver never
    // returns Busy, because the caller's retry loop has no backoff and a wedged radio
    // must not be able to hold the cycle open.
    return TxResult::Failed;
}

size_t Sx1262Radio::receive(uint8_t* buf, size_t cap, uint32_t timeoutMs) {
    if (!up_ || timeoutMs == 0) return 0;

    // ⚠ MILLISECONDS. RadioLib's own debug line reads "Timeout in %lu ms"
    // (SX126x.cpp:287) — this was written as microseconds on the assumption that a radio
    // library would want the finer unit, and the node sat in receive for 250 SECONDS
    // instead of 250 ms. It booted, brought the radio up, transmitted, and then never
    // reached deep sleep, which from the outside is indistinguishable from a crash.
    // Caught at the bench; nothing host-testable could have caught it.
    radio_.setRxBoostedGainMode(true);
    const int16_t st = radio_.receive(buf, cap, (RadioLibTime_t)timeoutMs);

#ifdef SOUNDINGS_BENCH
    // ⚠ THE RETURN VALUE BELOW THROWS THIS AWAY, AND THAT IS THE POINT OF PRINTING IT.
    //
    // receive() collapses timeout, CRC failure and over-length into a single 0, which is
    // correct for the caller — all three mean "nothing was heard" — but it makes a packet
    // that ARRIVED AND WAS CORRUPTED indistinguishable from silence. On the gateway that
    // is the difference between "the node is out of range" and "the node is too close and
    // saturating the receiver", and those have opposite fixes.
    //
    // A timeout is deliberately NOT printed: the gateway calls this every 200 ms and the
    // line would be pure spam at five a second. Everything else is rare and interesting.
    if (st == RADIOLIB_ERR_NONE) {
        Serial.printf("radio.receive OK %u bytes  rssi %.1f dBm  snr %.1f dB\n",
                      (unsigned)radio_.getPacketLength(),
                      (double)radio_.getRSSI(), (double)radio_.getSNR());
    } else if (st != RADIOLIB_ERR_RX_TIMEOUT) {
        // -7 is RADIOLIB_ERR_CRC_MISMATCH: energy arrived and did not survive the trip.
        Serial.printf("radio.receive NOT-A-TIMEOUT status %d  rssi %.1f dBm  snr %.1f dB\n",
                      (int)st, (double)radio_.getRSSI(), (double)radio_.getSNR());
    }
#endif

    // A timeout is silence, and silence is the ordinary outcome — the gateway is under no
    // obligation to reply. It is reported the same as a CRC failure and a length error,
    // because all three have the same consequence for the caller: nothing was heard, the
    // window closes, the node sleeps (contracts/downlink-v1.md).
    if (st != RADIOLIB_ERR_NONE) return 0;

    const size_t n = radio_.getPacketLength();
    // Longer than the caller's buffer is dropped whole rather than truncated: a partial
    // frame fails its CRC anyway, and handing one back invites someone to parse it.
    return (n > cap) ? 0 : n;
}

void Sx1262Radio::startReceive() {
    if (!up_) return;
    radio_.setRxBoostedGainMode(true);
    continuous_ = true;
    radio_.startReceive();   // RX_TIMEOUT_INF — stays in receive indefinitely
}

size_t Sx1262Radio::poll(uint8_t* buf, size_t cap) {
    if (!up_ || !continuous_) return 0;

    // Non-blocking: ask the chip whether a packet landed. Nothing yet is the answer on
    // almost every call and is not a fault.
    if (!(radio_.getIrqFlags() & RADIOLIB_SX126X_IRQ_RX_DONE)) return 0;

    const size_t n = radio_.getPacketLength();   // must precede readData (SX126x.h:353)

    // readData() reports a CRC failure AFTER copying the bytes, deliberately, so the
    // caller may keep them (SX126x.cpp:551). We do not: a corrupt frame is dropped whole,
    // matching receive() and the seam's rule against handing back anything parseable.
    // It must still be READ, though — leaving it in the buffer with its IRQ set would
    // wedge this poll on the same stale packet forever.
    uint8_t  sink[kMaxRadioFrame];
    const bool tooBig = (n > cap);
    const int16_t st = radio_.readData(tooBig ? sink : buf,
                                       tooBig ? (n > sizeof(sink) ? sizeof(sink) : n) : n);

#ifdef SOUNDINGS_BENCH
    if (st == RADIOLIB_ERR_NONE && !tooBig) {
        Serial.printf("radio.poll OK %u bytes  rssi %.1f dBm  snr %.1f dB\n",
                      (unsigned)n, (double)radio_.getRSSI(), (double)radio_.getSNR());
    } else if (st != RADIOLIB_ERR_NONE) {
        // -7 is CRC mismatch: energy arrived and did not survive. Distinguishable from
        // silence, which is the whole reason the 3.9c duty-cycle bug took a bench cycle
        // to find — receive() collapses the two into a single 0.
        Serial.printf("radio.poll status %d  rssi %.1f dBm  snr %.1f dB\n",
                      (int)st, (double)radio_.getRSSI(), (double)radio_.getSNR());
    }
#endif

    // No re-arm needed here: with RX_TIMEOUT_INF the chip stays in receive across a
    // packet, and readData() does not call standby() the way receive() does.
    if (st != RADIOLIB_ERR_NONE || tooBig) return 0;
    return n;
}

void Sx1262Radio::sleep() {
    if (!up_) return;
    // Warm start retained: the radio keeps its configuration across sleep, so waking it
    // does not mean re-running begin(). Nothing here relies on that yet — parked boards
    // are reset rather than woken — but it is the cheaper of the two and 3.10 will want it.
    radio_.sleep(true);
}

} // namespace soundings
