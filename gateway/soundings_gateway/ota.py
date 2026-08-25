"""Firmware manifest + update policy — the daemon's half of the OTA trigger.

`contracts/firmware-manifest-v1.md` is the format; the node has an independent C++
parser (`firmware/src/core/fw_manifest.cpp`) graded against the same literal, which
is the arrangement packet-v1 and downlink-v1 already use.

**The manifest is the trigger, not the `.bin`.** Writing an image is not atomic, so
the image is written first under its own name and the manifest naming it is written
last, atomically. Until that rename lands the new firmware is invisible.

⚠ **This module refuses to flag a node for an image it can already tell is broken.**
The node's WiFi radio is the most expensive thing it owns — a wake spent joining a
network to fetch a truncated file is a wake spent for a result knowable here for
free. The node re-verifies the hash while streaming anyway (different failure: this
checks the file on disk, the node checks what arrived over WiFi), and `Update.end()`
is the third line of defence.

stdlib only, per the project's stdlib-first convention.
"""
from __future__ import annotations

import hashlib
import logging
from dataclasses import dataclass
from pathlib import Path

from .downlink import FLAGS_NONE, FLAG_UPDATE_WAITING

log = logging.getLogger(__name__)

__all__ = [
    "FwManifest", "parse_manifest", "validate_image", "UpdatePolicy",
    "MANIFEST_NAME", "MAX_MANIFEST_BYTES", "MAX_MANIFEST_LINES",
]

#: Fixed name at the base URL. The node asks for one known address every time; the
#: only thing that changes is what sits behind it. Discovery would mean a node
#: deciding at runtime which file to trust.
MANIFEST_NAME = "manifest.txt"

#: Mirrors `kMaxManifestBytes` / `kMaxManifestLines` in `fw_manifest.h`. The node
#: reads this over HTTP before it can trust anything about it, and it has 320 KB of
#: RAM and no way to report that it died.
MAX_MANIFEST_BYTES = 512
MAX_MANIFEST_LINES = 32

_SEPARATOR = ": "
_HEX = set("0123456789abcdef")
_REQUIRED = ("version", "size", "sha256", "file")


@dataclass(frozen=True)
class FwManifest:
    version: int      # u16, matches fw_version on the wire
    size: int         # image length in bytes
    sha256: str       # 64 lowercase hex chars
    file: str         # bare filename, resolved against the base URL


def _valid_filename(name: str) -> bool:
    """A bare filename: no separators, no traversal, no scheme.

    `file` resolves against a fixed base URL, so a value that can express a path is a
    value that can point a field node at an arbitrary address — and it has no rollback.
    """
    if not name or len(name) > 63:
        return False
    if name.startswith("."):
        return False
    return all(0x21 <= ord(c) <= 0x7E and c not in "/\\:" for c in name)


def parse_manifest(text: str) -> FwManifest | None:
    """Parse manifest text. Returns None — silently — if any contract rule is broken.

    Rejection is total; there is no partially-applied manifest and no error type. A
    manifest the daemon cannot read leads to no flag, which is indistinguishable from
    nothing being available, and that is the correct outcome for both.
    """
    if not text or len(text.encode()) > MAX_MANIFEST_BYTES:
        return None

    lines = text.splitlines()
    if len(lines) > MAX_MANIFEST_LINES:
        return None

    found: dict[str, str] = {}
    for line in lines:
        line = line.rstrip("\r")
        if not line or line.startswith("#"):
            continue
        # Split on ": " — a colon AND a space. Not split-on-first-colon: a future value
        # could legitimately contain a colon and a guessing parser would mangle it.
        if _SEPARATOR not in line:
            continue   # malformed lines are skipped; a required key it meant to be is
                       # now missing, and that IS fatal below
        key, _, value = line.partition(_SEPARATOR)
        if key in _REQUIRED:
            found[key] = value.rstrip(" ")
        # Unknown keys ignored — this format's only forward compatibility, and one-way:
        # a v2 key carrying a requirement would be silently skipped, so anything
        # mandatory needs a version bump rather than a new key.

    if any(k not in found for k in _REQUIRED):
        return None

    # `str.isdigit()` rather than int(): it rejects "-1", "+1", whitespace and unicode
    # digits, all of which int() would happily take or mis-take.
    if not found["version"].isdigit() or not found["size"].isdigit():
        return None
    version, size = int(found["version"]), int(found["size"])
    if not 0 <= version <= 0xFFFF or size <= 0:
        return None

    sha = found["sha256"]
    # Lowercase only. Accepting both cases would be two implementations agreeing to
    # differ on the one field where "close enough" means flashing an unchecked image.
    if len(sha) != 64 or not set(sha) <= _HEX:
        return None

    if not _valid_filename(found["file"]):
        return None

    return FwManifest(version=version, size=size, sha256=sha, file=found["file"])


def validate_image(manifest: FwManifest | None, directory: Path) -> bool:
    """Does the image the manifest names actually exist, at that length, with that hash?

    Size alone is not integrity, which is why the manifest carries both: a truncated
    copy usually fails the length check first and cheaply, and anything that passes it
    still has to match the hash.
    """
    if manifest is None:
        return False

    path = Path(directory) / manifest.file
    try:
        if not path.is_file() or path.stat().st_size != manifest.size:
            return False
        digest = hashlib.sha256()
        with path.open("rb") as fh:
            # Chunked: an image is ~320 KB today, but a reader that slurps is a reader
            # that breaks quietly the first time one isn't.
            for chunk in iter(lambda: fh.read(65536), b""):
                digest.update(chunk)
    except OSError:
        log.exception("could not read firmware image %s", path)
        return False

    return digest.hexdigest() == manifest.sha256


class UpdatePolicy:
    """Decides the downlink flags for one reading.

    Held by the gateway's `respond=` hook. Everything it needs is a pure function of
    the reading plus what is on disk, so the interesting behaviour is testable without
    a radio, a node, or a broker.
    """

    def __init__(self, directory: Path):
        self.directory = Path(directory)

    def flags_for(self, reading: dict) -> int:
        """Flags to send this node, or FLAGS_NONE for silence.

        ⚠ Re-reads the manifest on every call, deliberately. Publishing firmware must
        not require restarting the daemon — the manifest IS the trigger, and a cached
        one is a trigger that never fires. It is a 512-byte read once per node per
        fifteen minutes.

        Never raises. It runs inside the gateway's decode loop, and a bad file on disk
        must not stop the daemon from decoding telemetry.
        """
        try:
            reported = reading.get("fw_version")
            if not isinstance(reported, int):
                return FLAGS_NONE

            path = self.directory / MANIFEST_NAME
            if not path.is_file():
                return FLAGS_NONE

            manifest = parse_manifest(path.read_text(errors="replace"))
            if manifest is None:
                log.warning("manifest at %s did not parse; flagging nothing", path)
                return FLAGS_NONE

            # Inequality, not ordering. There is no rollback mechanism (operator call,
            # 2026-08-23), so republishing an older manifest is the only downgrade path
            # that exists and it has to work.
            if manifest.version == reported:
                return FLAGS_NONE

            if not validate_image(manifest, self.directory):
                log.warning(
                    "manifest names version %d but %s does not match its size/hash — "
                    "not flagging node %s",
                    manifest.version, manifest.file, reading.get("node_id"),
                )
                return FLAGS_NONE

            return FLAG_UPDATE_WAITING
        except Exception:  # noqa: BLE001 — a bad file must not stop the decode loop
            log.exception("update policy failed for node %s", reading.get("node_id"))
            return FLAGS_NONE
