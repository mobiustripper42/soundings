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

    // Queue one frame for a future receive(), in order. A test that queues nothing is
    // scripting the ordinary case — a quiet window.
    void pushReceive(const uint8_t* buf, size_t len) {
        if (rxCount_ >= kCapacity || len > kMaxFrame) { rxOverflowed_ = true; return; }
        memcpy(rxFrames_[rxCount_], buf, len);
        rxLens_[rxCount_] = len;
        ++rxCount_;
    }

    size_t receive(uint8_t* buf, size_t cap, uint32_t timeoutMs) override {
        ++rxCalls_;
        lastTimeoutMs_ = timeoutMs;
        // Nothing queued reads as silence, NOT as an error — the gateway is under no
        // obligation to reply and this is what almost every real window looks like.
        if (rxIdx_ >= rxCount_) return 0;
        const size_t len = rxLens_[rxIdx_];
        // Too big to hand over is dropped, matching the seam's contract; the frame is
        // still consumed, because on a real radio it arrived and is gone.
        if (len > cap) { ++rxIdx_; return 0; }
        memcpy(buf, rxFrames_[rxIdx_], len);
        ++rxIdx_;
        return len;
    }

    int      receiveCalls() const   { return rxCalls_; }
    uint32_t lastTimeoutMs() const  { return lastTimeoutMs_; }

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

    uint8_t  rxFrames_[kCapacity][kMaxFrame] = {};
    size_t   rxLens_[kCapacity] = {};
    int      rxCount_ = 0;
    int      rxIdx_   = 0;
    int      rxCalls_ = 0;
    bool     rxOverflowed_ = false;
    uint32_t lastTimeoutMs_ = 0;
};

} // namespace soundings
