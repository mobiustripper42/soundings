"""Publish a firmware image so nodes will fetch it — `contracts/firmware-manifest-v1.md`.

    gateway/.venv/bin/python tools/publish_firmware.py \
        --bin ../firmware/.pio/build/node/firmware.bin \
        --version 261 \
        --dir /srv/soundings-firmware

⚠ **The manifest is the trigger, and the ORDER here is the whole point.** The image is
copied first under its own name, then the manifest naming it is written to a temporary
file and moved into place with `os.replace`, which is atomic on the same filesystem.

Until that final rename, the new firmware is invisible: the manifest still names the old
image, and nothing about the half-written copy is reachable by any node. A node that polls
during the copy sees the previous release, which is correct rather than merely safe.

Doing it the other way round — manifest first, or a manifest written in place — leaves a
window in which a node fetches an image that is still being written, and the failure
arrives as a bricked node in a tank rather than as an error here.

stdlib only.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
from pathlib import Path

MANIFEST_NAME = "manifest.txt"

#: The ESP32-S3 OTA app slot on this board — app0/app1 in `default_8MB.csv`, confirmed
#: against the linker report. An image larger than this cannot be flashed at all, so
#: publishing one only moves the failure to a node in a tunnel.
APP_SLOT_BYTES = 0x330000


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def render_manifest(version: int, size: int, sha256: str, file: str) -> str:
    """The four required keys, `": "` separated. Nothing computed, nothing optional."""
    return (
        f"version: {version}\n"
        f"size: {size}\n"
        f"sha256: {sha256}\n"
        f"file: {file}\n"
    )


def publish(bin_path: Path, version: int, directory: Path, *, name: str | None = None) -> Path:
    """Copy the image in, then atomically swap the manifest. Returns the manifest path.

    Raises before touching anything if the inputs are wrong — a bad publish should fail
    here, where a person is watching, rather than on a node that has no way to complain.
    """
    if not 0 <= version <= 0xFFFF:
        raise ValueError(f"version {version} does not fit u16 (fw_version is u16 on the wire)")
    if not bin_path.is_file():
        raise FileNotFoundError(bin_path)

    size = bin_path.stat().st_size
    if size == 0:
        raise ValueError(f"{bin_path} is empty")
    if size > APP_SLOT_BYTES:
        raise ValueError(
            f"{bin_path} is {size} bytes — larger than the {APP_SLOT_BYTES}-byte OTA app "
            "slot. It could not be flashed even over USB; publishing it would only move "
            "the failure to a node in a tunnel."
        )

    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)

    file = name or f"soundings-node-{version}.bin"
    # Validate the name we generated against the same rule the parsers enforce, so a
    # publish can never produce a manifest the node will silently refuse.
    if "/" in file or "\\" in file or ":" in file or file.startswith("."):
        raise ValueError(f"image name {file!r} is not a bare filename")

    # 1. The image, first and under its own name. Copied to a temp name and moved, so
    #    even this step never exposes a partial file under the name the manifest will use.
    dest = directory / file
    tmp_bin = directory / (file + ".part")
    shutil.copyfile(bin_path, tmp_bin)
    os.replace(tmp_bin, dest)

    digest = sha256_of(dest)   # hashed AFTER the copy — this proves what actually landed

    # 2. The manifest, last, atomically. This is the moment the release becomes visible.
    manifest = directory / MANIFEST_NAME
    tmp_manifest = directory / (MANIFEST_NAME + ".part")
    tmp_manifest.write_text(render_manifest(version, size, digest, file))
    os.replace(tmp_manifest, manifest)

    return manifest


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--bin", required=True, type=Path, help="path to firmware.bin")
    p.add_argument("--version", required=True, type=int,
                   help="must match kFwVersion in src/esp32/main.cpp for this build")
    p.add_argument("--dir", required=True, type=Path, help="directory the HTTP server serves")
    p.add_argument("--name", default=None, help="image filename (default soundings-node-<v>.bin)")
    args = p.parse_args()

    manifest = publish(args.bin, args.version, args.dir, name=args.name)
    print(manifest.read_text(), end="")
    print(f"-> {manifest}")
    print(
        "\n⚠ The node compares fw_version for INEQUALITY. If this version matches what "
        "the nodes already report, nothing will be flagged — check kFwVersion was bumped."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
