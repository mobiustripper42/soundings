"""publish_firmware.py — the write half of `contracts/firmware-manifest-v2.md`.

Two properties under test.

**Ordering:** the image must be complete and under its final name before the manifest
naming it exists. A node polling mid-publish must see the previous release, not a
half-written one — that is the entire reason the manifest is the trigger rather than
the `.bin`.

**Signing, and that it cannot be skipped:** `--key` is required, the signature is
computed before the atomic rename, and there is no code path that publishes an unsigned
manifest. A tool that could publish unsigned by accident would produce a fleet that
quietly stopped updating (DEC-013).
"""
from __future__ import annotations

import hashlib
import json
import os
import stat
import sys
from pathlib import Path

import pytest
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import publish_firmware as pf  # noqa: E402

from soundings_gateway import ota  # noqa: E402

PAYLOAD = b"firmware bytes, pretend this is 320 KB"

VECTORS_PATH = Path(__file__).resolve().parents[2] / "contracts/vectors/manifest-sig-v1.json"
GOLDEN = json.loads(VECTORS_PATH.read_text())
TEST_PRIV_RAW = bytes.fromhex(GOLDEN["test_key"]["private_key_hex"])
TEST_PUB = bytes.fromhex(GOLDEN["test_key"]["public_key_hex"])


@pytest.fixture
def image(tmp_path):
    p = tmp_path / "firmware.bin"
    p.write_bytes(PAYLOAD)
    return p


@pytest.fixture
def key(tmp_path):
    """The published test key on disk, at the 0600 the tool expects."""
    p = tmp_path / "ota-signing-key"
    p.write_bytes(TEST_PRIV_RAW)
    p.chmod(0o600)
    return p


def test_publish_writes_an_image_and_a_manifest(image, tmp_path, key):
    served = tmp_path / "srv"
    manifest = pf.publish(image, 261, served, key)

    assert manifest.name == "manifest.txt"
    assert (served / "soundings-node-261.bin").read_bytes() == PAYLOAD


def test_what_it_writes_is_what_the_daemon_reads(image, tmp_path, key):
    """Round trip against the parser the daemon actually uses, rather than against a
    string this test built. The node has a third, independent parser in C++."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)

    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert m is not None
    assert m.version == 261
    assert m.size == len(PAYLOAD)
    assert m.sha256 == hashlib.sha256(PAYLOAD).hexdigest()
    assert m.file == "soundings-node-261.bin"
    # ...and the daemon agrees the image is intact, which is the gate on flagging a node.
    assert ota.validate_image(m, served) is True


def test_what_it_signs_is_what_the_daemon_verifies(image, tmp_path, key):
    """⚠ THE CROSS-IMPLEMENTATION CLAIM. The signer and the verifier are different code
    reached through different modules; if they disagreed about which bytes get signed,
    every publish would produce a manifest the whole fleet silently refuses."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)

    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert ota.verify_manifest(m, TEST_PUB) is True
    # ...and the two modules agree on the message itself, not merely on the verdict.
    assert ota.canonical_message(m) == pf.canonical_message(
        m.version, m.size, m.sha256, m.file)


def test_the_signature_covers_a_later_edit(image, tmp_path, key):
    """Publish, then tamper on disk the way an attacker with write access would.

    The evil image is the SAME LENGTH as the real one, deliberately. A longer one would
    fail the size check and prove nothing about the signature — the interesting case is
    the manifest that is internally consistent in every pre-3.9e respect."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)
    text = (served / "manifest.txt").read_text()

    evil = b"EVIL" + PAYLOAD[4:]
    assert len(evil) == len(PAYLOAD) and evil != PAYLOAD
    (served / "soundings-node-261.bin").write_bytes(evil)
    tampered = text.replace(
        hashlib.sha256(PAYLOAD).hexdigest(), hashlib.sha256(evil).hexdigest())
    (served / "manifest.txt").write_text(tampered)

    m = ota.parse_manifest(tampered)
    assert ota.validate_image(m, served) is True     # the pre-3.9e checks all pass
    assert ota.verify_manifest(m, TEST_PUB) is False  # this is the one that does not


def test_the_sig_line_is_present_and_well_formed(image, tmp_path, key):
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)
    lines = (served / "manifest.txt").read_text().splitlines()
    sig_lines = [l for l in lines if l.startswith("sig: ")]
    assert len(sig_lines) == 1
    assert len(sig_lines[0][len("sig: "):]) == ota.SIG_HEX_LEN


def test_the_hash_describes_what_landed_not_what_was_offered(image, tmp_path, key):
    """Hashed after the copy, so the manifest attests to the file on the server rather
    than to the one on the build machine."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)
    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert m.sha256 == pf.sha256_of(served / "soundings-node-261.bin")


def test_republishing_replaces_the_manifest_and_keeps_the_old_image(image, tmp_path, key):
    """The old image stays on disk deliberately: republishing its manifest is the only
    downgrade path there is, because there is no rollback mechanism."""
    served = tmp_path / "srv"
    pf.publish(image, 260, served, key)
    pf.publish(image, 261, served, key)

    assert (served / "soundings-node-260.bin").is_file()
    assert (served / "soundings-node-261.bin").is_file()
    assert ota.parse_manifest((served / "manifest.txt").read_text()).version == 261


def test_no_part_files_are_left_behind(image, tmp_path, key):
    served = tmp_path / "srv"
    pf.publish(image, 261, served, key)
    assert [p.name for p in served.iterdir() if p.name.endswith(".part")] == []


def test_the_manifest_is_written_last(image, tmp_path, key, monkeypatch):
    """⚠ THE ORDERING PROPERTY, asserted rather than described.

    At the moment the manifest appears, the image it names must already be complete and
    under its final name. Checked by intercepting the atomic move: when the manifest is
    swapped in, the image is inspected as it stands on disk right then.
    """
    served = tmp_path / "srv"
    real_replace = pf.os.replace
    observed = {}

    def spy(src, dst):
        if Path(dst).name == "manifest.txt":
            img = served / "soundings-node-261.bin"
            observed["image_exists_and_is_complete"] = (
                img.is_file() and img.read_bytes() == PAYLOAD
            )
            observed["no_partial_visible"] = not (served / "soundings-node-261.bin.part").exists()
        return real_replace(src, dst)

    monkeypatch.setattr(pf.os, "replace", spy)
    pf.publish(image, 261, served, key)

    assert observed["image_exists_and_is_complete"] is True
    assert observed["no_partial_visible"] is True


def test_the_manifest_is_signed_before_it_is_visible(image, tmp_path, key, monkeypatch):
    """The signing counterpart of the ordering test. There must be no instant at which
    manifest.txt exists unsigned — not even briefly, because a node polls on its own
    schedule and nothing coordinates with this script."""
    served = tmp_path / "srv"
    real_replace = pf.os.replace
    observed = {}

    def spy(src, dst):
        if Path(dst).name == "manifest.txt":
            # Inspect the temp file that is about to BECOME the manifest.
            m = ota.parse_manifest(Path(src).read_text())
            observed["verified_at_swap"] = ota.verify_manifest(m, TEST_PUB)
        return real_replace(src, dst)

    monkeypatch.setattr(pf.os, "replace", spy)
    pf.publish(image, 261, served, key)

    assert observed["verified_at_swap"] is True


# ---- The key ---------------------------------------------------------------

def test_publishing_without_a_key_is_impossible(image, tmp_path):
    """Not a flag, not a default — a positional argument with no fallback. If this ever
    grows an optional key, unsigned publishes come back silently."""
    with pytest.raises(TypeError):
        pf.publish(image, 261, tmp_path / "srv")


def test_a_missing_key_file_is_refused_before_anything_is_written(image, tmp_path):
    served = tmp_path / "srv"
    with pytest.raises(FileNotFoundError):
        pf.publish(image, 261, served, tmp_path / "no-such-key")
    # Nothing was written — not the manifest, and not the 900 KB image either.
    assert not served.exists() or list(served.iterdir()) == []


def test_a_wrong_length_key_is_refused(image, tmp_path):
    bad = tmp_path / "bad-key"
    bad.write_bytes(b"\x00" * 31)
    with pytest.raises(ValueError):
        pf.publish(image, 261, tmp_path / "srv", bad)


def test_gen_key_writes_0600_and_returns_the_public_half(tmp_path):
    path = tmp_path / "sub" / "ota-signing-key"
    pub = pf.gen_key(path)

    assert path.is_file()
    assert len(path.read_bytes()) == 32
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    assert len(pub) == 64 and set(pub) <= set("0123456789abcdef")

    # The returned public key really is the public half of what was written.
    priv = Ed25519PrivateKey.from_private_bytes(path.read_bytes())
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
    assert priv.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex() == pub


def test_gen_key_refuses_to_overwrite(tmp_path):
    """⚠ Clobbering a signing key strands every node carrying its public half — they can
    only be re-keyed over USB, which is the exact cost this task exists to avoid."""
    path = tmp_path / "ota-signing-key"
    first = pf.gen_key(path)
    with pytest.raises(FileExistsError):
        pf.gen_key(path)
    assert Ed25519PrivateKey.from_private_bytes(path.read_bytes()) is not None
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
    still = Ed25519PrivateKey.from_private_bytes(
        path.read_bytes()).public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex()
    assert still == first


def test_a_generated_key_round_trips_through_publish(image, tmp_path):
    """End to end with a key this test made: generate, publish, verify."""
    keypath = tmp_path / "fresh-key"
    pub = bytes.fromhex(pf.gen_key(keypath))
    served = tmp_path / "srv"
    pf.publish(image, 261, served, keypath)

    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert ota.verify_manifest(m, pub) is True
    assert ota.verify_manifest(m, TEST_PUB) is False


def test_a_world_readable_key_warns_but_still_signs(image, tmp_path, capsys):
    """The operator's machine and their call — but a key readable by anyone else has
    stopped being a private key, and that should not be silent."""
    keypath = tmp_path / "loose-key"
    keypath.write_bytes(TEST_PRIV_RAW)
    keypath.chmod(0o644)

    pf.publish(image, 261, tmp_path / "srv", keypath)
    assert "readable beyond its owner" in capsys.readouterr().err


# ---- Refusals --------------------------------------------------------------

def test_an_oversized_image_is_refused(tmp_path, key):
    """Larger than the OTA app slot cannot be flashed at all, so publishing it would only
    move the failure to a node in a tunnel."""
    big = tmp_path / "big.bin"
    big.write_bytes(b"\0" * (pf.APP_SLOT_BYTES + 1))
    served = tmp_path / "srv"

    with pytest.raises(ValueError):
        pf.publish(big, 261, served, key)
    assert not (served / "manifest.txt").exists() if served.exists() else True

    # Positive control: an image at exactly the slot size IS accepted, so this pins the
    # boundary rather than a general dislike of large files.
    ok = tmp_path / "ok.bin"
    ok.write_bytes(b"\0" * pf.APP_SLOT_BYTES)
    assert pf.publish(ok, 261, served, key).is_file()


def test_an_empty_image_is_refused(tmp_path, key):
    empty = tmp_path / "empty.bin"
    empty.write_bytes(b"")
    with pytest.raises(ValueError):
        pf.publish(empty, 261, tmp_path / "srv", key)


def test_a_missing_image_is_refused(tmp_path, key):
    with pytest.raises(FileNotFoundError):
        pf.publish(tmp_path / "nope.bin", 261, tmp_path / "srv", key)


@pytest.mark.parametrize("bad", [-1, 65536, 100000])
def test_a_version_that_does_not_fit_u16_is_refused(image, tmp_path, key, bad):
    with pytest.raises(ValueError):
        pf.publish(image, bad, tmp_path / "srv", key)
    # The largest legal version is accepted.
    assert pf.publish(image, 65535, tmp_path / "srv", key).is_file()


def test_a_name_that_is_not_a_bare_filename_is_refused(image, tmp_path, key):
    """A publish must never be able to produce a manifest the node will silently refuse."""
    with pytest.raises(ValueError):
        pf.publish(image, 261, tmp_path / "srv", key, name="sub/evil.bin")
    assert pf.publish(image, 261, tmp_path / "srv", key, name="fine.bin").is_file()


def test_a_failed_publish_leaves_the_previous_release_serving(image, tmp_path, key):
    """The property that matters operationally: a bad publish is a no-op, not an outage."""
    served = tmp_path / "srv"
    pf.publish(image, 260, served, key)
    before = (served / "manifest.txt").read_text()

    big = tmp_path / "big.bin"
    big.write_bytes(b"\0" * (pf.APP_SLOT_BYTES + 1))
    with pytest.raises(ValueError):
        pf.publish(big, 261, served, key)

    assert (served / "manifest.txt").read_text() == before
    m = ota.parse_manifest(before)
    assert ota.validate_image(m, served) is True
    # Still signed, still verifying — a failed publish did not disturb the release.
    assert ota.verify_manifest(m, TEST_PUB) is True
