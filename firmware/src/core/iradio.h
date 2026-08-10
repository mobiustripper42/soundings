#pragma once
#include <stdint.h>
#include <stddef.h>

namespace soundings {

// IRadio — the transmit seam. The SX1262 sits behind it on the real node; a fake queues
// frames in host tests so the run cycle is exercised with no radio and no airtime.
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
    virtual ~IRadio() = default;
};

} // namespace soundings
