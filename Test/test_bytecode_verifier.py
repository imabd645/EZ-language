"""Malformed .ezc regression checks. Run: python Test/test_bytecode_verifier.py build/ez.exe"""

import pathlib
import re
import struct
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "bytecode" / "Bytecode.h"


def opcode_values():
    source = HEADER.read_text(encoding="utf-8")
    body = re.search(r"enum class OpCode\s*:\s*uint8_t\s*\{(.*?)\n\};", source, re.S).group(1)
    body = re.sub(r"//.*", "", body)
    values = {}
    next_value = 0
    for declaration in body.split(","):
        item = declaration.strip()
        if not item:
            continue
        if "=" in item:
            name, value = item.split("=", 1)
            next_value = int(value.strip(), 0)
        else:
            name = item
        values[name.strip()] = next_value
        next_value += 1
    return values


OPS = opcode_values()


def u32(value):
    return struct.pack("@I", value)


def string(value):
    data = value.encode("utf-8")
    return u32(len(data)) + data


def bundle(code, *, local_count=0, constants=(), global_slots=()):
    data = bytearray(b"EZC3")
    data += u32(len(global_slots))
    for name in global_slots:
        data += string(name)

    data += string("<main>") + string("verifier-test.ez")
    data += u32(0)                         # arity
    data += bytes((0, 0, 0))               # function flags
    data += u32(0)                         # upvalue count
    data += u32(local_count)
    data += u32(0)                         # upvalue table size
    data += u32(0)                         # local debug table size
    data += u32(0)                         # parameter names
    data += u32(0)                         # call-site table
    data += u32(len(code)) + bytes(code)   # instruction bytes
    data += b"".join(struct.pack("@N", 1) for _ in code)  # line table
    data += u32(len(constants))
    for value in constants:
        data += bytes((4,)) + string(value)  # Constant::Type::STRING
    data += u32(0)                         # nested functions
    return data


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python Test/test_bytecode_verifier.py <path-to-ez.exe>")
    interpreter = pathlib.Path(sys.argv[1]).resolve()
    if not interpreter.is_file():
        raise SystemExit(f"interpreter not found: {interpreter}")

    valid_source = """\
model BundleVerifierCheck {
    init(value) { self.value = value }
    task getValue() { give self.value }
}
item = BundleVerifierCheck(37)
out item.value
out item.getValue()
"""

    cases = {
        "invalid opcode": bundle([0xFF]),
        "truncated operand": bundle([OPS["LOAD_CONST"]]),
        "invalid constant index": bundle([OPS["LOAD_CONST"], 0, 0, OPS["RETURN"]]),
        "invalid local slot": bundle([OPS["LOAD_LOCAL"], 0, OPS["RETURN"]]),
        "invalid jump target": bundle([OPS["JUMP"], 0, 0, 0, 10]),
        "invalid global slot": bundle([OPS["LOAD_GLOBAL_SLOT"], 0, 0, OPS["RETURN"]]),
    }

    failures = []
    with tempfile.TemporaryDirectory(prefix="ezc-verifier-") as temp:
        source_path = pathlib.Path(temp) / "valid_bundle.ez"
        source_path.write_text(valid_source, encoding="utf-8")
        compile_result = subprocess.run(
            [str(interpreter), str(source_path), "--compile"], capture_output=True, text=True
        )
        bundle_path = source_path.with_suffix(".ezc")
        if compile_result.returncode != 0 or not bundle_path.is_file():
            failures.append(("valid bundle compilation", compile_result.returncode,
                             compile_result.stdout + compile_result.stderr))
        else:
            run_result = subprocess.run(
                [str(interpreter), str(bundle_path)], capture_output=True, text=True
            )
            if run_result.returncode != 0 or run_result.stdout.splitlines()[-2:] != ["37", "37"]:
                failures.append(("valid bundle round trip", run_result.returncode,
                                 run_result.stdout + run_result.stderr))
            else:
                print("PASS: valid bundle with property and method inline caches")

        for name, contents in cases.items():
            path = pathlib.Path(temp) / (name.replace(" ", "_") + ".ezc")
            path.write_bytes(contents)
            result = subprocess.run([str(interpreter), str(path)], capture_output=True, text=True)
            diagnostic = result.stdout + result.stderr
            if result.returncode != 65 or "Corrupt bytecode" not in diagnostic:
                failures.append((name, result.returncode, diagnostic))
            else:
                print(f"PASS: {name}")

    if failures:
        for name, code, output in failures:
            print(f"FAIL: {name} (exit {code})")
            print(output)
        raise SystemExit(1)

    print(f"All {len(cases)} malformed-bundle checks and the valid-bundle round trip passed.")


if __name__ == "__main__":
    main()
