#pragma once
#include <stdint.h>

namespace soundings {

// IByteSource — the UART receive seam. The A02YYUW free-runs frames at 9600 8N1 with no
// trigger (docs/tank-level-sensor.md "Read strategy"), so the driver's whole job is to
// consume a byte stream and find frames in it. That stream is the only thing it needs
// from the platform, and this is the narrowest interface that provides one.
//
// Deliberately NOT a "read N bytes" call. The parser resynchronises byte by byte, and a
// buffered read would make the caller responsible for holding the remainder between
// calls — which is the parser's state, not the caller's.
struct IByteSource {
    // Non-blocking, and that is the whole contract: false means nothing is available
    // RIGHT NOW, not that the stream has ended. The sensor is free-running, so a false
    // here is the ordinary case between frames — the driver keeps polling against a
    // deadline rather than treating it as a failure (CLAUDE.md "non-blocking firmware").
    virtual bool readByte(uint8_t& out) = 0;
    virtual ~IByteSource() = default;
};

} // namespace soundings
