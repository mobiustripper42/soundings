#pragma once
#include "ibytesource.h"

namespace soundings {

// FakeByteSource — host-test stand-in for the sensor's UART. A test loads the exact byte
// stream the A02YYUW would emit (including the malformed frames that are the point of the
// parser) and the driver consumes it.
//
// Capacity is generous because a realistic script is long: one read() cycle consumes
// 3 discarded frames plus 5 collected ones plus whatever garbage the test is proving the
// parser survives, at 4 bytes each.
class FakeByteSource : public IByteSource {
public:
    static const int kCapacity = 256;

    // Queue one byte. Past kCapacity the push is dropped rather than wrapping — a
    // silently truncated script would make the driver look like it timed out when in
    // fact the test never delivered the bytes.
    void push(uint8_t b) {
        if (count_ < kCapacity) buf_[count_++] = b;
    }

    // Queue a well-formed frame with a correct checksum, computed the way the sensor
    // computes it. The single place a test says "and then a good frame arrives".
    void pushFrame(uint16_t mm) {
        const uint8_t hi = (uint8_t)(mm >> 8);
        const uint8_t lo = (uint8_t)(mm & 0xFF);
        push(0xFF);
        push(hi);
        push(lo);
        push((uint8_t)((0xFF + hi + lo) & 0xFF));
    }

    // Queue a frame whose SUM byte is deliberately wrong. Takes the bad sum explicitly so
    // the test states the corruption rather than trusting this helper to invent one.
    void pushFrameWithSum(uint16_t mm, uint8_t sum) {
        push(0xFF);
        push((uint8_t)(mm >> 8));
        push((uint8_t)(mm & 0xFF));
        push(sum);
    }

    bool readByte(uint8_t& out) override {
        // Exhausted reads as "nothing available right now", NOT as an error — that is
        // exactly what a real free-running sensor's UART looks like between frames, and
        // it is what lets a test prove the driver's deadline fires instead of hanging.
        if (idx_ >= count_) return false;
        out = buf_[idx_++];
        return true;
    }

    // How many bytes the driver actually consumed. Lets a test assert the driver stopped
    // reading once it had its five frames rather than draining the whole stream.
    int consumed() const { return idx_; }

private:
    uint8_t buf_[kCapacity] = {};
    int     count_ = 0;
    int     idx_   = 0;
};

} // namespace soundings
