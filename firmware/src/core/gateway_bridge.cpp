#include "gateway_bridge.h"
#include "elapsed.h"

namespace soundings {

size_t awaitFramedPayload(IByteSource& src, SerialFrameReader& reader, IClock& clock,
                          uint32_t windowMs, uint8_t* out, size_t cap) {
    // Checked before anything else, and before touching the port. A zero window means
    // "do not relay", and a bridge that drains bytes on its way past would eat the frame
    // the next window was going to complete.
    if (windowMs == 0) return 0;

    Elapsed window(clock);
    window.arm(windowMs);

    for (;;) {
        uint8_t b = 0;
        // False is "nothing right now", not "stream ended" (ibytesource.h) — the
        // ordinary case between the packet going up and the daemon answering.
        if (src.readByte(b)) {
            const size_t n = reader.feed(b, out, cap);
            // Return the INSTANT the frame completes. Waiting out the rest of the window
            // would spend the node's receive budget holding a frame we already have,
            // which is the 200 ms stall this function exists to remove.
            if (n > 0) return n;
        }

        // Checked after the read, not before: a frame already sitting in the buffer when
        // the window opens must be relayed, not discarded for being one tick late.
        if (window.expired()) return 0;
    }
}

} // namespace soundings
