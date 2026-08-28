# Dev Reference

Reference material pulled out of the always-loaded `CLAUDE.md` shell (DEC-S031) — read when you need it, not every session.

## `<VersionTag />` component — **N/A**

The shell's §Versioning points here for the `<VersionTag />` wiring. Soundings has no web UI: no Next.js app, no Vercel deploy, nothing rendering a version string to a browser. The seeds template's VersionTag section is webapp-shaped and deliberately not reproduced.

**Where the version actually surfaces here:** `package.json` `version`, bumped by `/retro` and `/bump-major`, and `fw_version:u16` in the packet header (DEC-003). Those are two different numbers on purpose — the packet field is the *wire contract's* view of a build, and a node in the field may be running an older one than the repo's HEAD. **When they disagree, the packet is the truth about what is on the device.**

## Phone PR review

The GitHub app collapses large diffs and hides the test plan below the fold. The `### Verify by hand` block is the part worth reading first, so `/kill-this` puts the review-pass receipt and the setup block near the top of the body rather than after the file list.

## CHANGELOG format

Written by `/retro` (patch + minor) and `/bump-major`. Keep-a-Changelog shape: `## [X.Y.Z] — YYYY-MM-DD` with `### Added / Changed / Fixed / Removed`. One entry per release, newest first.

## PlatformIO `extra_configs` — merges sections key-by-key (verified 2026-08-24)

firmware/node\_secret.ini (untracked — written without backticks on purpose, since the
context checker reads a backticked path as a claim that it resolves) supplies WiFi
credentials and the firmware-server
address; `firmware/platformio.ini` carries empty `[wifi]` / `[ota]` defaults and
`extra_configs = node_secret*.ini`.

**The question was whether a redefined section MERGES or REPLACES**, because a replace
would silently drop any key the secret file omitted. Settled by three builds rather than
by reading docs:

| Secret file | `ota.host` | `ota.path` | Build |
|---|---|---|---|
| present, complete | `192.168.50.201` | `/firmware/` | ok |
| **absent** | `""` | `""` | **ok** — empty defaults survive |
| present, `path` omitted | `192.168.50.201` | `""` | ok — **the default filled the gap** |

**It merges.** A partial secret file is safe, and a missing file compiles to empty strings
rather than failing.

⚠ **Which makes empty values the thing to guard, not build errors.** An empty `path`
builds fine and produces `http://host:8080manifest.txt` — a broken URL from a clean build.
The node must treat an empty ssid or host as *"not configured, never attempt an update"*
and validate that `path` begins and ends with `/`. The failure this design avoids is a
build that stops; the failure it creates is a build that succeeds and is wrong.

Test it the same way if the pattern is reused: `pio run -e node -v` and grep the compiler
line for the `-D` flags. ⚠ That output contains the WiFi password — redact before pasting.
