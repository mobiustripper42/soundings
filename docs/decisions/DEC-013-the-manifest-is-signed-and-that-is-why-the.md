---
schema: 1
id: DEC-013
title: "The manifest is signed, and that is why the format got a v2"
topic: "Firmware architecture"
status: "active"
date: "2026-08-28"
ruling: "Firmware manifests carry an Ed25519 signature the node checks before fetching an image. Unsigned ones are refused."
claims:
  - kind: "file"
    target: "contracts/firmware-manifest-v2.md"
  - kind: "file"
    target: "firmware/src/core/ed25519.cpp"
  - kind: "test"
    target: "contracts/vectors/manifest-sig-v1.json"
revisit_if: "An image needs a second signing key, or a node can be re-keyed without USB."
---

## DEC-013: The manifest is signed, and that is why the format got a v2

See also DEC-012, which stands whole: this is its own "revisit before the fleet grows"
condition, met early rather than a change of mind.

Its three checks compared the image to a `sha256` from the same unauthenticated
channel, so anyone answering at `SOUNDINGS_OTA_HOST` passed them. Bit 0 of a CRC-only
downlink made that code execution, unrecoverable.

Adding `sig` needed a new spec version, not a new field on v1: rule 3 of the v1 spec
says so, because parsers skip fields they do not know, silently, and a required one
bolted onto v1 would leave "v1" meaning two things at once. That same hazard makes the
rollout free, once — the fielded node parses v1 and ignores `sig`.

Ed25519 is vendored because mbedtls lacks it and libsodium is unreachable from
`[env:native]`; verification only the target can run is verification no host test can
grade, which this repo has shipped. It verifies only — the signer uses `cryptography`,
since a wrong verifier gives a wrong answer tests catch, and a wrong signer leaks the
private key, which nothing does.

What gets signed is a reconstruction — four fields, fixed order, never the manifest as
received — so three parsers cannot disagree about the bytes.

Rejected: ECDSA-P256 (variable-length signature, leakable nonce) and TLS alone
(authenticates the server, not what it serves).
