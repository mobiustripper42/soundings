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

void Sx1262Radio::sleep() {
    if (!up_) return;
    // Warm start retained: the radio keeps its configuration across sleep, so waking it
    // does not mean re-running begin(). Nothing here relies on that yet — parked boards
    // are reset rather than woken — but it is the cheaper of the two and 3.10 will want it.
    radio_.sleep(true);
}

} // namespace soundings
