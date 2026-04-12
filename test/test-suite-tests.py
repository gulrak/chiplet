#!/usr/bin/env python3

import argparse
import pathlib
import subprocess
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests-dir", required=True)
    ap.add_argument("--bin-dir", required=True)
    args = ap.parse_args()

    tests_dir = pathlib.Path(args.tests_dir)
    bin_dir = pathlib.Path(args.bin_dir)

    failures = []
    test_files = sorted(tests_dir.glob("*.8o"))

    if not test_files:
        print(f"No .8o files found in {tests_dir}", file=sys.stderr)
        return 1

    for src in test_files:
        stem = src.stem
        expected = bin_dir / f"{stem}.ch8"
        actual = pathlib.Path(f"{stem}.ch8")


        if not expected.exists():
            failures.append(f"{stem}: missing expected binary {expected}")
            continue

        # Adjust CLI flags to match your assembler.
        cmd = ["bin/chiplet", str(src), "-o", str(actual)]
        print(f"Executing: {' '.join(cmd)}")
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            failures.append(
                f"{stem}: assembler failed\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            )
            continue

        actual_bytes = actual.read_bytes()
        expected_bytes = expected.read_bytes()

        if actual_bytes != expected_bytes:
            failures.append(
                f"{stem}: binary mismatch "
                f"(expected {len(expected_bytes)} bytes, got {len(actual_bytes)} bytes)"
            )

    if failures:
        print(f"{len(failures)} test(s) failed:\n", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}\n", file=sys.stderr)
        return 1

    print(f"All {len(test_files)} external corpus tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())