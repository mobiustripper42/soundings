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
    virtual TxResult send(const uint8_t* buf, size_t len) = 0;
    virtual ~IRadio() = default;
};

} // namespace soundings
