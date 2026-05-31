#!/usr/bin/env python3

import argparse
import difflib
import pathlib
import re
import subprocess
import sys


def read_normalized_text(path: pathlib.Path) -> str:
    data = path.read_bytes()
    data = re.sub(rb"\r+\n?", b"\n", data)
    return data.decode("utf-8")


def normalize_paths(text: str, tests_dir: pathlib.Path) -> str:
    text = text.replace("\\", "/")
    tests_dir_text = tests_dir.as_posix()
    utils_text = (tests_dir / "../utils").as_posix()
    text = text.replace(utils_text, "<chip8testsuite>/src/tests/../utils")
    text = text.replace(tests_dir_text, "<chip8testsuite>/src/tests")
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assembler", required=True)
    parser.add_argument("--tests-dir", required=True)
    parser.add_argument("--reference", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args()

    assembler = pathlib.Path(args.assembler).resolve()
    tests_dir = pathlib.Path(args.tests_dir).resolve()
    reference = pathlib.Path(args.reference)
    work_dir = pathlib.Path(args.work_dir)
    source = tests_dir / "5-quirks.8o"
    actual = work_dir / "5-quirks-line-info.8o"

    work_dir.mkdir(parents=True, exist_ok=True)

    command = [
        str(assembler),
        "-P",
        "-q",
        "-o",
        str(actual),
        str(source),
    ]
    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        print("preprocessor failed", file=sys.stderr)
        print(f"command: {' '.join(command)}", file=sys.stderr)
        print(f"stdout:\n{result.stdout}", file=sys.stderr)
        print(f"stderr:\n{result.stderr}", file=sys.stderr)
        return result.returncode

    expected_text = read_normalized_text(reference)
    actual_text = normalize_paths(read_normalized_text(actual), tests_dir)

    if actual_text == expected_text:
        print("Preprocessor line-info output matches reference.")
        return 0

    diff = difflib.unified_diff(
        expected_text.splitlines(keepends=True),
        actual_text.splitlines(keepends=True),
        fromfile=str(reference),
        tofile=str(actual),
        n=3,
    )
    max_diff_lines = 200
    diff_lines = list(diff)
    print("preprocessor line-info output changed", file=sys.stderr)
    print("".join(diff_lines[:max_diff_lines]), file=sys.stderr)
    if len(diff_lines) > max_diff_lines:
        print("... diff truncated ...", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
