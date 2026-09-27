#!/usr/bin/env python3
"""Verify tracked license copies and self-contained source distributions.

Run without arguments in a complete, initialized checkout to verify notices
against the pinned PAWN submodule. --archive PATH checks an existing ZIP or tar
archive against the tracked license copies, without consulting the submodule.
--git-ref REF builds an in-memory git archive and checks its public source tree.
Requires Python 3.11+ and its standard library; git is needed except in archive
mode. These checks verify packaging, not legal compatibility or authorship.
"""

import argparse
from hashlib import sha256
from io import BytesIO
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
from zipfile import BadZipFile, ZipFile, is_zipfile


ROOT = Path(__file__).resolve().parents[1]
FLOAT = "src/main/resources/ccpawn-native/include/float.inc"
LICENSE_FILES = (
    "LICENSE",
    "TEMPLATE_LICENSE.txt",
    "licenses/pawn-LICENSE.txt",
    "licenses/pawn-NOTICE.txt",
    "licenses/ISC-strlcpy-strlcat.txt",
    "licenses/LGPL-2.1.txt",
    "licenses/glibc-NOTICE.txt",
)
REQUIRED = (*LICENSE_FILES, "licenses/README.md", FLOAT)
UPSTREAM_COPIES = {
    "licenses/pawn-LICENSE.txt": "third_party/pawn/LICENSE",
    "licenses/pawn-NOTICE.txt": "third_party/pawn/NOTICE",
}
FLOAT_REFERENCES = ("licenses/pawn-LICENSE.txt", "licenses/pawn-NOTICE.txt")
# Complete GNU C Library COPYING.LIB snapshot documented in licenses/README.md.
LGPL_SHA256 = "20e50fe7aae3e56378ebf0417d9de904f55a0e61e4df315333e632a4d3555d95"
MAX_NOTICE_BYTES = 2 * 1024 * 1024


def read_required(root):
    contents, errors = {}, []
    for name in REQUIRED:
        try:
            contents[name] = (root / name).read_bytes()
        except OSError as error:
            errors.append(f"Cannot read required source file {name}: {error}")
    return contents, errors


def validate_contents(contents, expected=None):
    """Validate required payloads; expected contains trusted local license copies."""
    errors = []
    for name in REQUIRED:
        if name not in contents:
            errors.append("Missing required source entry: " + name)
        elif not contents[name].strip():
            errors.append("Empty required source entry: " + name)
    if expected is not None:
        for name in LICENSE_FILES:
            if name not in expected:
                errors.append("Missing reference license/notice: " + name)
            elif name in contents and contents[name] != expected[name]:
                errors.append("License/notice differs from tracked copy: " + name)
    if "licenses/LGPL-2.1.txt" in contents:
        if sha256(contents["licenses/LGPL-2.1.txt"]).hexdigest() != LGPL_SHA256:
            errors.append("LGPL text differs from the audited complete COPYING.LIB snapshot")
    if FLOAT in contents:
        try:
            header = "\n".join(contents[FLOAT].decode("utf-8").splitlines()[:20])
            for reference in FLOAT_REFERENCES:
                if reference not in header:
                    errors.append("Float header lacks source-distribution notice reference: " + reference)
            if "Local changes:" not in header:
                errors.append("Float header lacks its local modification notice")
        except UnicodeError:
            errors.append("Float include is not UTF-8 text")
    return errors


def isc_notice(source):
    """Extract the identical strlcpy/strlcat notice, removing only C decoration."""
    notices = []
    for comment in re.findall(r"/\*(.*?)\*/", source, re.DOTALL):
        if "Copyright (c) 1998 Todd C. Miller" not in comment:
            continue
        lines = [re.sub(r"^\s*\* ?", "", line).rstrip()
                 for line in comment.splitlines()]
        notices.append(("\n".join(lines).strip() + "\n").encode("utf-8"))
    if len(notices) != 2 or notices[0] != notices[1]:
        raise ValueError("expected two identical Todd C. Miller notices in compiler/lstring.c")
    return notices[0]


def check_checkout(root=ROOT):
    contents, errors = read_required(root)
    errors.extend(validate_contents(contents))
    for local, upstream in UPSTREAM_COPIES.items():
        try:
            original = (root / upstream).read_bytes()
        except OSError as error:
            errors.append(f"Cannot read upstream {upstream}; initialize the pinned submodule: {error}")
            continue
        if local in contents and contents[local] != original:
            errors.append("License/notice differs from pinned PAWN source: " + local)
    source = root / "third_party/pawn/compiler/lstring.c"
    try:
        original_isc = isc_notice(source.read_text(encoding="utf-8"))
        if contents.get("licenses/ISC-strlcpy-strlcat.txt") != original_isc:
            errors.append("ISC notice differs from compiler/lstring.c")
    except (OSError, UnicodeError, ValueError) as error:
        errors.append(f"Cannot verify upstream ISC notice: {error}")
    return errors


def check_submodule_revision(root=ROOT):
    try:
        expected = subprocess.run(
            ["git", "rev-parse", "HEAD:third_party/pawn"], cwd=root,
            capture_output=True, text=True, check=True, timeout=15).stdout.strip()
        actual = subprocess.run(
            ["git", "-C", "third_party/pawn", "rev-parse", "HEAD"], cwd=root,
            capture_output=True, text=True, check=True, timeout=15).stdout.strip()
    except (OSError, subprocess.SubprocessError) as error:
        return [f"Cannot verify pinned PAWN revision: {error}"]
    return [] if expected == actual else [
        f"PAWN submodule revision differs from HEAD gitlink: expected {expected}, got {actual}"]


def archive_contents(records):
    """Read required regular files only, without extracting paths to disk.

    records: (name, is_regular_file, byte_size, zero-argument byte reader).
    Both git's flat archives and GitHub's single enclosing directory work.
    """
    errors, names = [], {}
    for name, regular, size, reader in records:
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or "\\" in name:
            errors.append("Invalid archive entry path: " + name)
            continue
        normal = str(path)
        names.setdefault(normal, []).append((regular, size, reader))
    prefixes = {""}
    for name in names:
        for required in REQUIRED:
            if name.endswith("/" + required):
                prefixes.add(name[:-len(required)])
    scores = {prefix: sum(prefix + name in names for name in REQUIRED)
              for prefix in prefixes}
    best = max(scores.values())
    candidates = [prefix for prefix, score in scores.items() if score == best]
    if len(candidates) != 1:
        return {}, errors + ["Ambiguous source archive root"]
    prefix = candidates[0]
    contents = {}
    for name in REQUIRED:
        entries = names.get(prefix + name, [])
        if not entries:
            continue
        if len(entries) != 1:
            errors.append("Duplicate required source entry: " + name)
            continue
        regular, size, reader = entries[0]
        if not regular:
            errors.append("Required source entry is not a regular file: " + name)
        elif size > MAX_NOTICE_BYTES:
            errors.append("Required source entry exceeds size limit: " + name)
        else:
            contents[name] = reader()
    return contents, errors


def check_tar(archive, expected):
    records = [(member.name, member.isfile(), member.size,
                lambda member=member: archive.extractfile(member).read(MAX_NOTICE_BYTES + 1))
               for member in archive.getmembers()]
    contents, errors = archive_contents(records)
    return errors + validate_contents(contents, expected)


def check_archive(path, root=ROOT):
    expected, errors = read_required(root)
    if errors:
        return errors
    if is_zipfile(path):
        with ZipFile(path) as archive:
            records = [(entry.filename, not entry.is_dir()
                        and (entry.external_attr >> 16) & 0o170000 != 0o120000,
                        entry.file_size, lambda entry=entry: archive.read(entry))
                       for entry in archive.infolist()]
            contents, archive_errors = archive_contents(records)
            return archive_errors + validate_contents(contents, expected)
    with tarfile.open(path, "r:*") as archive:
        return check_tar(archive, expected)


def check_git_ref(ref, root=ROOT):
    if not ref or ref.startswith("-"):
        return ["Git reference must be a nonempty revision, not an option"]
    expected, errors = read_required(root)
    if errors:
        return errors
    result = subprocess.run(["git", "archive", "--format=tar", ref], cwd=root,
                            capture_output=True, check=True, timeout=30)
    with tarfile.open(fileobj=BytesIO(result.stdout), mode="r:") as archive:
        return check_tar(archive, expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--archive", type=Path, help="check an existing source ZIP/tar archive")
    mode.add_argument("--git-ref", help="check a git archive of this revision, for example HEAD")
    args = parser.parse_args()
    try:
        if args.archive:
            errors = check_archive(args.archive)
            label = str(args.archive)
        elif args.git_ref is not None:
            errors = check_git_ref(args.git_ref)
            label = "git archive " + args.git_ref
        else:
            errors = check_checkout() + check_submodule_revision()
            label = "working tree and pinned PAWN sources"
    except (OSError, ValueError, UnicodeError, tarfile.TarError, BadZipFile,
            RuntimeError, subprocess.SubprocessError) as error:
        errors, label = [str(error)], "source licensing"
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print(f"PASS: {label}: {len(REQUIRED)} required source files, complete notices, "
          "adapted Float references")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
