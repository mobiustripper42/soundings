#pragma once
#include <RadioLib.h>
#include "iradio.h"

namespace soundings {

// The SX1262 behind IRadio, on a Heltec Wireless Stick Lite V3. Modem parameters and the
// reasoning for them are DEC-010; the numbers below are that decision, not preferences.
//
// This file is the one place in the project that knows a radio library exists. Nothing in
// src/core includes RadioLib, which is what keeps the core host-testable and what makes
// swapping the library a one-file change (iradio.h).

// DEC-010. SF10 / 250 kHz / CR 4/5 — the highest spreading factor whose worst-case
// 46-byte packet still fits inside 400 ms, leaving ~8 dB in the worst location DEC-009
// measured.
constexpr float   kLoRaFreqMHz   = 906.875f;   // matches the range test's centre
constexpr float   kLoRaBwKHz     = 250.0f;
constexpr uint8_t kLoRaSf        = 10;
constexpr uint8_t kLoRaCr        = 5;          // 4/5
constexpr uint8_t kLoRaPreamble  = 8;
constexpr uint8_t kLoRaSyncWord  = 0x12;       // private; no interoperability wanted

// ⚠ TX power is CONFIGURATION, and the bench value is not a nicety. Both boards sit ~6 ft
// apart on bee-grace during bring-up; at full power that close the receiver is saturated,
// decodes fail, and the symptom is indistinguishable from a framing bug or a modem
// mismatch. A hardware problem wearing a software costume is the expensive kind.
constexpr int8_t kTxPowerFieldDbm = 17;
constexpr int8_t kTxPowerBenchDbm = 2;

// Selected at BUILD time, not by editing a line before flashing. A source edit you have
// to remember to revert is one you eventually forget, and the failure mode of forgetting
// is a deployed node shouting at 2 dBm from the far side of the property — which reads as
// a range problem and gets debugged as one.
//
// `-D SOUNDINGS_BENCH` comes from the node_bench / gateway_bench envs (platformio.ini).
// The field envs cannot pick it up by accident because they are different targets.
#ifdef SOUNDINGS_BENCH
constexpr int8_t kTxPowerDbm = kTxPowerBenchDbm;
#else
constexpr int8_t kTxPowerDbm = kTxPowerFieldDbm;
#endif

// Heltec Wireless Stick Lite V3 pinout — SX1262 nets, confirmed on the WSL V3 Rev1.1
// datasheet (HW-15, HARDWARE_BUILD_PLAN.md §3.5).
constexpr int8_t kPinNss  = 8;
constexpr int8_t kPinDio1 = 14;
constexpr int8_t kPinRst  = 12;
constexpr int8_t kPinBusy = 13;

// ⚠ The single most common bring-up trap on this board. These modules run their TCXO from
// the radio's DIO3, and RadioLib will not turn it on unless told the voltage. Leave it at
// 0 and begin() fails with no error anyone would connect to a crystal — the radio simply
// never starts, and it looks exactly like a dead antenna or a wiring fault.
constexpr float kTcxoVoltage = 1.6f;

// Scratch for draining an over-length frame in poll(). Sized to the SX1262's own maximum
// rather than to packet-v1's, because the number that matters here is what the RADIO can
// hand back — anything larger than our contract still has to be read out of the chip's
// buffer to clear it, or poll() re-reads the same stale packet forever.
constexpr size_t kMaxRadioFrame = 255;

class Sx1262Radio : public IRadio {
public:
    explicit Sx1262Radio(int8_t txPowerDbm = kTxPowerFieldDbm) : txPower_(txPowerDbm) {}

    // Bring the radio up. Returns false if the module did not initialise — the caller
    // should treat that as "no radio this cycle" and still sleep, because a node that
    // aborts on boot is a reflash in a tunnel (the same reasoning as the jitter clamp).
    bool begin();

    // Blocking, with a timeout, returning Ok or Failed and NEVER Busy.
    //
    // That is a deliberate reading of iradio.h:22-28: RunCycle retries Busy back-to-back
    // with no backoff of its own, so an async driver that reports Busy while a
    // transmission is in flight turns the retry loop into a tight spin — and a node stuck
    // awake is the battery killer DEC-006 identifies. A 46-byte packet is 288 ms at
    // SF10/250k, so a bounded blocking send is the honest shape and it keeps the retry
    // loop out of the picture entirely.
    TxResult send(const uint8_t* buf, size_t len) override;

    size_t receive(uint8_t* buf, size_t cap, uint32_t timeoutMs) override;

    // Continuous receive — the gateway's mode (iradio.h). RadioLib's no-arg startReceive()
    // uses RADIOLIB_SX126X_RX_TIMEOUT_INF (SX126x.cpp:474), so there is no deadline in
    // either hardware or software and nothing to truncate an in-flight packet against.
    void   startReceive() override;
    size_t poll(uint8_t* buf, size_t cap) override;

    // Put the radio into its own sleep state. Not part of IRadio — nothing in the core
    // run cycle needs it, and a seam method that only one platform implements is a seam
    // method that will be stubbed everywhere else.
    //
    // Required by HARDWARE_BUILD_PLAN.md:508 (the deep-sleep disable list, issue #49):
    // the SX1262 does not sleep just because the MCU did, and a radio left in standby is
    // milliamps against a budget measured in microamps.
    void sleep();

    // The last RadioLib status code. Exposed because "the radio did not come up" is not
    // an actionable statement at a bench — RADIOLIB_ERR_CHIP_NOT_FOUND (wiring or SPI)
    // and RADIOLIB_ERR_SPI_CMD_TIMEOUT (BUSY never released, usually the TCXO) send you
    // to completely different places.
    int16_t lastStatus() const { return status_; }

private:
    SX1262 radio_ = new Module(kPinNss, kPinDio1, kPinRst, kPinBusy);
    int8_t  txPower_;
    bool    up_ = false;
    int16_t status_ = 0;
    // Set by startReceive() and never cleared. send() consults it to decide whether to
    // re-arm, which is the half-duplex invariant iradio.h states: RadioLib's transmit()
    // forces standby (SX126x.cpp:214) and leaves it there (:271), so a gateway that does
    // not put itself back is deaf from its first downlink onward — silently, and looking
    // exactly like a node that stopped transmitting. The node never sets this and must
    // stay able to sleep (DEC-006).
    bool    continuous_ = false;
};

} // namespace soundings
