#pragma once
#include "isampler.h"
#include "itemp.h"

namespace soundings {

// TempSampler — adapts ITemp onto the generic ISampler the run cycle walks. Its own header
// rather than living in isampler.h, for the same reason distance_sampler.h is: the generic
// seam must not include a specific sensor's, or every node type drags in every sensor.
//
// ⚠ THIS IS WHERE THE SIGN CROSSES THE CONTRACT, and it is the only place it does.
//
// ITemp::Reading::raw is int16_t because the sensor's value genuinely goes below zero.
// ISampler::Sample::raw is uint16_t because the packet stores every channel as a raw 16-bit
// word and lets the registry say how to read it (isampler.h:15, packet.h:37-40). Channel 4
// is typed I16 in contracts/packet-v1.md and in the gateway's packet.py, so the bytes on the
// wire are correct as long as nothing along the way tries to interpret them.
//
// The cast is a reinterpretation, not a conversion: two's-complement little-endian is
// byte-identical either way, which is exactly why this is safe AND why it would be silent if
// it were wrong. A driver that returned uint16_t, or an adapter that clamped negatives to
// zero, would produce a packet that parses cleanly and reads +4095 C in January. That is the
// round trip test_temp_roundtrip pins end to end.
class TempSampler : public ISampler {
public:
    explicit TempSampler(ITemp& sensor) : sensor_(sensor) {}
    Sample sample() override {
        ITemp::Reading r = sensor_.read();
        return Sample{(uint16_t)r.raw, r.ok};
    }
private:
    ITemp& sensor_;
};

} // namespace soundings
