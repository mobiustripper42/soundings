# Dev Reference

Reference material pulled out of the always-loaded `CLAUDE.md` shell (DEC-S031) — read when you need it, not every session.

## `<VersionTag />` component — **N/A**

The shell's §Versioning points here for the `<VersionTag />` wiring. Soundings has no web UI: no Next.js app, no Vercel deploy, nothing rendering a version string to a browser. The seeds template's VersionTag section is webapp-shaped and deliberately not reproduced.

**Where the version actually surfaces here:** `package.json` `version`, bumped by `/retro` and `/bump-major`, and `fw_version:u16` in the packet header (DEC-003). Those are two different numbers on purpose — the packet field is the *wire contract's* view of a build, and a node in the field may be running an older one than the repo's HEAD. **When they disagree, the packet is the truth about what is on the device.**

## Phone PR review

The GitHub app collapses large diffs and hides the test plan below the fold. The `### Verify by hand` block is the part worth reading first, so `/kill-this` puts the review-pass receipt and the setup block near the top of the body rather than after the file list.

## CHANGELOG format

Written by `/retro` (patch + minor) and `/bump-major`. Keep-a-Changelog shape: `## [X.Y.Z] — YYYY-MM-DD` with `### Added / Changed / Fixed / Removed`. One entry per release, newest first.
