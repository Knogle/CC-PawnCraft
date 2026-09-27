#!/usr/bin/env python3
"""Check that the built PawnCraft JAR contains PAWN, but no guest Java runtime.

Usage: python3 scripts/check_pawncraft_jar.py [path/to/pawncraft-VERSION.jar]
Requires Python 3.11+ (standard library only). No Minecraft or network access.
The default artifact and expected version come from gradle.properties.
"""

import argparse
from collections import Counter
from pathlib import Path, PurePosixPath
import re
import sys
import tomllib
from zipfile import BadZipFile, ZipFile


ROOT = Path(__file__).resolve().parents[1]
METADATA = "META-INF/neoforge.mods.toml"
LUA = "data/computercraft/lua/rom"
LICENSES = {
    "META-INF/licenses/pawncraft/LICENSE.txt": ROOT / "LICENSE",
    "META-INF/licenses/pawncraft/TEMPLATE_LICENSE.txt": ROOT / "TEMPLATE_LICENSE.txt",
    "META-INF/licenses/ccpawn/pawn-LICENSE.txt": ROOT / "third_party/pawn/LICENSE",
    "META-INF/licenses/ccpawn/pawn-NOTICE.txt": ROOT / "third_party/pawn/NOTICE",
}
REQUIRED = {
    METADATA,
    "industries/knogle/ccpawn/CcPawnMod.class",
    "industries/knogle/ccpawn/PawnApi.class",
    "industries/knogle/ccpawn/NativeTools.class",
    "ccpawn-native/linux-x86_64/ccpawn-pawncc",
    "ccpawn-native/linux-x86_64/ccpawn-runner",
    f"{LUA}/programs/pawncc.lua",
    f"{LUA}/programs/pawn.lua",
    f"{LUA}/modules/main/ccpawn/runtime.lua",
    f"{LUA}/modules/main/ccpawn/json.lua",
    f"{LUA}/help/pawn.txt",
}
REQUIRED.update(LICENSES)
REQUIRED.update(
    "ccpawn-native/include/" + name + ".inc"
    for name in (
        "computercraft", "ccevents", "ccjson", "ccperipheral", "ccstdlib",
        "console", "core", "default", "float", "string", "time",
    )
)


def removed_guest_java(name):
    """Reject removed feature outputs, not Java-based NeoForge infrastructure."""
    path = PurePosixPath(name)
    return (
        "ccjava" in path.parts
        or path.name in {"worker.jar", "ccjava-worker.jar", "jcc.lua", "jrun.lua"}
        or path.name == "JavaApi.class"
        or (path.name.startswith("JavaApi$") and path.name.endswith(".class"))
    )


def check_jar(path, expected_version):
    errors = []
    with ZipFile(path) as archive:
        entries = archive.infolist()
        names = {entry.filename for entry in entries}
        duplicates = sorted(name for name, count in Counter(
            entry.filename for entry in entries).items() if count > 1)
        if duplicates:
            errors.append("Duplicate entries: " + ", ".join(duplicates))

        missing = sorted(REQUIRED - names)
        if missing:
            errors.append("Missing required entries: " + ", ".join(missing))
        empty = sorted(entry.filename for entry in entries
                       if entry.filename in REQUIRED and entry.file_size == 0)
        if empty:
            errors.append("Empty required entries: " + ", ".join(empty))
        stale = sorted(name for name in names if removed_guest_java(name))
        if stale:
            errors.append("Removed guest Java support is still packaged: "
                          + ", ".join(stale))

        for name, source in LICENSES.items():
            if name in names and archive.read(name) != source.read_bytes():
                errors.append(f"License/notice differs from {source.relative_to(ROOT)}: "
                              + name)

        if METADATA in names:
            try:
                metadata = tomllib.loads(archive.read(METADATA).decode("utf-8"))
                if metadata.get("license") != "MIT":
                    errors.append("Metadata license: expected 'MIT', "
                                  f"got {metadata.get('license')!r}")
                mods = metadata.get("mods", [])
                if not isinstance(mods, list) or len(mods) != 1:
                    errors.append("Expected exactly one [[mods]] metadata entry")
                elif not isinstance(mods[0], dict):
                    errors.append("Invalid [[mods]] metadata entry")
                else:
                    for key, expected in {
                        "modId": "ccpawn",
                        "displayName": "PawnCraft",
                        "version": expected_version,
                    }.items():
                        actual = mods[0].get(key)
                        if actual != expected:
                            errors.append(f"Metadata {key}: expected {expected!r}, "
                                          f"got {actual!r}")
            except (UnicodeError, tomllib.TOMLDecodeError) as error:
                errors.append(f"Invalid mod metadata: {error}")

        corrupt = archive.testzip()
        if corrupt:
            errors.append("Corrupt ZIP entry: " + corrupt)
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jar", nargs="?", type=Path,
                        help="JAR to check (default: build/libs/pawncraft-VERSION.jar)")
    args = parser.parse_args()
    properties = (ROOT / "gradle.properties").read_text(encoding="utf-8")
    match = re.search(r"^mod_version\s*=\s*(\S+)\s*$", properties, re.MULTILINE)
    if not match:
        parser.error("gradle.properties has no mod_version")
    version = match.group(1)
    path = args.jar or ROOT / "build/libs" / f"pawncraft-{version}.jar"
    try:
        errors = check_jar(path, version)
    except (OSError, BadZipFile, RuntimeError) as error:
        print(f"FAIL: {path}: {error}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print(f"PASS: PawnCraft {version} ({path}): {len(REQUIRED)} required entries, "
          "valid MIT metadata, exact license/notice copies, no guest Java runtime")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
