#!/usr/bin/env python3
"""Exercise the showcase's real AMX with simulated CC calls, without Minecraft."""

import pathlib
import subprocess
import sys
import tempfile

from redstone_test import call, reply, running


def exchange(process, api, method, arguments, values=(), fail=False):
    sequence, received = call(process)
    wanted = (api, method, arguments)
    assert received == wanted, ("expected", wanted, "received", received)
    if fail:
        message = "simulated CC failure".encode().hex()
        process.stdin.write(f"ERROR\t{sequence}\t{message}\n")
        process.stdin.flush()
        assert process.stdout.readline().strip() == "DONE\t1"
        assert process.wait(timeout=2) == 0
    else:
        reply(process, sequence, values)


def sleep(process, milliseconds, fail=False):
    exchange(process, "__cc", "sleep", [("I", milliseconds)], fail=fail)


def loop_start(process, counter, fail_print=False):
    sleep(process, 1000)
    exchange(process, "__cc", "write", [("S", f"Loop count: {counter}\n")],
             fail=fail_print)


def running_step(process, counter, signal):
    loop_start(process, counter)
    exchange(process, "redstone", "getInput", [("S", "right")], (("B", signal),))
    sleep(process, 500)


def switching_step(process, direction):
    loop_start(process, 1)
    exchange(process, "redstone", "setOutput", [("S", "left"), ("B", direction)])
    sleep(process, 10000)
    sleep(process, 500)


def test_state_machine(runner, program):
    with running(runner, program) as process:
        running_step(process, 1, 0)
        running_step(process, 2, 1)
        switching_step(process, 1)
        # Switching increments the reset counter to one. Subsequent running
        # loops therefore begin at two, preserving the original script.
        for counter in range(2, 76):
            running_step(process, counter, 0)
        switching_step(process, 0)
        # A held input retriggers after the switching wait, not on an edge.
        running_step(process, 2, 1)
        switching_step(process, 1)
        running_step(process, 2, 1)
        switching_step(process, 0)
        sleep(process, 1000, fail=True)
    print("PASS Annihilator: sensor, counter fallback, held input, direction and wait ordering")


def test_errors(runner, program):
    with running(runner, program) as process:
        loop_start(process, 1, fail_print=True)
    for failed_call in ("output", "switch_wait", "poll_wait"):
        with running(runner, program) as process:
            running_step(process, 1, 1)
            loop_start(process, 1)
            exchange(process, "redstone", "setOutput", [("S", "left"), ("B", 1)],
                     fail=failed_call == "output")
            if failed_call == "output":
                continue
            sleep(process, 10000, fail=failed_call == "switch_wait")
            if failed_call == "switch_wait":
                continue
            sleep(process, 500, fail=True)
    print("PASS Annihilator: print/output/sleep failures stop with PAWN return value 1")


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    compiler, runner = (pathlib.Path(value).resolve() for value in sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="pawncraft-annihilator-test-") as temporary:
        program = pathlib.Path(temporary) / "annihilator2.amx"
        # This example must stay warning-free, unlike the supplied source.
        result = subprocess.run(
            [compiler, root / "examples/annihilator2.pwn", f"-o{program}",
             f"-i{root / 'src/main/resources/ccpawn-native/include'}", "-d2"],
            text=True, capture_output=True, timeout=10, check=True,
        )
        assert "warning " not in (result.stdout + result.stderr).lower(), result
        test_state_machine(runner, program)
        test_errors(runner, program)


if __name__ == "__main__":
    main()
