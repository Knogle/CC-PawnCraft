#!/usr/bin/env python3
"""Real PAWN compilation/VM tests for safe standard-library additions."""
import pathlib
import subprocess
import sys
import tempfile
import threading


def run_case(compiler, runner, root, temporary, name, source, expected_error=False):
    path = pathlib.Path(temporary) / (name + ".pwn")
    path.write_text(source)
    program = path.with_suffix(".amx")
    result = subprocess.run(
        [compiler, path, f"-o{program}", f"-i{root / 'src/main/resources/ccpawn-native/include'}", "-d2"],
        capture_output=True, text=True, timeout=10,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    proc = subprocess.Popen([runner, program], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    writes = []
    timeout = threading.Timer(10, proc.kill)
    timeout.start()
    try:
        assert proc.stdout.readline().strip() == "READY", name
        while True:
            line = proc.stdout.readline()
            assert line, f"{name}: runner exited early: {proc.stderr.read()}"
            parts = line.rstrip("\n").split("\t")
            if parts[0] == "ERROR":
                assert expected_error, f"{name}: {parts}"
                break
            if parts[0] == "DONE":
                assert not expected_error and parts[1] == "0", f"{name}: {parts}"
                break
            assert parts[0] == "CALL", parts
            api, method = (bytes.fromhex(value).decode() for value in parts[2:4])
            assert (api, method) == ("__cc", "write"), parts
            assert parts[4:6] == ["1", "S"], parts
            writes.append("" if parts[6] == "-" else bytes.fromhex(parts[6]).decode())
            proc.stdin.write(f"RETURN\t{parts[1]}\t0\n")
            proc.stdin.flush()
        code = proc.wait(timeout=2)
        stderr = proc.stderr.read()
        assert not stderr.strip(), f"{name}: unexpected stderr (including sanitizer diagnostics): {stderr}"
        assert (code != 0) == expected_error, (name, code)
    finally:
        timeout.cancel()
        if proc.poll() is None:
            proc.kill()
            proc.wait()
    return writes


def main():
    compiler, runner = (str(pathlib.Path(value).resolve()) for value in sys.argv[1:3])
    root = pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="ccpawn-stdlib-") as temporary:
        writes = run_case(compiler, runner, root, temporary, "stdlib",
                         (root / "examples/stdlib_test.pwn").read_text())
        assert writes == ["OUTPUT -42 hello 2.50 101\n", "stdlib OK", "\n"], writes
        writes = run_case(compiler, runner, root, temporary, "stdlib_bounds",
                         (root / "examples/stdlib_bounds_test.pwn").read_text())
        assert writes == ["stdlib bounds OK", "\n"], writes
        for name, code in {
            "sqrt_domain": "new Float:x = floatsqroot(-1.0); printf(\"%f\", x);",
            "float_divide_zero": "new Float:x = floatdiv(1.0, 0.0); printf(\"%f\", x);",
            "float_int_range": "return floatround(2147483648.0);",
            "strval_range": "return strval(\"999999999999999999999999999\");",
            "random_zero": "return random(0);",
            "clamp_invalid": "return clamp(1, 9, 2);",
        }.items():
            run_case(compiler, runner, root, temporary, name,
                     "#include <computercraft>\n#include <ccstdlib>\nmain() { " + code + " }\n", True)
    print("PAWN stdlib tests passed: strings, formatting, float/operators, helpers, hostile bounds/formats, 6 domain errors")


if __name__ == "__main__":
    main()
