#include "sensor_registry.h"
#include "missing_sampler.h"

namespace soundings {
namespace {

// Stateless, so one instance serves every unbound channel on the node — and every node,
// since there is only ever one running.
MissingSampler g_missing;

ISampler* resolve(uint8_t sensorTypeId, const SamplerEntry* registry, size_t count) {
    if (sensorTypeId == kSensorTypeNone) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        if (registry[i].sensorTypeId == sensorTypeId && registry[i].sampler) {
            return registry[i].sampler;
        }
    }
    return nullptr;
}

bool channelIsSizable(uint8_t bit) {
    // A bit the codec cannot size is a bit the packet cannot carry (packet.h registry;
    // width 0 means reserved/unknown).
    return bit < kMaxChannels && kChannelWidth[bit] != 0;
}

} // namespace

BindResult bindManifest(const NodeManifest& m,
                        const SamplerEntry* registry, size_t registryCount,
                        SensorSlot* outSlots, size_t outCap, size_t& outCount) {
    // Nothing is written until the whole manifest validates. A partially bound array
    // would run the node on a configuration the operator never wrote, which is worse than
    // refusing outright — it looks like it worked.
    outCount = 0;

    // Two separate bounds, and they are not the same bound. `outCap` is the caller's
    // array; kMaxChannels is the manifest's OWN. Today every caller passes kMaxChannels
    // so the two coincide — but the next caller is a byte-format deserializer that does
    // not exist yet, and a parsed `count` is an attacker-or-bug-supplied number. Trusting
    // it against only the caller's array would read past `m.channels` the moment someone
    // passes a roomier one.
    if (m.count > kMaxChannels) return BindResult::TooManyChannels;
    if (m.count > outCap)       return BindResult::TooManyChannels;

    uint16_t seen = 0;
    for (uint8_t i = 0; i < m.count; ++i) {
        const uint8_t bit = m.channels[i].channelBit;
        if (!channelIsSizable(bit)) return BindResult::UnknownChannel;
        const uint16_t mask = (uint16_t)(1u << bit);
        // Last-wins would silently swallow a provisioning typo and ship half the sensors
        // the operator believes are fitted.
        if (seen & mask) return BindResult::DuplicateChannel;
        seen |= mask;
    }

    for (uint8_t i = 0; i < m.count; ++i) {
        ISampler* s = resolve(m.channels[i].sensorTypeId, registry, registryCount);
        // Declared but unservable is NOT an error and is NOT dropped: it binds to the
        // always-failing sampler so it rides as declared-and-faulted every cycle. That is
        // the whole of DEC-002 — "expected, missing, go and look" instead of silence.
        outSlots[i].channelBit = m.channels[i].channelBit;
        outSlots[i].sampler    = s ? s : &g_missing;
    }
    outCount = m.count;
    return BindResult::Ok;
}

RunCycleConfig configFromManifest(const NodeManifest& m, uint16_t fw_version) {
    RunCycleConfig cfg;
    cfg.node_id    = m.node_id;
    cfg.fw_version = fw_version;   // describes the binary, not the node — see the header
    cfg.intervalMs = m.intervalMs;
    cfg.jitterMs   = m.jitterMs;
    return cfg;
}

} // namespace soundings
