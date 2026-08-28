"""Firmware manifest + update policy, daemon side — `contracts/firmware-manifest-v2.md`.

The node has an independent C++ parser and verifier (`firmware/src/core/fw_manifest.cpp`,
`ed25519.cpp`), graded against the **same golden vectors** as the tests here, which is the
arrangement packet-v1 and downlink-v1 already use. A divergence in field handling shows up
as two failing suites rather than as a node that flashed something nobody checked.

⚠ The daemon's job is not only to read the manifest — it is to **refuse to flag a node
for an image it can already tell is broken**. The node's WiFi radio is the single most
expensive thing it owns, and waking it for a truncated `.bin` spends that for a result
knowable here for free. Since v2 that includes refusing a manifest that does not verify
(DEC-013).
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pytest
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

from soundings_gateway import ota
from soundings_gateway.downlink import FLAGS_NONE, FLAG_UPDATE_WAITING

# The shared golden file, written by contracts/tools/gen_sig_vectors.py and read by both
# ends. Its "private" key is published on purpose: it is a test key, derived from a fixed
# ASCII phrase so this file's fixtures can be regenerated rather than trusted.
VECTORS_PATH = Path(__file__).resolve().parents[2] / "contracts/vectors/manifest-sig-v1.json"
GOLDEN = json.loads(VECTORS_PATH.read_text())

TEST_PRIV = Ed25519PrivateKey.from_private_bytes(
    bytes.fromhex(GOLDEN["test_key"]["private_key_hex"]))
TEST_PUB = bytes.fromhex(GOLDEN["test_key"]["public_key_hex"])

# The shared literal's payload. The hash is SHA-256 of the four ASCII bytes b"test" and the
# tests below RECOMPUTE it rather than trusting the constant — the contract's first draft
# carried an invented hash that looked just as real.
PAYLOAD = b"test"
SHA = hashlib.sha256(PAYLOAD).hexdigest()


def signed(version: int = 261, size: int = 4, sha: str | None = None,
           file: str = "t.bin") -> str:
    """A correctly signed manifest, for tests that need to vary a field.

    Signs with the published test key. The golden vectors cover the fixed cases and the
    cross-implementation agreement; this exists so a policy test can say "version 262"
    without hand-maintaining a signature for every value.
    """
    sha = SHA if sha is None else sha
    msg = f"version: {version}\nsize: {size}\nsha256: {sha}\nfile: {file}\n"
    return msg + f"sig: {TEST_PRIV.sign(msg.encode()).hex()}\n"


GOOD = signed()


def test_the_shared_literal_uses_the_hash_it_claims():
    """Guards the constant above against the exact mistake the contract made once."""
    assert SHA == "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
    assert f"sha256: {SHA}" in GOOD


# ---- The golden vectors ----------------------------------------------------
# The cross-implementation check. The same twelve cases run in
# firmware/test/test_manifest_sig, against a completely different verifier.

@pytest.mark.parametrize("vec", GOLDEN["vectors"], ids=lambda v: v["name"])
def test_golden_vector(vec):
    m = ota.parse_manifest(vec["manifest"])
    assert (m is not None) is vec["expect_parse"], vec["description"]
    if m is None:
        return
    assert ota.verify_manifest(m, TEST_PUB) is vec["expect_verify"], vec["description"]


def test_the_golden_file_covers_both_outcomes():
    """A guard on the fixtures themselves. If every vector expected the same answer, the
    parametrized test above would pass against a verifier that returns a constant."""
    outcomes = {(v["expect_parse"], v["expect_verify"]) for v in GOLDEN["vectors"]}
    assert (True, True) in outcomes
    assert (True, False) in outcomes
    assert (False, False) in outcomes


# ---- canonical_message -----------------------------------------------------

def test_canonical_message_is_exactly_the_four_fields():
    """Pins the format rather than checking two implementations agree with each other.
    The same bytes are asserted literally in test_manifest_sig.cpp."""
    m = ota.parse_manifest(GOOD)
    assert ota.canonical_message(m) == (
        b"version: 261\n"
        b"size: 4\n"
        b"sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        b"file: t.bin\n"
    )


def test_canonical_message_ignores_layout():
    """Reordered, commented, CRLF-terminated, trailing spaces — same four values, so the
    signed bytes must be identical. This is what lets three parsers agree without ever
    exchanging the raw file."""
    a = ota.parse_manifest(GOOD)
    b = ota.parse_manifest(
        "# published by hand\r\n"
        "\r\n"
        "file: t.bin   \r\n"
        f"sha256: {SHA}\r\n"
        "size: 4\r\n"
        "version: 261\r\n"
        f"sig: {a.sig}\r\n"
    )
    assert b is not None
    assert ota.canonical_message(a) == ota.canonical_message(b)
    # ...and the shared signature verifies against both, which is the practical claim.
    assert ota.verify_manifest(a, TEST_PUB) is True
    assert ota.verify_manifest(b, TEST_PUB) is True


def test_the_sig_line_is_not_part_of_what_is_signed():
    """Otherwise signing would be circular and nothing could ever verify."""
    m = ota.parse_manifest(GOOD)
    assert b"sig:" not in ota.canonical_message(m)


# ---- verify_manifest -------------------------------------------------------

def test_a_wrong_public_key_does_not_verify():
    other = Ed25519PrivateKey.generate().public_key()
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
    raw = other.public_bytes(Encoding.Raw, PublicFormat.Raw)
    m = ota.parse_manifest(GOOD)
    assert ota.verify_manifest(m, raw) is False
    assert ota.verify_manifest(m, TEST_PUB) is True


def test_a_malformed_public_key_does_not_raise():
    """The key comes from a config value a person typed. A wrong length must be a
    refusal, not a traceback out of the decode loop."""
    m = ota.parse_manifest(GOOD)
    assert ota.verify_manifest(m, b"\x00" * 31) is False


def test_verify_of_none_is_false():
    assert ota.verify_manifest(None, TEST_PUB) is False


@pytest.mark.parametrize("field,replacement", [
    ("version: 261", "version: 262"),
    ("size: 4", "size: 5"),
    (f"sha256: {SHA}", "sha256: " + "0" * 64),
    ("file: t.bin", "file: evil.bin"),
])
def test_editing_any_signed_field_breaks_the_signature(field, replacement):
    """Each of the four fields individually. A verifier that hashed only some of them
    would still pass the single happy-path test."""
    tampered = ota.parse_manifest(GOOD.replace(field, replacement))
    assert tampered is not None, "the tampered manifest should still PARSE"
    assert ota.verify_manifest(tampered, TEST_PUB) is False
    assert ota.verify_manifest(ota.parse_manifest(GOOD), TEST_PUB) is True


# ---- parse_manifest --------------------------------------------------------

def test_parses_the_shared_literal():
    m = ota.parse_manifest(GOOD)
    assert m is not None
    assert (m.version, m.size, m.sha256, m.file) == (261, 4, SHA, "t.bin")
    assert len(m.sig) == ota.SIG_HEX_LEN


def test_key_order_does_not_matter():
    sig = ota.parse_manifest(GOOD).sig
    m = ota.parse_manifest(f"sig: {sig}\nfile: t.bin\nsha256: {SHA}\nsize: 4\nversion: 261\n")
    assert m is not None and m.version == 261


def test_unknown_keys_comments_and_blanks_are_ignored():
    sig = ota.parse_manifest(GOOD).sig
    m = ota.parse_manifest(
        "# written by publish_firmware.py\n\n"
        "version: 261\nreleased_by: eric\nsize: 4\n"
        f"sha256: {SHA}\nfile: t.bin\nsig: {sig}\n"
    )
    assert m is not None and m.version == 261


def test_a_near_miss_key_does_not_satisfy_the_requirement():
    """`signature:` is not `sig:`. An ignored unknown key must not count as the field."""
    sig = ota.parse_manifest(GOOD).sig
    assert ota.parse_manifest(
        f"version: 261\nsize: 4\nsha256: {SHA}\nfile: t.bin\nsignature: {sig}\n"
    ) is None


def test_crlf_parses():
    m = ota.parse_manifest(GOOD.replace("\n", "\r\n"))
    assert m is not None and m.file == "t.bin"
    assert ota.verify_manifest(m, TEST_PUB) is True


@pytest.mark.parametrize("drop", ["version", "size", "sha256", "file", "sig"])
def test_a_missing_required_key_is_rejected(drop):
    text = "".join(l + "\n" for l in GOOD.splitlines() if not l.startswith(drop))
    assert ota.parse_manifest(text) is None
    # Positive control, same test: the complete manifest still parses. Without it this
    # passes against a parser that returns None for everything.
    assert ota.parse_manifest(GOOD) is not None


def test_an_unsigned_v1_manifest_is_rejected():
    """The old format, unchanged. It was valid before 3.9e and must not be now — this is
    the assertion that makes signing mandatory rather than advisory, and it is the reason
    the format got a version bump instead of an amendment (DEC-013)."""
    assert ota.parse_manifest(
        f"version: 261\nsize: 4\nsha256: {SHA}\nfile: t.bin\n"
    ) is None


@pytest.mark.parametrize("bad_sha", [
    "9f86d081",                                                            # too short
    SHA.upper(),                                                           # uppercase
    "z" * 64,                                                              # not hex
    SHA + "00",                                                            # too long
])
def test_a_bad_hash_is_rejected(bad_sha):
    assert ota.parse_manifest(GOOD.replace(SHA, bad_sha)) is None
    assert ota.parse_manifest(GOOD) is not None


@pytest.mark.parametrize("bad_sig", [
    "a" * 127,          # one short
    "a" * 129,          # one long
    "z" * 128,          # not hex
    "A" * 128,          # uppercase — one canonical spelling, same rule as sha256
    "",                 # empty
])
def test_a_bad_signature_field_is_rejected_at_parse(bad_sig):
    """Shape only. Whether it verifies is a separate question with a separate answer,
    and conflating them would mean a malformed field and a forged one look alike."""
    good_sig = ota.parse_manifest(GOOD).sig
    assert ota.parse_manifest(GOOD.replace(good_sig, bad_sig)) is None
    assert ota.parse_manifest(GOOD) is not None


@pytest.mark.parametrize("bad_file", [
    "sub/t.bin", "..bin", "../t.bin", "http://elsewhere/evil.bin", "", "a b.bin",
])
def test_a_bad_filename_is_rejected(bad_file):
    """`file` resolves against a fixed base URL. A value that can express a path is a
    value that can point a field node at an arbitrary address, and it has no rollback."""
    assert ota.parse_manifest(GOOD.replace("file: t.bin", f"file: {bad_file}")) is None
    assert ota.parse_manifest(GOOD) is not None


@pytest.mark.parametrize("bad", ["65536", "-1", "lots", ""])
def test_a_bad_version_is_rejected(bad):
    assert ota.parse_manifest(GOOD.replace("version: 261", f"version: {bad}")) is None
    # 65535 is the largest legal value and IS accepted — pins the boundary rather than a
    # general dislike of big numbers.
    assert ota.parse_manifest(signed(version=65535)) is not None


@pytest.mark.parametrize("bad", ["0", "-4", "four"])
def test_a_bad_size_is_rejected(bad):
    assert ota.parse_manifest(GOOD.replace("size: 4", f"size: {bad}")) is None
    assert ota.parse_manifest(GOOD) is not None


def test_an_oversized_manifest_is_rejected():
    assert ota.parse_manifest("#" + "x" * ota.MAX_MANIFEST_BYTES) is None
    assert ota.parse_manifest(GOOD) is not None


def test_a_signed_manifest_still_fits_the_byte_cap():
    """The cap did not move when `sig` arrived. Worst case: every field at maximum width.
    Without this, "it fits" is an assumption rather than a measurement."""
    worst = (
        f"version: 65535\nsize: 4294967295\nsha256: {'a' * 64}\n"
        f"file: {'x' * 63}\nsig: {'b' * 128}\n"
    )
    assert len(worst.encode()) <= ota.MAX_MANIFEST_BYTES
    assert len(worst.splitlines()) <= ota.MAX_MANIFEST_LINES


def test_a_manifest_with_too_many_lines_is_rejected():
    assert ota.parse_manifest("\n".join(["#"] * (ota.MAX_MANIFEST_LINES + 1))) is None
    assert ota.parse_manifest(GOOD) is not None


# ---- validate_image --------------------------------------------------------

@pytest.fixture
def served(tmp_path):
    """A firmware directory with a signed manifest and a matching image."""
    (tmp_path / "t.bin").write_bytes(PAYLOAD)
    (tmp_path / "manifest.txt").write_text(GOOD)
    return tmp_path


def policy(directory):
    return ota.UpdatePolicy(directory, TEST_PUB)


def test_a_matching_image_validates(served):
    m = ota.parse_manifest(GOOD)
    assert ota.validate_image(m, served) is True


def test_a_missing_image_does_not_validate(served):
    (served / "t.bin").unlink()
    assert ota.validate_image(ota.parse_manifest(GOOD), served) is False


def test_a_short_image_does_not_validate(served):
    """The half-finished-copy case the whole manifest-as-trigger design exists for."""
    (served / "t.bin").write_bytes(PAYLOAD[:2])
    assert ota.validate_image(ota.parse_manifest(GOOD), served) is False
    # Positive control: restoring the full image validates again.
    (served / "t.bin").write_bytes(PAYLOAD)
    assert ota.validate_image(ota.parse_manifest(GOOD), served) is True


def test_an_image_of_the_right_size_but_wrong_content_does_not_validate(served):
    """Size alone is not integrity — this is why the manifest carries a hash."""
    (served / "t.bin").write_bytes(b"tesT")
    assert len((served / "t.bin").read_bytes()) == 4
    assert ota.validate_image(ota.parse_manifest(GOOD), served) is False
    (served / "t.bin").write_bytes(PAYLOAD)
    assert ota.validate_image(ota.parse_manifest(GOOD), served) is True


# ---- UpdatePolicy ----------------------------------------------------------

def reading(fw_version: int, node_id: int = 7) -> dict:
    return {"node_id": node_id, "fw_version": fw_version, "seq": 1}


def test_the_policy_requires_a_real_key():
    """No default, no None. An optional key would mean an optional verification, and
    "verification off by accident" is the failure this whole task exists to remove."""
    with pytest.raises(ValueError):
        ota.UpdatePolicy(Path("."), None)
    with pytest.raises(ValueError):
        ota.UpdatePolicy(Path("."), b"\x00" * 31)
    with pytest.raises(TypeError):
        ota.UpdatePolicy(Path("."))


def test_a_stale_node_is_flagged(served):
    assert policy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_current_node_is_not_flagged(served):
    p = policy(served)
    assert p.flags_for(reading(261)) == FLAGS_NONE
    # Positive control: a different version from the SAME policy is flagged, so this is
    # not passing against a policy that never flags anything.
    assert p.flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_newer_node_is_still_flagged(served):
    """Inequality, not ordering. There is no rollback mechanism, so republishing an
    older manifest is the only downgrade path there is and it has to work."""
    assert policy(served).flags_for(reading(999)) == FLAG_UPDATE_WAITING


def test_an_absent_manifest_flags_nothing(tmp_path):
    p = policy(tmp_path)
    assert p.flags_for(reading(260)) == FLAGS_NONE
    # Positive control: the same policy against a directory that HAS a manifest flags.
    (tmp_path / "t.bin").write_bytes(PAYLOAD)
    (tmp_path / "manifest.txt").write_text(GOOD)
    assert p.flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_broken_image_flags_nothing(served):
    """⚠ THE POINT OF VALIDATING DAEMON-SIDE. The node's WiFi is the most expensive thing
    it owns; waking it for an image we can already see is truncated spends that for
    nothing. A half-finished scp must be invisible."""
    (served / "t.bin").write_bytes(PAYLOAD[:1])
    assert policy(served).flags_for(reading(260)) == FLAGS_NONE
    # Positive control: once the copy completes, the same stale node IS flagged.
    (served / "t.bin").write_bytes(PAYLOAD)
    assert policy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_malformed_manifest_flags_nothing(served):
    (served / "manifest.txt").write_text("version: 261\nsize: 4\n")   # no sha, file or sig
    assert policy(served).flags_for(reading(260)) == FLAGS_NONE
    (served / "manifest.txt").write_text(GOOD)
    assert policy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_forged_manifest_flags_nothing(served):
    """⚠ THE 3.9e ASSERTION, DAEMON SIDE. An attacker who can write to the served
    directory can produce a manifest whose sha256 matches their own image — every
    pre-3.9e check passes. The signature is the only thing that does not."""
    evil = PAYLOAD + b"evil"
    (served / "t.bin").write_bytes(evil)
    forged = (
        f"version: 262\nsize: {len(evil)}\n"
        f"sha256: {hashlib.sha256(evil).hexdigest()}\nfile: t.bin\n"
        f"sig: {'0' * 128}\n"
    )
    (served / "manifest.txt").write_text(forged)

    # Internally consistent — it would have flagged before this task.
    m = ota.parse_manifest(forged)
    assert m is not None
    assert ota.validate_image(m, served) is True

    assert policy(served).flags_for(reading(260)) == FLAGS_NONE


def test_a_manifest_signed_by_the_wrong_key_flags_nothing(served):
    """The sharper version of the test above: a real, valid Ed25519 signature — just not
    by the fleet's key. Anything that checks the signature's SHAPE and not its ISSUER
    passes the forged-manifest test and fails this one."""
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
    attacker = Ed25519PrivateKey.generate()
    msg = f"version: 262\nsize: 4\nsha256: {SHA}\nfile: t.bin\n"
    (served / "manifest.txt").write_text(msg + f"sig: {attacker.sign(msg.encode()).hex()}\n")

    # It verifies perfectly — under the attacker's key.
    attacker_pub = attacker.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    assert ota.verify_manifest(ota.parse_manifest((served / "manifest.txt").read_text()),
                               attacker_pub) is True

    assert policy(served).flags_for(reading(260)) == FLAGS_NONE


def test_a_reading_without_fw_version_flags_nothing(served):
    """Defensive: every decoded packet carries fw_version, but the policy is handed a
    dict and must not raise inside the gateway's decode loop."""
    assert policy(served).flags_for({"node_id": 7}) == FLAGS_NONE
    assert policy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_the_policy_rereads_the_manifest_each_time(served):
    """Publishing new firmware must not require restarting the daemon — the manifest is
    the trigger, and a cached one would mean the trigger never fires."""
    p = policy(served)
    assert p.flags_for(reading(261)) == FLAGS_NONE

    (served / "t.bin").write_bytes(PAYLOAD)
    (served / "manifest.txt").write_text(signed(version=262))
    assert p.flags_for(reading(261)) == FLAG_UPDATE_WAITING
