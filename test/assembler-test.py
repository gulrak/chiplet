#!/usr/bin/env python3
"""Compare Octo fixtures, warning on diagnostic text or location differences."""

from __future__ import annotations

import argparse
import math
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Sequence
from fnmatch import fnmatchcase
from pathlib import Path


EXPECTED_ERROR = re.compile(r"\((\d+):(\d+)\)\s*(.*)")
GENERATED_ERROR = re.compile(r"^.*?:(\d+):(\d+):\s*(.*)", re.MULTILINE)
C_OCTO_ERROR = re.compile(r"^\((\d+):(\d+)\)\s*(.*)", re.MULTILINE)


def check_result(
    source: Path, output: Path, result: subprocess.CompletedProcess[str],
    backend: str = "chiplet",
) -> str | None:
    binary = source.with_suffix(".ch8")
    error = source.with_suffix(".err")
    if binary.is_file() == error.is_file():
        return "expected exactly one .ch8 or .err reference"
    if binary.is_file():
        if result.returncode != 0:
            return f"compilation failed with return code {result.returncode}"
        if not output.is_file():
            return "compiler returned success without creating a binary"
        expected, actual = binary.read_bytes(), output.read_bytes()
        if expected != actual:
            offset = next(
                (index for index, pair in enumerate(zip(expected, actual))
                 if pair[0] != pair[1]),
                min(len(expected), len(actual)),
            )
            return (
                f"binary differs at offset 0x{offset:X}; "
                f"expected {len(expected)} bytes, got {len(actual)}"
            )
        return None
    if result.returncode <= 0:
        return f"expected an error exit, got return code {result.returncode}"
    expected = EXPECTED_ERROR.fullmatch(
        error.read_text(encoding="utf-8").strip()
    )
    if expected is None:
        return "invalid .err reference; expected '(line:column) message'"
    pattern = GENERATED_ERROR if backend == "chiplet" else C_OCTO_ERROR
    generated = pattern.search(result.stderr)
    if generated is None:
        return "compiler did not report a source-positioned assembly error"
    # Require rejection of invalid input, but allow diagnostic differences.
    if expected[1] != generated[1]:
        print(
            f"WARNING {source.name}: expected error line {expected[1]}, "
            f"got {generated[1]}"
        )
    if expected[3] != generated[3]:
        print(
            f"WARNING {source.name}: expected diagnostic: {expected[3]}\n"
            f"  actual diagnostic: {generated[3]}"
        )
    if expected[2] != generated[2]:
        print(
            f"WARNING {source.name}: expected error column {expected[2]}, "
            f"got {generated[2]}"
        )
    return None


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembler")
    parser.add_argument("testdir", type=Path)
    parser.add_argument(
        "--backend", choices=("chiplet", "c-octo", "octo-js"),
        default="chiplet",
        help="compiler command and diagnostic format (default: chiplet)",
    )
    parser.add_argument(
        "--js-compiler", type=Path,
        help="Octo compiler.js path (required for --backend octo-js)",
    )
    parser.add_argument(
        "--filter", default="*.8o", help="source filename glob"
    )
    parser.add_argument(
        "--exclude", action="append", default=[],
        help="exclude a source filename glob (repeatable)",
    )
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)
    if args.backend == "octo-js" and args.js_compiler is None:
        parser.error("--backend octo-js requires --js-compiler")
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be a positive finite number")
    executable = shutil.which(args.assembler) or args.assembler
    assembler = str(Path(executable).expanduser().resolve())
    sources = sorted(
        source for source in args.testdir.resolve().glob(args.filter)
        if source.is_file() and source.suffix == ".8o"
        and not any(fnmatchcase(source.name, pattern) for pattern in args.exclude)
    )
    if not sources:
        print("ERROR: No matching .8o fixtures found.", file=sys.stderr)
        return 2
    failures = 0
    try:
        with tempfile.TemporaryDirectory(prefix="chiplet-assembler-") as path:
            work_dir = Path(path)
            for index, source in enumerate(sources):
                output = work_dir / f"{index}.ch8"
                command = (
                    [assembler, str(source), str(output)]
                    if args.backend == "c-octo"
                    else [assembler, "-q", "-o", str(output),
                          "--no-line-info", str(source)]
                )
                if args.backend == "octo-js":
                    adapter = Path(__file__).with_name("octo-js-cli.cjs")
                    command = [assembler, str(adapter.resolve()),
                               str(args.js_compiler.resolve()),
                               str(source), str(output)]
                try:
                    result = subprocess.run(
                        command,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        errors="replace",
                        timeout=args.timeout,
                        check=False,
                    )
                except subprocess.TimeoutExpired:
                    failures += 1
                    print(f"FAIL {source.name}: compiler timed out")
                    continue
                problem = check_result(source, output, result, args.backend)
                if problem is None:
                    print(f"PASS {source.name}")
                else:
                    failures += 1
                    print(f"FAIL {source.name}: {problem}")
                    diagnostics = (result.stdout + result.stderr).strip()
                    if diagnostics:
                        print(diagnostics[:2000])
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    print(
        f"{len(sources) - failures}/{len(sources)} passed, {failures} failed"
    )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
