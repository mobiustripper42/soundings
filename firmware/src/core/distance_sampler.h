#pragma once
#include "isampler.h"
#include "idistance.h"

namespace soundings {

// DistanceSampler — adapts the tank node's IDistance onto the generic ISampler the run
// cycle walks. Deliberately its own header rather than living in isampler.h: the generic
// seam must not include a specific sensor's, or every node type drags in every sensor.
class DistanceSampler : public ISampler {
public:
    explicit DistanceSampler(IDistance& sensor) : sensor_(sensor) {}
    Sample sample() override {
        IDistance::Reading r = sensor_.read();
        return Sample{r.raw, r.ok};
    }
private:
    IDistance& sensor_;
};

} // namespace soundings
