#pragma once
#include <stdint.h>
#include <stddef.h>
#include "nodemanifest.h"
#include "isampler.h"
#include "runcycle.h"

namespace soundings {

// The manifest names sensor TYPES; the node knows which compiled-in drivers it actually
// has. This is where the two meet: bindManifest() walks the declaration and produces the
// SensorSlot array RunCycle already consumes.

struct SamplerEntry {
    uint8_t   sensorTypeId;
    ISampler* sampler;
};

enum class BindResult {
    Ok,
    UnknownChannel,     // a channel bit with no width in the packet.h registry
    DuplicateChannel,   // the same channel declared twice — last-wins would hide a typo
    TooManyChannels,    // more declarations than the caller's slot array can hold
};

// Binds `m`'s declarations against `registry`, writing slots into `outSlots`.
//
// A declared channel whose type id is not in the registry is NOT an error and is NOT
// dropped: it binds to a shared MissingSampler so it rides as declared-and-faulted every
// cycle (DEC-002, see missing_sampler.h). A registered driver the manifest does not
// declare is simply not bound — carrying dormant drivers is the point of one firmware.
//
// Validation happens HERE rather than at serialize time so a bad manifest names the
// manifest. serialize() would reject the same packet with LengthMismatch or a refusal to
// size an unknown channel, which is a true statement about the wrong thing.
BindResult bindManifest(const NodeManifest& m,
                        const SamplerEntry* registry, size_t registryCount,
                        SensorSlot* outSlots, size_t outCap, size_t& outCount);

// Cadence and identity out of the manifest; fw_version stays a build constant, because it
// describes the binary rather than the node and a manifest that could claim otherwise
// would be able to lie about which code is running.
RunCycleConfig configFromManifest(const NodeManifest& m, uint16_t fw_version);

} // namespace soundings
