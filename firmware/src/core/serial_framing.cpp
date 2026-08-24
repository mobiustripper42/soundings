#include "serial_framing.h"
#include <string.h>

namespace soundings {

size_t frameForSerial(const uint8_t* payload, size_t len, uint8_t* out, size_t cap) {
    // Range-checked before anything is written. The length field is a u8 and the decoder
    // range-checks it too (contracts/serial-framing-v1.md step 3), so a length outside
    // this window would produce a frame that cost airtime to receive and is then thrown
    // away at the far end — refusing here is the only place a caller can learn about it.
    if (len < kMinFramedPayload || len > kMaxPacketLen) return 0;

    const size_t total = len + kSerialFrameOverhead;
    // Refuse rather than truncate. A short write desynchronises the reader for a frame,
    // and the caller has a real packet in hand and fifteen minutes before the next one.
    if (cap < total) return 0;

    out[0] = kSerialSync0;
    out[1] = kSerialSync1;
    out[2] = (uint8_t)len;
    // Copied verbatim — nothing is escaped. A payload may legitimately contain the sync
    // pattern, and the decoder only hunts for sync when it is BETWEEN frames, so there is
    // nothing to escape it from.
    memcpy(out + kSerialFrameOverhead, payload, len);
    return total;
}

} // namespace soundings
