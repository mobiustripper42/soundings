"""publish_firmware.py — the write half of `contracts/firmware-manifest-v1.md`.

The property under test is **ordering**: the image must be complete and under its final
name before the manifest naming it exists. A node polling mid-publish must see the
previous release, not a half-written one — that is the entire reason the manifest is the
trigger rather than the `.bin`.
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import publish_firmware as pf  # noqa: E402

from soundings_gateway import ota  # noqa: E402

PAYLOAD = b"firmware bytes, pretend this is 320 KB"


@pytest.fixture
def image(tmp_path):
    p = tmp_path / "firmware.bin"
    p.write_bytes(PAYLOAD)
    return p


def test_publish_writes_an_image_and_a_manifest(image, tmp_path):
    served = tmp_path / "srv"
    manifest = pf.publish(image, 261, served)

    assert manifest.name == "manifest.txt"
    assert (served / "soundings-node-261.bin").read_bytes() == PAYLOAD


def test_what_it_writes_is_what_the_daemon_reads(image, tmp_path):
    """Round trip against the parser the daemon actually uses, rather than against a
    string this test built. The node has a third, independent parser in C++."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served)

    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert m is not None
    assert m.version == 261
    assert m.size == len(PAYLOAD)
    assert m.sha256 == hashlib.sha256(PAYLOAD).hexdigest()
    assert m.file == "soundings-node-261.bin"
    # ...and the daemon agrees the image is intact, which is the gate on flagging a node.
    assert ota.validate_image(m, served) is True


def test_the_hash_describes_what_landed_not_what_was_offered(image, tmp_path):
    """Hashed after the copy, so the manifest attests to the file on the server rather
    than to the one on the build machine."""
    served = tmp_path / "srv"
    pf.publish(image, 261, served)
    m = ota.parse_manifest((served / "manifest.txt").read_text())
    assert m.sha256 == pf.sha256_of(served / "soundings-node-261.bin")


def test_republishing_replaces_the_manifest_and_keeps_the_old_image(image, tmp_path):
    """The old image stays on disk deliberately: republishing its manifest is the only
    downgrade path there is, because there is no rollback mechanism."""
    served = tmp_path / "srv"
    pf.publish(image, 260, served)
    pf.publish(image, 261, served)

    assert (served / "soundings-node-260.bin").is_file()
    assert (served / "soundings-node-261.bin").is_file()
    assert ota.parse_manifest((served / "manifest.txt").read_text()).version == 261


def test_no_part_files_are_left_behind(image, tmp_path):
    served = tmp_path / "srv"
    pf.publish(image, 261, served)
    assert [p.name for p in served.iterdir() if p.name.endswith(".part")] == []


def test_the_manifest_is_written_last(image, tmp_path, monkeypatch):
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
    pf.publish(image, 261, served)

    assert observed["image_exists_and_is_complete"] is True
    assert observed["no_partial_visible"] is True


def test_an_oversized_image_is_refused(tmp_path):
    """Larger than the OTA app slot cannot be flashed at all, so publishing it would only
    move the failure to a node in a tunnel."""
    big = tmp_path / "big.bin"
    big.write_bytes(b"\0" * (pf.APP_SLOT_BYTES + 1))
    served = tmp_path / "srv"

    with pytest.raises(ValueError):
        pf.publish(big, 261, served)
    assert not (served / "manifest.txt").exists() if served.exists() else True

    # Positive control: an image at exactly the slot size IS accepted, so this pins the
    # boundary rather than a general dislike of large files.
    ok = tmp_path / "ok.bin"
    ok.write_bytes(b"\0" * pf.APP_SLOT_BYTES)
    assert pf.publish(ok, 261, served).is_file()


def test_an_empty_image_is_refused(tmp_path):
    empty = tmp_path / "empty.bin"
    empty.write_bytes(b"")
    with pytest.raises(ValueError):
        pf.publish(empty, 261, tmp_path / "srv")


def test_a_missing_image_is_refused(tmp_path):
    with pytest.raises(FileNotFoundError):
        pf.publish(tmp_path / "nope.bin", 261, tmp_path / "srv")


@pytest.mark.parametrize("bad", [-1, 65536, 100000])
def test_a_version_that_does_not_fit_u16_is_refused(image, tmp_path, bad):
    with pytest.raises(ValueError):
        pf.publish(image, bad, tmp_path / "srv")
    # The largest legal version is accepted.
    assert pf.publish(image, 65535, tmp_path / "srv").is_file()


def test_a_name_that_is_not_a_bare_filename_is_refused(image, tmp_path):
    """A publish must never be able to produce a manifest the node will silently refuse."""
    with pytest.raises(ValueError):
        pf.publish(image, 261, tmp_path / "srv", name="sub/evil.bin")
    assert pf.publish(image, 261, tmp_path / "srv", name="fine.bin").is_file()


def test_a_failed_publish_leaves_the_previous_release_serving(image, tmp_path):
    """The property that matters operationally: a bad publish is a no-op, not an outage."""
    served = tmp_path / "srv"
    pf.publish(image, 260, served)
    before = (served / "manifest.txt").read_text()

    big = tmp_path / "big.bin"
    big.write_bytes(b"\0" * (pf.APP_SLOT_BYTES + 1))
    with pytest.raises(ValueError):
        pf.publish(big, 261, served)

    assert (served / "manifest.txt").read_text() == before
    assert ota.validate_image(ota.parse_manifest(before), served) is True
