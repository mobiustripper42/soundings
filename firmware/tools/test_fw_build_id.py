"""Host tests for fw_build_id.py's pure helpers. Run: python3 tools/test_fw_build_id.py

Not a pio test: this is build tooling, not firmware, so it is graded here rather than in
the Unity suite. The SCons wiring at the bottom of the module is import-safe by design,
which is what lets this file import it at all.
"""
import os
import sys
import tempfile
import unittest
from datetime import datetime, timezone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fw_build_id as fb


class TestBuildId(unittest.TestCase):
    def test_identity_includes_sha_and_timestamp(self):
        dt = datetime(2026, 8, 25, 3, 4, 5, tzinfo=timezone.utc)
        self.assertEqual(fb.compute_build_id("abc1234", False, dt), "abc1234-260825-030405")

    def test_dirty_is_marked(self):
        dt = datetime(2026, 8, 25, 3, 4, 5, tzinfo=timezone.utc)
        self.assertEqual(fb.compute_build_id("abc1234", True, dt), "abc1234-dirty-260825-030405")

    def test_two_builds_a_second_apart_do_not_collide(self):
        """The whole reason the timestamp exists: the same sha, rebuilt, must produce a
        different identity — otherwise an OTA that took looks like one that didn't."""
        a = fb.compute_build_id("abc1234", False, datetime(2026, 8, 25, 3, 4, 5, tzinfo=timezone.utc))
        b = fb.compute_build_id("abc1234", False, datetime(2026, 8, 25, 3, 4, 6, tzinfo=timezone.utc))
        self.assertNotEqual(a, b)


class TestAppSizeGate(unittest.TestCase):
    def _bin(self, n):
        fd, path = tempfile.mkstemp(suffix=".bin")
        with os.fdopen(fd, "wb") as fh:
            fh.write(b"\0" * n)
        self.addCleanup(os.unlink, path)
        return path

    def test_an_image_over_the_slot_fails_the_build(self):
        with self.assertRaises(SystemExit):
            fb.check_app_size(self._bin(fb.APP_SLOT_BYTES + 1))

    def test_an_image_at_exactly_the_slot_size_passes(self):
        """The boundary from the legal side. Without this, the test above passes against
        a gate that rejects every image."""
        self.assertEqual(fb.check_app_size(self._bin(fb.APP_SLOT_BYTES)), fb.APP_SLOT_BYTES)

    def test_the_slot_matches_the_boards_partition_table(self):
        """0x330000 is app0/app1 in default_8MB.csv. tinkle's value is 0x140000 (4 MB
        board) and copying it unchanged would have under-reported the budget by 2.6x."""
        self.assertEqual(fb.APP_SLOT_BYTES, 0x330000)
        self.assertEqual(fb.APP_SLOT_BYTES, 3342336)


class TestArchive(unittest.TestCase):
    def test_archive_names_the_file_by_env_and_build_id(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "firmware.bin")
            with open(src, "wb") as fh:
                fh.write(b"image")
            dest = fb.archive_firmware(src, os.path.join(d, "archive"), "node", "abc-260825-030405")
            self.assertTrue(dest.endswith("soundings-node-abc-260825-030405.bin"))
            with open(dest, "rb") as fh:
                self.assertEqual(fh.read(), b"image")


if __name__ == "__main__":
    unittest.main(verbosity=2)
