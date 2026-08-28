#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ed25519.h"

namespace soundings {

// Firmware manifest v2 — the parse half of contracts/firmware-manifest-v2.md.
//
// The node fetches this over HTTP before it fetches an image. It is the first thing in
// this project that parses input a REMOTE party chose the length and shape of — every
// other parser reads a fixed-width frame off a radio or a serial port. That is why the
// bounds below are constants with tests behind them rather than assumptions.
//
// ⚠ V2 ADDS A REQUIRED `sig` FIELD, AND THAT IS WHY IT IS A NEW VERSION RATHER THAN AN
// AMENDMENT. v1's own rule — unknown keys are ignored, so anything mandatory needs a
// version bump — is precisely the rule a required `sig` would break if it were bolted
// onto v1 (DEC-013). Nothing on the wire distinguishes the two: a v2 manifest is a v1
// manifest with a `sig` line, and which parser runs is decided at compile time by which
// firmware is on the chip. That asymmetry is what makes the fleet transition free — a
// node still running v1 ignores `sig` and takes the first signed image over the air,
// with no trip to the tank — and it is a one-time property, not a design to lean on.
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
    uint8_t  sig[64] = {};                       // decoded from 128 lowercase hex chars
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

// Render the exact bytes that were signed, into `buf`. Returns the length written, or 0
// if the buffer is too small — NEVER a truncated message, because a truncated message
// still hashes to something, and something is what an attacker is hoping for.
//
// ⚠ THIS IS A RECONSTRUCTION, NOT A COPY. It renders the four parsed values in the fixed
// order version, size, sha256, file — it never echoes the bytes as they arrived. That is
// the whole reason three independent implementations can agree on what was signed: line
// order, comments, blank lines, trailing spaces and CRLF all vanish before signing, so
// there is no "which bytes exactly" question left for them to answer differently.
//
// The cost, stated plainly: this signs the MEANING, not the file. A future unknown key
// would not be covered by the signature. That is already true of the format — unknown
// keys are ignored, so they cannot carry a requirement — but it is the thing to check
// first if v3 ever adds a field.
size_t canonicalMessage(const FwManifest& m, char* buf, size_t cap);

// Does this manifest's signature verify against `pubkey`? False on any failure, including
// a buffer or bound problem — there is no outcome here other than "flash it" and "don't".
bool verifyManifest(const FwManifest& m, const uint8_t pubkey[32]);

} // namespace soundings
