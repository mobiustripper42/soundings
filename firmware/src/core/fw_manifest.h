#pragma once
#include <stdint.h>
#include <stddef.h>

namespace soundings {

// Firmware manifest v1 — the parse half of contracts/firmware-manifest-v1.md.
//
// The node fetches this over HTTP before it fetches an image. It is the first thing in
// this project that parses input a REMOTE party chose the length and shape of — every
// other parser reads a fixed-width frame off a radio or a serial port. That is why the
// bounds below are constants with tests behind them rather than assumptions.
//
// In src/core so `pio test -e native` compiles it, and so the same parser can be reasoned
// about without an ESP32 in the room. Nothing here does I/O: the caller fetches the bytes
// and hands them over, which is what keeps HTTP out of the testable tier.

// The manifest is a handful of short lines. These caps exist so that a server which is
// broken, hostile, or simply serving the wrong file cannot make the node read until it
// runs out of RAM — it has 320 KB total and no way to report that it died.
constexpr size_t kMaxManifestBytes = 512;
constexpr size_t kMaxManifestLines = 32;

// Long enough for `soundings-node-<version>.bin` with room to spare, short enough that a
// manifest cannot push a large string into a node's stack.
constexpr size_t kMaxManifestFileLen = 63;

struct FwManifest {
    uint16_t version = 0;                        // matches fw_version on the wire
    uint32_t size    = 0;                        // image length in bytes
    uint8_t  sha256[32] = {};                    // decoded from 64 lowercase hex chars
    char     file[kMaxManifestFileLen + 1] = {}; // bare filename, NUL-terminated
};

// Parse `len` bytes of manifest text. Returns false — silently, and this is the ordinary
// failure rather than an exceptional one — if ANY rule in the contract is broken.
//
// Rejection is total: `out` must not be used when this returns false. There is no partial
// application and no error enum, matching decodeDownlink() and for the same reason. A node
// that cannot read a manifest is indistinguishable, from the field, from a node with
// nothing waiting for it, and inventing a distinction between those would create a failure
// mode out of the resting state.
//
// Accepts `\n` and `\r\n`. Ignores blank lines, `#` comments, and unknown keys — the last
// of which is this format's only forward compatibility, and it is one-way: a v2 key
// carrying a REQUIREMENT would be silently skipped, so anything mandatory needs a version
// bump rather than a new key.
bool parseManifest(const char* text, size_t len, FwManifest& out);

} // namespace soundings
