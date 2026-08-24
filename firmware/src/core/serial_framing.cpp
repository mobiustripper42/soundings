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

size_t SerialFrameReader::feed(uint8_t b, uint8_t* out, size_t cap) {
    if (held_ >= sizeof(buf_)) held_ = 0;   // unreachable if the loop below is right
    buf_[held_++] = b;

    // The five steps of contracts/serial-framing-v1.md § Reading the stream, in order.
    for (;;) {
        // 1. Find the sync pair; discard everything before it.
        size_t i = 0;
        bool   found = false;
        while (i + 1 < held_) {
            if (buf_[i] == kSerialSync0 && buf_[i + 1] == kSerialSync1) { found = true; break; }
            ++i;
        }
        if (!found) {
            // Retain a trailing sync0 — it may be the first half of a pair whose second
            // byte has not arrived, and dropping it loses the frame behind it.
            const size_t keep = (held_ > 0 && buf_[held_ - 1] == kSerialSync0) ? 1 : 0;
            discarded_ += (uint32_t)(held_ - keep);
            if (keep) buf_[0] = buf_[held_ - 1];
            held_ = keep;
            return 0;
        }
        if (i > 0) {
            discarded_ += (uint32_t)i;
            memmove(buf_, buf_ + i, held_ - i);
            held_ -= i;
        }

        // 2. Wait for the length byte.
        if (held_ < kSerialFrameOverhead) return 0;

        // 3. Range-check it. On failure discard EXACTLY ONE byte and rescan — dropping
        //    the whole sync pair would skip the real header in `A5 A5 5A ...`.
        const uint8_t len = buf_[2];
        if (len < kMinFramedPayload || len > kMaxPacketLen) {
            discarded_ += 1;
            memmove(buf_, buf_ + 1, held_ - 1);
            held_ -= 1;
            continue;
        }

        // 4. Wait for the payload.
        if (held_ < kSerialFrameOverhead + (size_t)len) return 0;

        // 5. Emit and consume.
        const size_t consumed = kSerialFrameOverhead + (size_t)len;
        const bool   fits     = ((size_t)len <= cap);
        if (fits) memcpy(out, buf_ + kSerialFrameOverhead, len);
        memmove(buf_, buf_ + consumed, held_ - consumed);
        held_ -= consumed;
        return fits ? (size_t)len : 0;
    }
}

} // namespace soundings
