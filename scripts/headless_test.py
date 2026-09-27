#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile


def decode(value: str) -> str:
    return "" if value == "-" else bytes.fromhex(value).decode("utf-8")


def reply(process, fields, values=()):
    sequence = fields[1]
    encoded = ["RETURN", sequence, str(len(values))]
    for kind, value in values:
        encoded.extend((kind, str(value)))
    process.stdin.write("\t".join(encoded) + "\n")
    process.stdin.flush()


def main():
    compiler = pathlib.Path(sys.argv[1]).resolve()
    runner = pathlib.Path(sys.argv[2]).resolve()
    root = pathlib.Path(__file__).resolve().parent.parent
    source = root / "examples" / "headless_test.pwn"
    includes = root / "src" / "main" / "resources" / "ccpawn-native" / "include"

    with tempfile.TemporaryDirectory(prefix="ccpawn-test-") as temporary:
        program = pathlib.Path(temporary) / "headless_test.amx"
        compile_result = subprocess.run(
            [compiler, source, f"-o{program}", f"-i{includes}", "-d2"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10,
        )
        if compile_result.returncode != 0:
            raise AssertionError("pawncc failed:\n" + compile_result.stdout)

        process = subprocess.Popen(
            [runner, program], text=True, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        assert process.stdout.readline().rstrip("\n") == "READY"
        writes = []
        while True:
            line = process.stdout.readline()
            if not line:
                raise AssertionError("runner exited early: " + process.stderr.read())
            fields = line.rstrip("\n").split("\t")
            if fields[0] == "DONE":
                assert fields[1] == "0"
                break
            assert fields[0] == "CALL", fields
            api, method = decode(fields[2]), decode(fields[3])
            if (api, method) == ("__cc", "write"):
                writes.append(decode(fields[6]))
                reply(process, fields)
            elif (api, method) == ("redstone", "getInput"):
                assert decode(fields[6]) == "right"
                reply(process, fields, (("B", "1"),))
            else:
                raise AssertionError(f"unexpected syscall {api}.{method}: {fields}")

        assert process.wait(timeout=2) == 0
        assert writes == ["hello from pawn\n", "redstone is high\n"], repr(writes)
        print("headless PAWN compiler/AMX/CC protocol test passed")

    subprocess.run(
        [sys.executable, root / "scripts/redstone_test.py", compiler, runner],
        check=True, timeout=40,
    )
    subprocess.run(
        [sys.executable, root / "scripts/annihilator_test.py", compiler, runner],
        check=True, timeout=40,
    )


if __name__ == "__main__":
    main()
