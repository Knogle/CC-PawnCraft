#!/usr/bin/env python3
"""Exercise the JAR checker with local synthetic archives, not a Minecraft server.

Run: python3 scripts/check_pawncraft_jar_test.py
Uses Python 3.11+ and its standard library only.
"""

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from zipfile import ZipFile

from check_pawncraft_jar import LICENSES, METADATA, REQUIRED, check_jar


VERSION = "0.4.0-test"


class JarLicenseTests(unittest.TestCase):
    def setUp(self):
        self.directory = TemporaryDirectory(prefix="pawncraft-jar-check-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "fixture.jar"
        self.contents = {name: b"fixture" for name in REQUIRED}
        self.contents.update({name: source.read_bytes()
                              for name, source in LICENSES.items()})
        self.contents[METADATA] = (
            'modLoader = "javafml"\nloaderVersion = "[4,)"\nlicense = "MIT"\n'
            '[[mods]]\nmodId = "ccpawn"\ndisplayName = "PawnCraft"\n'
            f'version = "{VERSION}"\n'
        ).encode("utf-8")

    def check(self, contents=None):
        with ZipFile(self.path, "w") as archive:
            for name, value in (self.contents if contents is None else contents).items():
                archive.writestr(name, value)
        return check_jar(self.path, VERSION)

    def test_valid_archive(self):
        self.assertEqual([], self.check())

    def test_each_license_or_notice_is_required(self):
        for name in LICENSES:
            with self.subTest(entry=name):
                contents = dict(self.contents)
                del contents[name]
                self.assertIn("Missing required entries: " + name, self.check(contents))

    def test_each_license_or_notice_must_match_source(self):
        for name, source in LICENSES.items():
            with self.subTest(entry=name):
                contents = dict(self.contents)
                contents[name] += b"\nUnexpected change\n"
                self.assertTrue(any(error.startswith("License/notice differs from ")
                                    and error.endswith(": " + name)
                                    for error in self.check(contents)))

    def test_empty_license_is_rejected(self):
        name = next(iter(LICENSES))
        self.contents[name] = b""
        errors = self.check()
        self.assertIn("Empty required entries: " + name, errors)
        self.assertTrue(any(error.startswith("License/notice differs from ")
                            for error in errors))

    def test_incorrect_top_level_license_is_rejected(self):
        self.contents[METADATA] = self.contents[METADATA].replace(
            b'license = "MIT"', b'license = "All Rights Reserved"')
        self.assertIn("Metadata license: expected 'MIT', got 'All Rights Reserved'",
                      self.check())

    def test_missing_top_level_license_is_rejected(self):
        self.contents[METADATA] = self.contents[METADATA].replace(
            b'license = "MIT"\n', b"")
        self.assertIn("Metadata license: expected 'MIT', got None", self.check())

    def test_license_in_mod_entry_does_not_replace_top_level_license(self):
        self.contents[METADATA] = self.contents[METADATA].replace(
            b'license = "MIT"\n', b"") + b'license = "MIT"\n'
        self.assertIn("Metadata license: expected 'MIT', got None", self.check())


if __name__ == "__main__":
    unittest.main(verbosity=2)
