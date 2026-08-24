#pragma once
#include <stdint.h>
#include <stddef.h>

namespace soundings {

// IRadio — the radio seam. The SX1262 sits behind it on the real node; a fake queues
// frames in host tests so the run cycle is exercised with no radio and no airtime.
//
// Transmit-only until 3.9b, which added receive(). One seam rather than two, because
// there is one radio: a separate IRadioRx would mean two objects wrapping one SX1262 and
// a caller having to keep them consistent about which one owns the chip.
//
// It takes BYTES, not a Packet. The radio stays ignorant of the wire contract: serialize()
// is the caller's job (packet.h, DEC-003). Welding the codec to the transport here would
// mean the gateway radio (Phase 3.9) inherits the weld, and a contract change would have
// to be made in two layers instead of one.
struct IRadio {
    // Busy and Failed are kept apart because they mean different things to a caller that
    // has a schedule to keep: Busy is worth retrying this cycle, Failed is worth a fault
    // bit. Collapsing them into a bool throws that distinction away at the seam, where
    // it is the only place the radio still knows which one happened.
    enum class TxResult { Ok, Busy, Failed };

    // Send `len` bytes. Does not retain the buffer — the caller may reuse it on return.
    //
    // Implementations must not return Busy instantly in a way that turns a caller's retry
    // loop into a tight spin: RunCycle retries Busy back-to-back, bounded by a count and a
    // wall-clock window but with no backoff of its own, and on a node stuck awake is the
    // failure mode that actually drains the pack (DEC-006). A driver that cannot make that
    // guarantee should return Failed instead. Noted here rather than in the caller because
    // it is the driver (Phase 3.9) that owns the timing.
    virtual TxResult send(const uint8_t* buf, size_t len) = 0;

    // Listen for up to `timeoutMs` and copy at most `cap` bytes of the first frame heard
    // into `buf`. Returns bytes received; 0 means nothing arrived, which is the ORDINARY
    // outcome and not a fault — the gateway is under no obligation to reply, and a quiet
    // window is what a node sees on almost every wake (contracts/downlink-v1.md).
    //
    // Blocking with a timeout, like send(). The node has nothing else to do inside its
    // window, and a poll-and-return API would put the deadline in the caller, where every
    // caller would reimplement it. The timeout is the whole safety property: a node that
    // listens forever is a node stuck awake, which DEC-006 names as the battery killer.
    //
    // A frame longer than `cap` is dropped rather than truncated — a partial frame fails
    // its CRC anyway, and returning one would invite a caller to parse it.
    virtual size_t receive(uint8_t* buf, size_t cap, uint32_t timeoutMs) = 0;

    // ---- Continuous receive (3.9c) ------------------------------------------------
    //
    // The gateway's mode, and NOT the node's. Two modes rather than one because the two
    // ends want opposite things and always will: the gateway is mains-powered (DEC-009)
    // and should never stop listening; the node is on a battery and must, which is what
    // the timeout above exists to guarantee.
    //
    // WHY THIS EXISTS. The gateway ran receive(200ms) in a loop — the node-shaped call
    // doing a gateway-shaped job. RadioLib's blocking receive() enforces its timeout in
    // SOFTWARE (SX126x.cpp:300) and then forces standby (:307), aborting any packet still
    // in flight; the chip itself would have finished it, since STOP_TIMER_ON_PREAMBLE is
    // defined but never issued in 7.7.1, leaving the hardware timer stopping on header
    // detection. A 16-byte packet is 165 ms at SF10 (DEC-010), so against a 200 ms slice
    // only packets starting in the first ~35 ms survived: predicted 17.5%, measured 2 of 9
    // twice, at RSSI -60 dBm and SNR +6 dB. The link was never the problem.
    //
    // startReceive() has no timeout, so there is no software deadline to truncate against.
    // This is the ordinary way to build a LoRa gateway, not an optimisation.

    // Enter receive and STAY there until something else changes mode. Idempotent.
    virtual void startReceive() = 0;

    // Non-blocking. Returns bytes copied into `buf`, or 0 if nothing has arrived yet —
    // which is what almost every call returns and is not a fault. Same over-length rule
    // as receive(): dropped whole rather than truncated.
    virtual size_t poll(uint8_t* buf, size_t cap) = 0;

    // ⚠ THE INVARIANT, AND IT BELONGS TO THE IMPLEMENTATION, NOT THE CALLER.
    //
    // The radio is half-duplex: RadioLib's transmit() forces standby on entry
    // (SX126x.cpp:214) and leaves the chip in standby on exit (:271). So every send()
    // silently disarms a continuous receive.
    //
    // An implementation that has been put in continuous mode MUST re-arm itself after
    // send() returns. It is stated here, at the seam, because the alternative is a caller
    // remembering — and the one caller is src/gateway/main.cpp, which no test env
    // compiles, and the failure is a gateway that goes permanently deaf after its first
    // downlink while looking exactly like a node that stopped transmitting.
    //
    // Re-arming is routine in any half-duplex radio program; owning it here is what keeps
    // it from being forgotten in the one file nothing can check.

    virtual ~IRadio() = default;
};

} // namespace soundings
