#!/usr/bin/env python3
"""Reject bad native arguments without emitting half a CC request."""
import pathlib
import subprocess
import struct
import sys
import tempfile
from stdlib_test import run_case
from redstone_test import running, call

compiler, runner = (str(pathlib.Path(value).resolve()) for value in sys.argv[1:3])
root = pathlib.Path(__file__).resolve().parents[1]
source = r'''
#include <computercraft>
main() {
    if (cc_call("os", "time", "x", 1) != -1) return 1;
    if (cc_peripheral_call("left", "test", "x", 1) != -1) return 2;
    if (cc_call("os", "time", "f", Float:0x7f800000) != -1) return 3;
    if (cc_call("os", "time", "ss", "only one argument") != -1) return 4;
    printf("transport intact: %d\n", 42);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ccpawn-transport-") as directory:
    assert run_case(compiler, runner, root, directory, "bad_request", source) == ["transport intact: 42\n"]

float_source = r'''
#include <computercraft>
main() {
    new error[128];
    for (new i = 0; i < 6; i++) {
        if (cc_call("os", "time", "") != -1) return 1;
        if (cc_last_error(error) <= 0) return 2;
        if (cc_result_count() != 0) return 3;
    }
    if (cc_call("os", "time", "") != 1) return 4;
    if (floatabs(cc_result_float(0) - 1.25) > 0.001) return 5;
    new expected[3] = [0x116c2, 0x800116c2, 1];
    for (new i = 0; i < sizeof expected; i++) {
        if (cc_call("os", "time", "") != 1) return 9;
        new Float:subnormal = cc_result_float(0);
        if (_:subnormal != expected[i]) return 10;
        if (cc_call("os", "startTimer", "f", subnormal) < 0) return 11;
    }
    if (cc_call("os", "getComputerLabel", "") != 1) return 6;
    new tiny[4];
    if (cc_result_string(0, tiny) != 16) return 7;
    if (strcmp(tiny, "abc") != 0) return 8;
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ccpawn-float-reply-") as directory:
    path = pathlib.Path(directory) / "float_reply.pwn"
    path.write_text(float_source)
    program = path.with_suffix(".amx")
    compiled = subprocess.run(
        [compiler, path, f"-o{program}",
         f"-i{root / 'src/main/resources/ccpawn-native/include'}", "-d2"],
        capture_output=True, text=True, timeout=10,
    )
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    with running(runner, program) as process:
        for value in ("nan", "inf", "-inf", "1e99", "1.0oops", "1e-99", "1.25"):
            sequence, received = call(process)
            assert received == ("os", "time", []), received
            process.stdin.write(f"RETURN\t{sequence}\t1\tF\t{value}\n")
            process.stdin.flush()
        for value, bits in (("1e-40", 0x116c2), ("-1e-40", 0x800116c2), ("1.40129846e-45", 1)):
            sequence, received = call(process)
            assert received == ("os", "time", []), received
            process.stdin.write(f"RETURN\t{sequence}\t1\tF\t{value}\n")
            process.stdin.flush()
            sequence, received = call(process)
            assert received[:2] == ("os", "startTimer"), received
            assert received[2][0][0] == "F", received
            assert struct.pack("<f", received[2][0][1]) == struct.pack("<I", bits), received
            process.stdin.write(f"RETURN\t{sequence}\t0\n")
            process.stdin.flush()
        sequence, received = call(process)
        assert received == ("os", "getComputerLabel", []), received
        process.stdin.write(f"RETURN\t{sequence}\t1\tS\t{'abcdefghijklmnop'.encode().hex()}\n")
        process.stdin.flush()
        assert process.stdout.readline().strip() == "DONE\t0"
        assert process.wait(timeout=2) == 0
        assert not process.stderr.read().strip()
print("PAWN transport passed: bad requests preserve framing; invalid Float replies rejected; subnormal round trips preserved")
