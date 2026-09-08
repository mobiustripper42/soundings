#pragma once
#include <stdint.h>
#include <stddef.h>
#include "packet.h"

namespace soundings {

// NodeManifest — a node's identity as DATA (DEC-002). One firmware binary carries every
// driver; this says which of them this particular node is supposed to be running, plus
// its id and its cadence.
//
// The alternative that keeps suggesting itself — a `-D NODE_TYPE=TANK` build flag — is
// exactly what DEC-002 forbids, because a per-node build flag is a per-node binary and
// the whole point is that the annual battery-swap window stays "plug in, flash, done"
// with no bookkeeping about which build goes where.
//
// This is the narrowed form (Phase 3.5): the tank node's shape, held in memory. The byte
// encoding and NVS storage land with the provisioning task, behind IManifestStore, so
// nothing here has to guess at a format whose constraints are not known yet. Phase 2.6
// (issue #24) extends this into the full node-type preset system.

// Sensor type ids. The manifest names a TYPE, not a driver instance — binding a type to
// the compiled-in driver that serves it is the registry's job (sensor_registry.h), which
// is what lets a node declare a sensor whose driver is absent and have that be a fault
// rather than a compile error.
// A declaration carrying kSensorTypeNone is NOT a distinct error path: it resolves like
// any other type nothing serves, i.e. declared-and-faulted (DEC-002). Said explicitly
// because the name invites the opposite assumption, and the provisioning task will be
// reading this header looking for a BindResult variant that does not exist.
constexpr uint8_t kSensorTypeNone     = 0;
constexpr uint8_t kSensorTypeDistance = 1;   // ultrasonic tank level (A02YYUW)
// ⚠ A manifest TYPE ID, not a channel bit. DEC-007's "no new channel bit spent" is a claim
// about the packet's 16-entry channel registry, which is scarce and shared with every other
// node type; this registry is the manifest's own and is neither. The DS18B20 rides the
// existing channel 4 (SOIL_TEMP_0) and still needs its own type here, because the registry
// binds a declared TYPE to a compiled-in driver (sensor_registry.h) and two sensors on one
// channel bit would otherwise be indistinguishable to it.
constexpr uint8_t kSensorTypeDsTemp   = 2;   // headspace temperature (DS18B20)

struct ChannelDecl {
    uint8_t channelBit;     // index into the packet.h channel registry
    uint8_t sensorTypeId;
};

struct NodeManifest {
    uint8_t     node_id    = 0;
    uint32_t    intervalMs = 0;
    uint32_t    jitterMs   = 0;
    ChannelDecl channels[kMaxChannels] = {};
    uint8_t     count      = 0;    // declared entries in `channels`
};

} // namespace soundings
