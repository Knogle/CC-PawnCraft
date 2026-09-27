#!/usr/bin/env python3
"""Local fixture tests for source/license checks. No server or network required.

Run: python3 scripts/check_license_sources_test.py
"""

from io import BytesIO
from pathlib import Path
import tarfile
from tempfile import TemporaryDirectory
import unittest
from zipfile import ZipFile, ZipInfo

from check_license_sources import (
    FLOAT, LICENSE_FILES, REQUIRED, ROOT, UPSTREAM_COPIES, archive_contents,
    check_archive, check_checkout, check_git_ref, isc_notice, validate_contents,
)


class SourceLicenseTests(unittest.TestCase):
    def setUp(self):
        self.directory = TemporaryDirectory(prefix="pawncraft-license-check-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name) / "source"
        self.contents = {name: (ROOT / name).read_bytes() for name in REQUIRED}
        for name, content in self.contents.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.path = Path(self.directory.name) / "source.zip"

    def zip_check(self, contents=None, prefix=""):
        with ZipFile(self.path, "w") as archive:
            for name, content in (self.contents if contents is None else contents).items():
                archive.writestr(prefix + name, content)
        return check_archive(self.path, self.root)

    def add_upstream_fixture(self):
        for local, upstream in UPSTREAM_COPIES.items():
            path = self.root / upstream
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.contents[local])
        path = self.root / "third_party/pawn/compiler/lstring.c"
        path.parent.mkdir(parents=True, exist_ok=True)
        notice = self.contents["licenses/ISC-strlcpy-strlcat.txt"].decode("utf-8")
        comment = "/*\n" + "\n".join(" * " + line for line in notice.splitlines()) + "\n */\n"
        path.write_text(comment + "\n" + comment, encoding="utf-8")

    def test_flat_zip_passes_without_initialized_submodule(self):
        self.assertFalse((self.root / "third_party").exists())
        self.assertEqual([], self.zip_check())

    def test_github_directory_prefix_passes(self):
        self.assertEqual([], self.zip_check(prefix="CC-PawnCraft-main/"))

    def test_tar_gz_passes_without_initialized_submodule(self):
        path = Path(self.directory.name) / "source.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            for name, content in self.contents.items():
                member = tarfile.TarInfo("CC-PawnCraft-main/" + name)
                member.size = len(content)
                archive.addfile(member, BytesIO(content))
        self.assertEqual([], check_archive(path, self.root))

    def test_every_required_file_is_required(self):
        for name in REQUIRED:
            with self.subTest(entry=name):
                contents = dict(self.contents)
                del contents[name]
                self.assertIn("Missing required source entry: " + name,
                              self.zip_check(contents))

    def test_every_license_and_notice_must_match(self):
        for name in LICENSE_FILES:
            with self.subTest(entry=name):
                contents = dict(self.contents)
                contents[name] += b"\nModified notice\n"
                self.assertIn("License/notice differs from tracked copy: " + name,
                              self.zip_check(contents))

    def test_empty_notice_is_rejected(self):
        self.contents["licenses/pawn-NOTICE.txt"] = b"\n"
        self.assertIn("Empty required source entry: licenses/pawn-NOTICE.txt", self.zip_check())

    def test_old_submodule_only_float_links_fail(self):
        self.contents[FLOAT] = self.contents[FLOAT].replace(
            b"licenses/pawn-LICENSE.txt", b"third_party/pawn/LICENSE").replace(
            b"licenses/pawn-NOTICE.txt", b"third_party/pawn/NOTICE")
        errors = self.zip_check()
        self.assertEqual(2, sum("Float header lacks source-distribution" in error
                                for error in errors))

    def test_missing_modification_notice_fails(self):
        self.contents[FLOAT] = self.contents[FLOAT].replace(b"Local changes:", b"Description:")
        self.assertIn("Float header lacks its local modification notice", self.zip_check())

    def test_missing_local_baseline_does_not_skip_comparison(self):
        (self.root / "licenses/pawn-NOTICE.txt").unlink()
        self.assertTrue(any("Cannot read required source file licenses/pawn-NOTICE.txt" in error
                            for error in self.zip_check()))

    def test_pinned_upstream_notice_copies_pass(self):
        self.add_upstream_fixture()
        self.assertEqual([], check_checkout(self.root))

    def test_default_checkout_requires_initialized_sources(self):
        errors = check_checkout(self.root)
        self.assertTrue(any("initialize the pinned submodule" in error for error in errors))
        self.assertTrue(any("Cannot verify upstream ISC notice" in error for error in errors))

    def test_modified_pawn_copy_fails_checkout(self):
        self.add_upstream_fixture()
        for name in UPSTREAM_COPIES:
            with self.subTest(entry=name):
                path = self.root / name
                path.write_bytes(self.contents[name] + b"changed")
                self.assertIn("License/notice differs from pinned PAWN source: " + name,
                              check_checkout(self.root))
                path.write_bytes(self.contents[name])

    def test_isc_notice_must_match_upstream(self):
        self.add_upstream_fixture()
        (self.root / "licenses/ISC-strlcpy-strlcat.txt").write_bytes(b"short notice")
        self.assertIn("ISC notice differs from compiler/lstring.c", check_checkout(self.root))

    def test_isc_extraction_rejects_missing_or_differing_blocks(self):
        with self.assertRaises(ValueError):
            isc_notice("/* unrelated */")
        source = "/*\n * Copyright (c) 1998 Todd C. Miller\n */\n"
        with self.assertRaises(ValueError):
            isc_notice(source)
        with self.assertRaises(ValueError):
            isc_notice(source + source.replace("Miller", "Miller and another author"))

    def test_incomplete_lgpl_snapshot_is_rejected(self):
        self.contents["licenses/LGPL-2.1.txt"] = b"GNU LESSER GENERAL PUBLIC LICENSE"
        self.assertIn("LGPL text differs from the audited complete COPYING.LIB snapshot",
                      validate_contents(self.contents))

    def test_duplicate_required_archive_entry_fails(self):
        payload = self.contents["LICENSE"]
        contents, errors = archive_contents([
            ("LICENSE", True, len(payload), lambda: payload),
            ("LICENSE", True, len(payload), lambda: payload),
        ])
        self.assertIn("Duplicate required source entry: LICENSE", errors)
        self.assertNotIn("LICENSE", contents)

    def test_required_symlink_is_rejected(self):
        with ZipFile(self.path, "w") as archive:
            for name, content in self.contents.items():
                if name == "licenses/pawn-NOTICE.txt":
                    entry = ZipInfo(name)
                    entry.create_system = 3
                    entry.external_attr = 0o120777 << 16
                    archive.writestr(entry, "../../third_party/pawn/NOTICE")
                else:
                    archive.writestr(name, content)
        self.assertIn("Required source entry is not a regular file: licenses/pawn-NOTICE.txt",
                      check_archive(self.path, self.root))

    def test_option_like_git_reference_is_rejected(self):
        self.assertIn("Git reference must be a nonempty revision, not an option",
                      check_git_ref("--output=unexpected", self.root))


if __name__ == "__main__":
    unittest.main(verbosity=2)
