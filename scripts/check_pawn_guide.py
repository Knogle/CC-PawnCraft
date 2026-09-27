#!/usr/bin/env python3
"""Validate the standalone PAWN guide against the bundled compiler and API.

No network or Minecraft access. All generated source/AMX fixtures live in a
temporary directory. Console-only examples run with a mock terminal transport.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

from stdlib_test import run_case

ROOT = pathlib.Path(__file__).resolve().parents[1]
GUIDE = ROOT / "docs/PAWN-GUIDE.md"
INCLUDES = ROOT / "src/main/resources/ccpawn-native/include"
FENCE = chr(96) * 3


def main():
    compiler = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (
        ROOT / "build/native/bin/ccpawn-pawncc")
    runner = pathlib.Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else (
        ROOT / "build/native/bin/ccpawn-runner")
    if not compiler.is_file() or not runner.is_file():
        raise SystemExit("Build local native tools first: ./gradlew buildNative")

    guide = GUIDE.read_text(encoding="utf-8")
    blocks = re.findall(r"^" + FENCE + r"pawn\n(.*?)^" + FENCE + r"\s*$",
                        guide, flags=re.MULTILINE | re.DOTALL)
    if not blocks:
        raise AssertionError("Guide contains no PAWN programs")

    declaration = re.compile(
        r"^\s*(?:native|stock)\s+(?:(?:bool|Float):)?([A-Za-z_]\w*)\s*\(",
        re.MULTILINE,
    )
    functions = set()
    for include in INCLUDES.glob("*.inc"):
        functions.update(declaration.findall(include.read_text(encoding="utf-8")))
    missing = sorted(name for name in functions if not re.search(
        r"\b" + re.escape(name) + r"\s*\(", guide))
    if missing:
        raise AssertionError("Undocumented public functions: " + ", ".join(missing))

    # Check the handwritten table of contents against Markdown heading anchors.
    headings = re.findall(r"^#{1,6} (.+)$", guide, re.MULTILINE)
    anchors = {re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
               for heading in headings}
    for target in re.findall(r"\]\(#([^)]+)\)", guide):
        if target not in anchors:
            raise AssertionError("Missing heading anchor: " + target)

    seen, executed = set(), 0
    with tempfile.TemporaryDirectory(prefix="ccpawn-guide-") as temporary:
        directory = pathlib.Path(temporary)
        for index, source in enumerate(blocks, 1):
            match = re.search(r"^// example: ([a-z0-9_]+\.pwn)$",
                              source, re.MULTILINE)
            if not match:
                raise AssertionError(f"Example {index} has no safe unique filename")
            name = match.group(1)
            if name in seen:
                raise AssertionError("Duplicate example filename: " + name)
            seen.add(name)
            if "#include <computercraft>" not in source or not re.search(
                    r"\bmain\s*\(\s*\)", source):
                raise AssertionError(name + " is not a standalone program")
            path = directory / name
            path.write_text(source, encoding="utf-8")
            output = path.with_suffix(".amx")
            result = subprocess.run(
                [str(compiler), str(path), "-o" + str(output),
                 "-i" + str(INCLUDES), "-d2"],
                capture_output=True, text=True, timeout=10, cwd=directory,
            )
            diagnostics = result.stdout + result.stderr
            if result.returncode != 0 or not output.is_file():
                raise AssertionError(name + " failed to compile:\n" + diagnostics)
            if re.search(r"\bwarning \d+", diagnostics):
                raise AssertionError(name + " compiled with warnings:\n" + diagnostics)

            if "// doc-test: console" in source:
                run_case(str(compiler), str(runner), ROOT, temporary,
                         path.stem + "_run", source)
                executed += 1
                print("PASS compile + console VM:", name)
            else:
                print("PASS compile:", name)
    print(f"Guide checked: {len(blocks)} warning-free programs, "
          f"{executed} console VM executions, "
          f"{len(functions)} public functions documented, TOC valid")


if __name__ == "__main__":
    main()
