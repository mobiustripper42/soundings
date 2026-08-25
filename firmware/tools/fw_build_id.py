# fw_build_id.py — build identity, per-build archive, and OTA slot-size guard (issue #79).
#
# PlatformIO extra_script (pre). Ported from tinkle's tools/fw_build_id.py, which has run
# on every build there since its #126/#159. Three jobs:
#
# 1. Inject FW_BUILD as a string macro: "<sha>[-dirty]-<UTC yymmdd-HHMMSS>".
#
#    ⚠ THE TIMESTAMP IS THE LOAD-BEARING PART, and tinkle learned it the hard way: the git
#    short-sha alone is unchanged across reflashes that differ only in a gitignored build
#    flag — here, node_secret.ini — so two genuinely different images showed an IDENTICAL
#    build string and an OTA looked like it had never taken. A per-build UTC timestamp
#    makes the identity change on every build.
#
#    This is NOT the same thing as kFwVersion. kFwVersion is a u16 on the wire and is what
#    the daemon compares against the manifest; FW_BUILD is a human-readable identity that
#    distinguishes two builds carrying the SAME version. A version collision is invisible
#    to the update mechanism (contracts/firmware-manifest-v1.md says so); FW_BUILD is how
#    a person tells them apart afterwards.
#
# 2. Archive each linked image to build_archive/soundings-<env>-<FW_BUILD>.bin (gitignored)
#    so every build leaves a distinct, sortable file. Clean up by hand whenever.
#
# 3. HARD GATE the linked image against the OTA app slot. The board runs the arduino-esp32
#    default_8MB.csv partition table — app0/app1 = 0x330000 (3,342,336 B) each, plus
#    otadata. It is ALREADY dual-slot; no repartition was needed. An image that outgrows a
#    slot would otherwise fail at flash time, or worse, be published and brick the OTA path
#    on a node in a tank. Fail here, loudly, instead.
#
# The pure helpers are import-safe and host-tested by tools/test_fw_build_id.py; the SCons
# wiring at the bottom runs only under PlatformIO.

import os
import shutil
import subprocess
from datetime import datetime, timezone

# app0/app1 in default_8MB.csv. ⚠ 0x140000 is tinkle's 4 MB figure — this board is 8 MB
# and the value is 2.6x larger. Confirmed against the partition CSV and against the
# linker report ("923872 bytes from 3342336").
APP_SLOT_BYTES = 0x330000


def compute_build_id(sha, dirty, dt):
    """'<sha>[-dirty]-<UTC yymmdd-HHMMSS>'. Second granularity, so two builds a minute
    apart never collide."""
    return "%s%s-%s" % (sha, "-dirty" if dirty else "", dt.strftime("%y%m%d-%H%M%S"))


def git_sha_and_dirty(project_dir):
    """(short-sha, dirty), or ('dev', False) outside a repo. `dirty` reflects any
    non-ignored working-tree change — a gitignored node_secret.ini never dirties it,
    which is exactly why the timestamp and not '-dirty' carries per-build uniqueness."""
    try:
        sha = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=project_dir, text=True
        ).strip()
    except Exception:
        return "dev", False
    try:
        dirty = bool(subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=project_dir, text=True
        ).strip())
    except Exception:
        dirty = False
    return sha, dirty


def archive_firmware(bin_path, archive_dir, env_name, build_id):
    """Copy the linked image to <archive_dir>/soundings-<env>-<build_id>.bin."""
    os.makedirs(archive_dir, exist_ok=True)
    dest = os.path.join(archive_dir, "soundings-%s-%s.bin" % (env_name, build_id))
    shutil.copy2(bin_path, dest)
    return dest


def check_app_size(bin_path):
    """Return the image size; raise SystemExit if it exceeds the OTA app slot."""
    size = os.path.getsize(bin_path)
    pct = 100.0 * size / APP_SLOT_BYTES
    print("fw_build_id: firmware.bin %d bytes — %.0f%% of the %d-byte OTA slot"
          % (size, pct, APP_SLOT_BYTES))
    if size > APP_SLOT_BYTES:
        raise SystemExit(
            "fw_build_id: firmware.bin is %d bytes — over the %d-byte app slot. OTA (and "
            "flash) would fail. Trim the image or revisit the partition table."
            % (size, APP_SLOT_BYTES)
        )
    return size


# --- SCons wiring (runs only under PlatformIO; import-safe for the host test) -------
try:
    Import("env")  # noqa: F821 — a SCons builtin, undefined on a plain import
    _under_scons = True
except NameError:
    _under_scons = False

if _under_scons:
    _sha, _dirty = git_sha_and_dirty(env["PROJECT_DIR"])  # noqa: F821
    _build_id = compute_build_id(_sha, _dirty, datetime.now(timezone.utc))
    env.Append(CPPDEFINES=[("FW_BUILD", env.StringifyMacro(_build_id))])  # noqa: F821

    def _post_build(source, target, env):  # noqa: F811
        path = target[0].get_abspath()
        check_app_size(path)  # the real gate — SystemExit on an over-slot image
        # Archiving is a convenience, not a gate: a copy failure (disk full, perms) must
        # NOT fail a build whose firmware.bin already linked and passed the size check.
        try:
            dest = archive_firmware(
                path, os.path.join(env["PROJECT_DIR"], "build_archive"),
                env["PIOENV"], _build_id,
            )
            print("fw_build_id: archived build -> %s" % dest)
        except Exception as exc:  # noqa: BLE001 — the build is already good
            print("fw_build_id: WARNING — archive copy failed (%s); build unaffected" % exc)

    env.AddPostAction("$BUILD_DIR/firmware.bin", _post_build)  # noqa: F821
