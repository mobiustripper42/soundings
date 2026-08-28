"""Publish a firmware image so nodes will fetch it — `contracts/firmware-manifest-v2.md`.

    gateway/.venv/bin/python tools/publish_firmware.py \
        --bin ../firmware/.pio/build/node/firmware.bin \
        --version 261 \
        --dir /srv/soundings-firmware \
        --key ~/.soundings/ota-signing-key

One-time setup, before the first signed release:

    gateway/.venv/bin/python tools/publish_firmware.py --gen-key ~/.soundings/ota-signing-key

⚠ **The manifest is the trigger, and the ORDER here is the whole point.** The image is
copied first under its own name, then the manifest naming it is written to a temporary
file and moved into place with `os.replace`, which is atomic on the same filesystem.

Until that final rename, the new firmware is invisible: the manifest still names the old
image, and nothing about the half-written copy is reachable by any node. A node that polls
during the copy sees the previous release, which is correct rather than merely safe.

Doing it the other way round — manifest first, or a manifest written in place — leaves a
window in which a node fetches an image that is still being written, and the failure
arrives as a bricked node in a tank rather than as an error here.

⚠ **SIGNING IS NOT OPTIONAL AND CANNOT BE FORGOTTEN.** `--key` is required, and the
signature is computed before the atomic rename, so the manifest is signed in the same
instant it becomes visible. There is no flag to publish unsigned: a node running v2
firmware would refuse the result anyway, and the failure would surface as a fleet that
quietly stopped updating rather than as an error here where a person is watching
(DEC-013).

stdlib, plus `cryptography` for Ed25519. The private key never enters this repo —
`.gitignore` carries a rule for it, and the reason it carries one.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import stat
import sys
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey, Ed25519PublicKey,
)
from cryptography.hazmat.primitives.serialization import (
    Encoding, NoEncryption, PrivateFormat, PublicFormat,
)

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


def canonical_message(version: int, size: int, sha256: str, file: str) -> bytes:
    """The exact bytes that get signed — the four values, fixed order, nothing else.

    ⚠ The `sig` line is NOT part of what is signed, and neither is the file's layout.
    Both other implementations rebuild these same bytes from what they parsed rather
    than from what they received, which is what lets three parsers agree on the
    message without a shared byte-for-byte copy of it.
    """
    return (
        f"version: {version}\n"
        f"size: {size}\n"
        f"sha256: {sha256}\n"
        f"file: {file}\n"
    ).encode()


def render_manifest(version: int, size: int, sha256: str, file: str, sig: str) -> str:
    """The five required keys, `": "` separated. Nothing computed, nothing optional.

    `sig` goes last for readability only — the parsers do not care about order, and
    the signature is over the reconstruction, not over these bytes.
    """
    return canonical_message(version, size, sha256, file).decode() + f"sig: {sig}\n"


def load_key(path: Path) -> Ed25519PrivateKey:
    """Load the offline signing key: 32 raw bytes, nothing else.

    Raw rather than PEM deliberately. A PEM file invites a passphrase, a passphrase
    invites a prompt, and a prompt in a publish script invites someone to remove the
    passphrase. 32 bytes in a 0600 file is the honest version of what this is.
    """
    path = Path(path).expanduser()
    if not path.is_file():
        raise FileNotFoundError(
            f"{path} — generate one with --gen-key, and keep it out of this repo"
        )
    raw = path.read_bytes()
    if len(raw) != 32:
        raise ValueError(
            f"{path} is {len(raw)} bytes; an Ed25519 private key is exactly 32"
        )
    mode = stat.S_IMODE(path.stat().st_mode)
    if mode & 0o077:
        # A warning rather than a refusal: it is the operator's machine and their
        # call, but a key readable by anyone else has stopped being a private key.
        print(f"⚠ {path} is mode {mode:04o} — readable beyond its owner", file=sys.stderr)
    return Ed25519PrivateKey.from_private_bytes(raw)


def gen_key(path: Path) -> str:
    """Write a new 32-byte private key at 0600. Returns the public key as hex.

    Refuses to overwrite. A signing key that gets clobbered is a fleet that cannot be
    updated — every deployed node carries the old public key, and the only way to give
    one a new key is a cable.
    """
    path = Path(path).expanduser()
    if path.exists():
        raise FileExistsError(
            f"{path} already exists. Overwriting a signing key strands every node that "
            "carries its public half — they can only be re-keyed over USB."
        )
    path.parent.mkdir(parents=True, exist_ok=True)

    key = Ed25519PrivateKey.generate()
    raw = key.private_bytes(Encoding.Raw, PrivateFormat.Raw, NoEncryption())

    # Created 0600 from the start rather than written and then chmod'ed — the window
    # between those two is short and entirely avoidable.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        os.write(fd, raw)
    finally:
        os.close(fd)

    return key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex()


def publish(bin_path: Path, version: int, directory: Path, key_path: Path,
            *, name: str | None = None) -> Path:
    """Copy the image in, then atomically swap the manifest. Returns the manifest path.

    Raises before touching anything if the inputs are wrong — a bad publish should fail
    here, where a person is watching, rather than on a node that has no way to complain.
    """
    if not 0 <= version <= 0xFFFF:
        raise ValueError(f"version {version} does not fit u16 (fw_version is u16 on the wire)")
    if not bin_path.is_file():
        raise FileNotFoundError(bin_path)

    # Loaded FIRST, before the image is copied. A missing or malformed key should fail
    # with nothing written, not after a 900 KB copy has already landed in the served
    # directory under a name the previous manifest does not mention.
    key = load_key(key_path)

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

    # 2. The signature, over the four values that are about to be published. Computed
    #    before the rename, so the manifest is signed at the instant it becomes visible
    #    and there is no window in which an unsigned one is reachable.
    sig = key.sign(canonical_message(version, size, digest, file)).hex()

    # 3. The manifest, last, atomically. This is the moment the release becomes visible.
    manifest = directory / MANIFEST_NAME
    tmp_manifest = directory / (MANIFEST_NAME + ".part")
    tmp_manifest.write_text(render_manifest(version, size, digest, file, sig))
    os.replace(tmp_manifest, manifest)

    return manifest


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--gen-key", type=Path, default=None, metavar="PATH",
                   help="generate a new offline signing key and exit; prints the public "
                        "key to paste into firmware/platformio.ini")
    p.add_argument("--bin", type=Path, help="path to firmware.bin")
    p.add_argument("--version", type=int,
                   help="must match kFwVersion in src/esp32/main.cpp for this build")
    p.add_argument("--dir", type=Path, help="directory the HTTP server serves")
    p.add_argument("--key", type=Path, help="path to the offline Ed25519 signing key")
    p.add_argument("--name", default=None, help="image filename (default soundings-node-<v>.bin)")
    args = p.parse_args()

    if args.gen_key is not None:
        pub = gen_key(args.gen_key)
        print(f"private key -> {Path(args.gen_key).expanduser()}  (mode 0600, keep it offline)")
        print(f"public key  -> {pub}")
        print(
            "\nPaste that public key into firmware/platformio.ini as SOUNDINGS_OTA_PUBKEY, "
            "then flash every node that should trust it.\n"
            "⚠ There is no way to give a deployed node a new key except over USB. Back the "
            "private key up somewhere you would still have it after this machine dies."
        )
        return 0

    # Required only for publishing, which is why they are not argparse-required: the
    # --gen-key path legitimately has none of them.
    missing = [n for n in ("bin", "version", "dir", "key") if getattr(args, n) is None]
    if missing:
        p.error("missing required argument(s): " + ", ".join("--" + m for m in missing))

    manifest = publish(args.bin, args.version, args.dir, args.key, name=args.name)
    print(manifest.read_text(), end="")
    print(f"-> {manifest}")
    print(
        "\n⚠ The node compares fw_version for INEQUALITY. If this version matches what "
        "the nodes already report, nothing will be flagged — check kFwVersion was bumped."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
