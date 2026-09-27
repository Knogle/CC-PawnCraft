#!/usr/bin/env python3
"""Check native compiler attribution and unchanged command-line behavior.

Usage: python3 scripts/compiler_notice_test.py [compiler] [--reference-compiler PATH]
The optional reference is an unadapted compiler built from the same pinned
PAWN revision. All fixtures and output files live in a temporary directory.
Uses the Python standard library only; no Minecraft or network access.
"""

import argparse
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[1]
NOTICE = (
    "GNU C Library: Copyright (C) Free Software Foundation, Inc. and other contributors.\n"
    "LGPL-2.1-or-later; see META-INF/licenses/glibc/LGPL-2.1.txt in the PawnCraft JAR.\n\n"
)
COMPILER_BANNER = "Copyright (c) 1997-2024, CompuPhase"


def invoke(compiler, arguments, directory, output=None, diagnostic_file=None):
    result = subprocess.run(
        [str(compiler), *arguments], cwd=directory, capture_output=True,
        text=True, timeout=10, env={"LC_ALL": "C"},
    )
    program = output.read_bytes() if output is not None and output.is_file() else None
    diagnostics = (diagnostic_file.read_bytes()
                   if diagnostic_file is not None and diagnostic_file.is_file() else None)
    return result, program, diagnostics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", nargs="?", type=Path,
                        default=ROOT / "build/native/bin/ccpawn-pawncc")
    parser.add_argument("--reference-compiler", type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    reference = args.reference_compiler.resolve() if args.reference_compiler else None
    if not compiler.is_file() or (reference is not None and not reference.is_file()):
        parser.error("Build the native compiler first: ./gradlew buildNative")

    checks = 0
    with TemporaryDirectory(prefix="pawncraft-compiler-notice-") as temporary:
        directory = Path(temporary)
        source = directory / "valid.pwn"
        source.write_text("main() { return 42; }\n", encoding="utf-8")
        bad_source = directory / "invalid.pwn"
        bad_source.write_text("main() { return unknown_symbol; }\n", encoding="utf-8")
        response = directory / "quiet.opts"
        response.write_text("-v0\n", encoding="utf-8")

        def check(name, arguments, expected_status, banner, output=None, diagnostic_file=None):
            nonlocal checks
            result, program, diagnostics = invoke(
                compiler, arguments, directory, output, diagnostic_file)
            assert result.returncode == expected_status, (
                name, result.returncode, result.stdout, result.stderr)
            assert (COMPILER_BANNER in result.stdout) == banner, (name, result.stdout)
            assert result.stdout.count(NOTICE) == int(banner), (name, result.stdout)
            assert "GNU C Library" not in result.stderr, (name, result.stderr)
            if banner:
                assert result.stdout.index(COMPILER_BANNER) < result.stdout.index(NOTICE)
            if reference is not None:
                original, original_program, original_diagnostics = invoke(
                    reference, arguments, directory, output, diagnostic_file)
                assert result.returncode == original.returncode, name
                assert result.stdout.replace(NOTICE, "") == original.stdout, (
                    name, result.stdout, original.stdout)
                assert result.stderr == original.stderr, (name, result.stderr, original.stderr)
                assert program == original_program, name + ": AMX output changed"
                assert diagnostics == original_diagnostics, name + ": error-file output changed"
            checks += 1
            print("PASS:", name)
            return result, program, diagnostics

        normal = directory / "normal.amx"
        result, normal_program, _ = check(
            "normal compilation with attribution",
            [str(source), "-p", "-d0", "-o" + str(normal)], 0, True, normal)
        assert normal_program and not result.stderr

        for name, options in (
            ("quiet compilation", ["-v0"]),
            ("quiet response-file compilation", ["@" + str(response)]),
        ):
            output = directory / (name.replace(" ", "_") + ".amx")
            result, program, _ = check(
                name, [str(source), "-p", "-d0", "-o" + str(output), *options],
                0, False, output)
            assert result.stdout == "" and result.stderr == "", name
            assert program == normal_program, name + ": verbosity changed AMX output"

        check("help", ["-?"], 3, True)
        check("help despite quiet option", ["-v0", "-?"], 3, True)
        check("no-argument usage", [], 3, True)

        for name, options, banner in (
            ("compile error", [], True),
            ("quiet compile error", ["-v0"], False),
        ):
            output = directory / (name.replace(" ", "_") + ".amx")
            result, program, _ = check(
                name, [str(bad_source), "-p", "-d0", "-o" + str(output), *options],
                1, banner, output)
            assert program is None, name + ": invalid source produced AMX"
            assert "error 017" in result.stderr, (name, result.stderr)

        error_file = directory / "compile-errors.txt"
        output = directory / "error_file.amx"
        result, program, diagnostics = check(
            "compile error redirected with -e",
            [str(bad_source), "-p", "-d0", "-o" + str(output), "-e" + str(error_file)],
            1, False, output, error_file)
        assert result.stdout == "" and result.stderr == ""
        assert program is None and diagnostics and b"error 017" in diagnostics

        check("help suppressed by -e", ["-e" + str(error_file), "-?"], 3, False)

    detail = "; output/status/AMX match the reference compiler" if reference else ""
    print(f"Compiler notice tests passed: {checks} real CLI cases{detail}")


if __name__ == "__main__":
    main()
