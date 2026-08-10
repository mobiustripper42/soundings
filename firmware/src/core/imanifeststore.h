#pragma once
#include "nodemanifest.h"

namespace soundings {

// IManifestStore — where a node's identity comes from.
//
// The ESP32 binding reads NVS, provisioned once at the bench; the fake holds a struct.
// The seam exists so this task does not have to invent a byte format for a store whose
// constraints are not settled — blob vs typed keys, serial provisioning vs a flashed
// partition, whether the gateway ever reads one back. A format designed against those
// unknowns is a format the provisioning task rewrites.
//
// Returns a reference, not a copy: the store owns the storage, and a node reads its own
// manifest on every boot on a device where a spare 40 bytes of stack is worth not
// spending.
struct IManifestStore {
    virtual const NodeManifest& load() const = 0;
    virtual ~IManifestStore() = default;
};

} // namespace soundings
