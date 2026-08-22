#include "a02yyuw.h"

namespace soundings {
namespace {

constexpr uint8_t kHeader = 0xFF;

// Median of up to seven values. Sorts in place — the caller's array is a local scratch
// buffer and arrival order carries no meaning once every frame has passed the range check.
//
// An EVEN count returns the upper of the two middle values rather than averaging them.
// Averaging would synthesise a distance the sensor never reported, which is precisely the
// property that made a median the right choice over a mean in the first place. The
// default configuration collects an odd count, so this is a guard on a configuration
// nobody has asked for yet, not a routine path.
uint16_t medianOf(uint16_t* v, uint8_t n) {
    for (uint8_t i = 1; i < n; ++i) {
        const uint16_t key = v[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; --j; }
        v[j + 1] = key;
    }
    return v[n / 2];
}

} // namespace

bool A02yyuwFrameParser::feed(uint8_t b, uint16_t& mm) {
    // Hunting for a header. Everything that is not 0xFF is discarded one byte at a time,
    // which is what makes the parser tolerate being switched on mid-frame.
    if (held_ == 0) {
        if (b != kHeader) return false;
        buf_[0] = b;
        held_   = 1;
        return false;
    }

    if (held_ < 3) {
        buf_[held_++] = b;
        return false;
    }

    // b is the SUM byte. Published formula, not inferred: SUM = (Header + Data_H +
    // Data_L) & 0x00FF (a02yyuw.h).
    const uint8_t expected = (uint8_t)((buf_[0] + buf_[1] + buf_[2]) & 0xFF);
    held_ = 0;

    if (b != expected) {
        ++badChecksums_;
        // The SUM byte is consumed, NOT reconsidered as a header for the next frame. It
        // could be one — 0xFF is a legal checksum — so this costs at most one extra frame
        // on the rare occasion that a corrupt frame is followed immediately by a real
        // one. The alternative, re-feeding it, buys that frame back at the price of a
        // parser that can be walked out of sync by any stream containing 0xFF where a
        // checksum should be. A frame costs 100 ms; the read budget holds twelve.
        //
        // In practice a false lock is already unlikely: the sensor tops out at 4500 mm,
        // so DATA_H never exceeds 0x11 and a mid-frame 0xFF can only be a checksum.
        return false;
    }

    // Distance = Data_H * 256 + Data_L, in millimetres.
    mm = (uint16_t)(((uint16_t)buf_[1] << 8) | (uint16_t)buf_[2]);
    return true;
}

IDistance::Reading A02yyuwDistance::read() {
    // Clamped rather than asserted. Phase 3.5 fills configuration from a declared
    // manifest, and the same reasoning as RunCycle::nextSleepMs applies: a node reading a
    // few too many frames is wrong and recoverable, a node that aborts on boot is a
    // reflash in a tunnel — and here it would also be a fixed-array overrun on a part
    // with no allocator and no way to report the corruption.
    uint8_t want = cfg_.sampleFrames;
    if (want == 0) want = 1;
    if (want > kMaxSampleFrames) want = kMaxSampleFrames;

    rail_.on();
    // A partial frame left over from a previous cycle is not a frame — the rail has been
    // off since, and the sensor has restarted mid-sentence.
    parser_.reset();

    const uint32_t start = clock_.millis();
    uint16_t samples[kMaxSampleFrames] = {};
    uint8_t  collected = 0;
    uint8_t  discarded = 0;

    for (;;) {
        // ONE clock reading per pass, holding a single instant that both the deadline and
        // the settle window are judged against. Two Elapsed timers would read the clock
        // twice and answer two slightly different questions about "now" — harmless on
        // hardware, and on the bench it makes the settle boundary depend on how many
        // times the loop happens to look at the clock. Unsigned subtraction is wrap-safe
        // across the ~49.7-day millis() rollover, same as Elapsed.
        const uint32_t since = clock_.millis() - start;

        if (since >= cfg_.deadlineMs) {
            // The rail comes down on the failure path too. This is the branch where a
            // stuck rail hides: the packet already carries a fault bit for the channel
            // (DEC-002), so nothing downstream would look twice at a node that is quietly
            // holding a free-running sensor powered until the pack is flat.
            rail_.off();
            return Reading{0, false};
        }

        uint8_t b;
        if (!bytes_.readByte(b)) continue;   // ordinary: the gap between frames

        // Inside the settling window the bytes are read and thrown away without being
        // parsed. There is no published power-on settling spec for this part, so nothing
        // it says this early is trusted — including a frame that would pass its checksum.
        if (since < cfg_.settleMs) continue;

        uint16_t mm;
        if (!parser_.feed(b, mm)) continue;               // partial, or malformed and dropped
        if (mm < cfg_.minMm || mm > cfg_.maxMm) continue; // not a distance this sensor can report

        // Discards are counted in VALID frames, not in elapsed time or bytes: the point is
        // to skip the sensor's first few real answers while it settles, and a frame that
        // failed its checksum was never one of those.
        if (discarded < cfg_.discardFrames) { ++discarded; continue; }

        samples[collected++] = mm;
        if (collected >= want) break;
    }

    rail_.off();
    return Reading{medianOf(samples, collected), true};
}

} // namespace soundings
