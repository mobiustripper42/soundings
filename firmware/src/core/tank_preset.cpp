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
    // Two sensors, one UART and one 1-Wire bus, still no analog front end — the reason
    // DEC-005 put this node first survives the second sensor.
    //
    // The headspace temperature is not optional garnish. DEC-007 makes it required: the
    // speed of sound moves ~0.176 %/C, which over a 2 m headspace is 14.1 cm of apparent
    // level change with no water moving, against a sensor specified to +/-1 cm. A tank node
    // that declared only the distance channel would report a level that swings with the
    // weather.
    m.channels[0] = ChannelDecl{8, kSensorTypeDistance};   // 8 = TANK_DISTANCE  (u16, mm)
    m.channels[1] = ChannelDecl{4, kSensorTypeDsTemp};     // 4 = SOIL_TEMP_0    (i16, 1/16 C)
    m.count       = 2;
    return m;
}

} // namespace soundings
