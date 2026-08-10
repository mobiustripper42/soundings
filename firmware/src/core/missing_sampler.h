#pragma once
#include "isampler.h"

namespace soundings {

// MissingSampler — the DEC-002 enforcement point, and the whole reason the manifest is
// worth having.
//
// A node declares a sensor its firmware has no driver for (a type id nothing serves, or
// a typo in provisioning). The tempting behaviour is to skip the channel. That is exactly
// wrong: a skipped channel is indistinguishable downstream from a sensor that was never
// fitted, so the tank goes quiet and nothing anywhere says why. Binding it to a sampler
// that always fails means the channel is DECLARED and FAULTED in every packet — "expected,
// missing, go and look" instead of silence.
//
// Stateless, so one shared instance serves every unbound channel on the node.
class MissingSampler : public ISampler {
public:
    Sample sample() override { return Sample{0, false}; }
};

} // namespace soundings
