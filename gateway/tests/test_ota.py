"""Firmware manifest + update policy, daemon side — `contracts/firmware-manifest-v1.md`.

The node has an independent C++ parser (`firmware/src/core/fw_manifest.cpp`), graded
against the **same literal** as the tests here, which is the arrangement packet-v1 and
downlink-v1 already use. A divergence in field handling shows up as two failing suites
rather than as a node that flashed something nobody checked.

⚠ The daemon's job is not only to read the manifest — it is to **refuse to flag a node
for an image it can already tell is broken**. The node's WiFi radio is the single most
expensive thing it owns, and waking it for a truncated `.bin` spends that for a result
knowable here for free.
"""
from __future__ import annotations

import hashlib

import pytest

from soundings_gateway import ota
from soundings_gateway.downlink import FLAGS_NONE, FLAG_UPDATE_WAITING

# The shared literal, byte-identical to test_manifest_parse.cpp. The hash is SHA-256 of
# the four ASCII bytes b"test" and the tests below RECOMPUTE it rather than trusting the
# constant — the contract's first draft carried an invented hash that looked just as real.
PAYLOAD = b"test"
SHA = hashlib.sha256(PAYLOAD).hexdigest()

GOOD = (
    "version: 261\n"
    "size: 4\n"
    f"sha256: {SHA}\n"
    "file: t.bin\n"
)


def test_the_shared_literal_uses_the_hash_it_claims():
    """Guards the constant above against the exact mistake the contract made once."""
    assert SHA == "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
    assert f"sha256: {SHA}" in GOOD


# ---- parse_manifest --------------------------------------------------------

def test_parses_the_shared_literal():
    m = ota.parse_manifest(GOOD)
    assert m is not None
    assert (m.version, m.size, m.sha256, m.file) == (261, 4, SHA, "t.bin")


def test_key_order_does_not_matter():
    m = ota.parse_manifest(f"file: t.bin\nsha256: {SHA}\nsize: 4\nversion: 261\n")
    assert m is not None and m.version == 261


def test_unknown_keys_comments_and_blanks_are_ignored():
    m = ota.parse_manifest(
        "# written by publish_firmware.py\n\n"
        "version: 261\nreleased_by: eric\nsize: 4\n"
        f"sha256: {SHA}\nfile: t.bin\n"
    )
    assert m is not None and m.version == 261


def test_crlf_parses():
    m = ota.parse_manifest(GOOD.replace("\n", "\r\n"))
    assert m is not None and m.file == "t.bin"


@pytest.mark.parametrize("drop", ["version", "size", "sha256", "file"])
def test_a_missing_required_key_is_rejected(drop):
    text = "".join(l + "\n" for l in GOOD.splitlines() if not l.startswith(drop))
    assert ota.parse_manifest(text) is None
    # Positive control, same test: the complete manifest still parses. Without it this
    # passes against a parser that returns None for everything.
    assert ota.parse_manifest(GOOD) is not None


@pytest.mark.parametrize("bad_sha", [
    "9f86d081",                                                            # too short
    SHA.upper(),                                                           # uppercase
    "z" * 64,                                                              # not hex
    SHA + "00",                                                            # too long
])
def test_a_bad_hash_is_rejected(bad_sha):
    assert ota.parse_manifest(GOOD.replace(SHA, bad_sha)) is None
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
    assert ota.parse_manifest(GOOD.replace("version: 261", "version: 65535")) is not None


@pytest.mark.parametrize("bad", ["0", "-4", "four"])
def test_a_bad_size_is_rejected(bad):
    assert ota.parse_manifest(GOOD.replace("size: 4", f"size: {bad}")) is None
    assert ota.parse_manifest(GOOD) is not None


def test_an_oversized_manifest_is_rejected():
    assert ota.parse_manifest("#" + "x" * ota.MAX_MANIFEST_BYTES) is None
    assert ota.parse_manifest(GOOD) is not None


def test_a_manifest_with_too_many_lines_is_rejected():
    assert ota.parse_manifest("\n".join(["#"] * (ota.MAX_MANIFEST_LINES + 1))) is None
    assert ota.parse_manifest(GOOD) is not None


# ---- validate_image --------------------------------------------------------

@pytest.fixture
def served(tmp_path):
    """A firmware directory with a manifest and a matching image."""
    (tmp_path / "t.bin").write_bytes(PAYLOAD)
    (tmp_path / "manifest.txt").write_text(GOOD)
    return tmp_path


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


def test_a_stale_node_is_flagged(served):
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_current_node_is_not_flagged(served):
    p = ota.UpdatePolicy(served)
    assert p.flags_for(reading(261)) == FLAGS_NONE
    # Positive control: a different version from the SAME policy is flagged, so this is
    # not passing against a policy that never flags anything.
    assert p.flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_newer_node_is_still_flagged(served):
    """Inequality, not ordering. There is no rollback mechanism, so republishing an
    older manifest is the only downgrade path there is and it has to work."""
    assert ota.UpdatePolicy(served).flags_for(reading(999)) == FLAG_UPDATE_WAITING


def test_an_absent_manifest_flags_nothing(tmp_path):
    p = ota.UpdatePolicy(tmp_path)
    assert p.flags_for(reading(260)) == FLAGS_NONE
    # Positive control: the same policy against a directory that HAS a manifest flags.
    (tmp_path / "t.bin").write_bytes(PAYLOAD)
    (tmp_path / "manifest.txt").write_text(GOOD)
    assert ota.UpdatePolicy(tmp_path).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_broken_image_flags_nothing(served):
    """⚠ THE POINT OF VALIDATING DAEMON-SIDE. The node's WiFi is the most expensive thing
    it owns; waking it for an image we can already see is truncated spends that for
    nothing. A half-finished scp must be invisible."""
    (served / "t.bin").write_bytes(PAYLOAD[:1])
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAGS_NONE
    # Positive control: once the copy completes, the same stale node IS flagged.
    (served / "t.bin").write_bytes(PAYLOAD)
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_malformed_manifest_flags_nothing(served):
    (served / "manifest.txt").write_text("version: 261\nsize: 4\n")   # no sha, no file
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAGS_NONE
    (served / "manifest.txt").write_text(GOOD)
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_a_reading_without_fw_version_flags_nothing(served):
    """Defensive: every decoded packet carries fw_version, but the policy is handed a
    dict and must not raise inside the gateway's decode loop."""
    assert ota.UpdatePolicy(served).flags_for({"node_id": 7}) == FLAGS_NONE
    assert ota.UpdatePolicy(served).flags_for(reading(260)) == FLAG_UPDATE_WAITING


def test_the_policy_rereads_the_manifest_each_time(served):
    """Publishing new firmware must not require restarting the daemon — the manifest is
    the trigger, and a cached one would mean the trigger never fires."""
    p = ota.UpdatePolicy(served)
    assert p.flags_for(reading(261)) == FLAGS_NONE

    (served / "t.bin").write_bytes(PAYLOAD)
    (served / "manifest.txt").write_text(GOOD.replace("version: 261", "version: 262"))
    assert p.flags_for(reading(261)) == FLAG_UPDATE_WAITING
