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

    virtual ~IRadio() = default;
};

} // namespace soundings
