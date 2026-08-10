#pragma once
#include "nodemanifest.h"

namespace soundings {

// The tank node's declaration — the one preset this phase carries (DEC-005 put the tank
// node first). The bed / tunnel-air / rig presets stay parked with Phase 2.6 (issue #24);
// adding them here would be building the preset SYSTEM, which is that task, not this one.
//
// A function rather than a constant so the node id — the one field that genuinely differs
// between two tank nodes — comes from provisioning rather than from a recompile.
NodeManifest tankPreset(uint8_t node_id);

} // namespace soundings
