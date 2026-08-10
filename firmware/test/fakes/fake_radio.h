#pragma once
#include <string.h>
#include "iradio.h"

namespace soundings {

// FakeRadio — host-test stand-in for the SX1262. Records every frame handed to it
// byte-exactly, so a test can assert what actually went out rather than what the caller
// believed it sent. The real driver lands with the bench bring-up.
class FakeRadio : public IRadio {
public:
    static const int    kCapacity  = 8;    // frames retained
    static const size_t kMaxFrame  = 64;   // ≥ kMaxPacketLen (46) with headroom

    // Scripted result for every subsequent send(). Set once, applies until changed.
    void setResult(TxResult r) { result_ = r; }

    TxResult send(const uint8_t* buf, size_t len) override {
        // Counted even when the result is Busy/Failed: the attempt happened, and a test
        // asserting "we did not transmit" needs the count to mean attempts, not successes.
        ++count_;
        if (count_ > kCapacity || len > kMaxFrame) {
            // Visible, not silent. A fake that quietly discards past capacity would let a
            // run-cycle test send a hundred frames and assert against the first eight.
            overflowed_ = true;
            return result_;
        }
        memcpy(frames_[count_ - 1], buf, len);
        lens_[count_ - 1] = len;
        return result_;
    }

    int    sentCount() const { return count_; }
    bool   overflowed() const { return overflowed_; }
    // Retained frames only — check overflowed() before trusting an index near capacity.
    const uint8_t* frame(int i) const { return frames_[i]; }
    size_t frameLen(int i) const { return lens_[i]; }

private:
    uint8_t  frames_[kCapacity][kMaxFrame] = {};
    size_t   lens_[kCapacity] = {};
    int      count_ = 0;
    bool     overflowed_ = false;
    TxResult result_ = TxResult::Ok;
};

} // namespace soundings
