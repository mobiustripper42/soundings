#include "tank_preset.h"
#include "runcycle.h"

namespace soundings {

NodeManifest tankPreset(uint8_t node_id) {
    NodeManifest m;
    m.node_id    = node_id;
    // Cadence comes from the run-cycle defaults rather than being restated here: SPEC's
    // 15-min/±30 s settlement is one number, and a preset that copied it would be a second
    // place for it to be wrong.
    m.intervalMs = kDefaultIntervalMs;
    m.jitterMs   = kDefaultJitterMs;
    // One sensor, one UART, no analog front end — the reason DEC-005 put this node first.
    m.channels[0] = ChannelDecl{8, kSensorTypeDistance};   // 8 = TANK_DISTANCE
    m.count       = 1;
    return m;
}

} // namespace soundings
