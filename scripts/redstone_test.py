#!/usr/bin/env python3
"""Run actual PAWN programs against simulated CC inputs, with a hard timeout."""
import contextlib
import pathlib
import subprocess
import sys
import tempfile
import threading


def reply(process, sequence, values=()):
    fields = ["RETURN", sequence, str(len(values))]
    for kind, value in values:
        fields.extend((kind, str(value)))
    process.stdin.write("\t".join(fields) + "\n")
    process.stdin.flush()


def compile_program(compiler, root, directory, name):
    program = directory / (name + ".amx")
    includes = root / "src/main/resources/ccpawn-native/include"
    result = subprocess.run(
        [compiler, root / "examples" / (name + ".pwn"),
         f"-o{program}", f"-i{includes}", "-d2"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10,
    )
    assert result.returncode == 0, result.stdout
    return program


@contextlib.contextmanager
def running(runner, program):
    process = subprocess.Popen(
        [runner, program], text=True, stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    timeout = threading.Timer(10, process.kill)
    timeout.start()
    try:
        assert process.stdout.readline().rstrip("\n") == "READY"
        yield process
    finally:
        timeout.cancel()
        if process.poll() is None:
            process.terminate()
        process.communicate(timeout=3)


def call(process):
    line = process.stdout.readline()
    assert line, "runner exited early or exceeded the test timeout"
    fields = line.rstrip("\n").split("\t")
    assert fields[0] == "CALL", fields
    assert len(fields) == 5 + int(fields[4]) * 2, fields
    values = []
    for kind, value in zip(fields[5::2], fields[6::2]):
        if kind in ("S", "J"):
            value = "" if value == "-" else bytes.fromhex(value).decode()
        elif kind == "F":
            value = float(value)
        else:
            value = int(value)
        values.append((kind, value))
    return fields[1], (bytes.fromhex(fields[2]).decode(),
                       bytes.fromhex(fields[3]).decode(), values)


def test_arguments(runner, program):
    expected = [
        ("redstone", "setOutput", [("S", "back"), ("B", 1)]),
        ("redstone", "setOutput", [("S", "back"), ("B", 0)]),
        ("redstone", "setAnalogOutput", [("S", "back"), ("I", 0)]),
        ("redstone", "setAnalogOutput", [("S", "back"), ("I", 15)]),
        ("term", "setCursorPos", [("I", -7), ("I", 42)]),
        ("os", "startTimer", [("F", 1.5)]),
        ("__cc", "sleep", [("I", 50)]),
    ]
    with running(runner, program) as process:
        for wanted in expected:
            sequence, received = call(process)
            assert received == wanted, ("expected", wanted, "received", received)
            reply(process, sequence)
        assert process.stdout.readline().strip() == "DONE\t0"
        assert process.wait(timeout=2) == 0


def test_controller(runner, program):
    # A held or overlapping signal must not retrigger; both inputs must go low.
    scenarios = [(0, 0), (1, 0), (1, 0), (0, 1), (0, 0),
                 (0, 1), (0, 1), (1, 1), (0, 0), (1, 1)]
    with running(runner, program) as process:
        sequence, request = call(process)
        assert request == ("redstone", "setOutput", [("S", "back"), ("B", 1)])
        reply(process, sequence)
        output = 1
        previous = False
        transitions = [output]
        for right, left in scenarios:
            for side, value in (("right", right), ("left", left)):
                sequence, request = call(process)
                assert request == ("redstone", "getInput", [("S", side)])
                reply(process, sequence, (("B", value),))
            signal = bool(right or left)
            if signal and not previous:
                output = 1 - output
                sequence, request = call(process)
                assert request == ("redstone", "setOutput", [("S", "back"), ("B", output)])
                transitions.append(output)
                reply(process, sequence)
            sequence, request = call(process)
            assert request == ("__cc", "sleep", [("I", 50)])
            reply(process, sequence)
            previous = signal
        assert transitions == [1, 0, 1, 0], transitions


def main():
    compiler, runner = (pathlib.Path(value).resolve() for value in sys.argv[1:3])
    root = pathlib.Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="ccpawn-redstone-test-") as temporary:
        directory = pathlib.Path(temporary)
        test_arguments(runner, compile_program(compiler, root, directory, "scalars_test"))
        test_controller(runner, compile_program(compiler, root, directory, "ernter"))
    print("PAWN redstone tests passed: scalar values and harvester signal transitions")


if __name__ == "__main__":
    main()
