#pragma once
#include <stdint.h>
#include <stddef.h>

namespace soundings {

// Ed25519 signature VERIFICATION — the node's half of the signed firmware
// manifest (contracts/firmware-manifest-v2.md, DEC-013).
//
// ⚠ WHY THIS IS VENDORED RATHER THAN LINKED. The ESP32 has two usable options
// already on the link line — libsodium (Ed25519) and mbedtls (ECDSA-P256) — and
// `[env:native]` has neither. Verification that only runs on target is
// verification no host test can grade, and this repo has already shipped
// nineteen tests that were green against a do-nothing implementation (PR #75).
// One implementation that compiles in both places is worth more than a better
// one the tests cannot reach.
//
// ⚠ THIS VERIFIES ONLY. There is no signing here and no secret material on the
// node — the private key is offline and the publish tool signs with an audited
// library. That asymmetry is deliberate: vendoring a verifier risks a wrong
// answer, which the golden vectors catch; vendoring a signer would risk leaking
// a key, which nothing catches.
//
// Derived from TweetNaCl (Bernstein, van Gastel, Janssen, Lange, Schwabe,
// Smetsers) — public domain, https://tweetnacl.cr.yp.to. Kept close to the
// published source on purpose, so it can be diffed against the original rather
// than reviewed from scratch. SHA-512 comes with it; mbedtls is not reachable
// from the host test tier either.

// The verifier hashes from a fixed-size stack buffer, so the message it will
// consider is bounded. The canonical manifest message is at most 175 bytes with
// every field at its maximum width (see canonicalMessage in fw_manifest.h);
// this leaves headroom without putting a large frame in the OTA call path.
constexpr size_t kMaxSignedMessageBytes = 256;

// Verify a detached Ed25519 signature. Returns false for a bad signature, a
// malformed public key, or a message longer than kMaxSignedMessageBytes —
// rejection is total and silent, matching every other parser in this firmware.
//
// Not constant-time in the signature comparison's surroundings, and it does not
// need to be: everything it touches is public. A timing side channel here leaks
// facts an attacker already holds.
bool verifyEd25519(const uint8_t* msg, size_t msgLen,
                   const uint8_t sig[64], const uint8_t pubkey[32]);

} // namespace soundings
