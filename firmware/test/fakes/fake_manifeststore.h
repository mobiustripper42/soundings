#pragma once
#include "imanifeststore.h"

namespace soundings {

// FakeManifestStore — RAM stand-in for NVS. Owns the manifest it hands out, which is what
// the real store will do too once it caches a read from flash.
class FakeManifestStore : public IManifestStore {
public:
    FakeManifestStore() = default;
    explicit FakeManifestStore(const NodeManifest& m) : m_(m) {}
    void set(const NodeManifest& m) { m_ = m; }
    const NodeManifest& load() const override { return m_; }
private:
    NodeManifest m_;
};

} // namespace soundings
