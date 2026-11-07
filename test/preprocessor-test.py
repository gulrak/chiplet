#!/usr/bin/env python3
"""Compile preprocessor fixtures with Chiplet or the sibling Octopus2."""

from __future__ import annotations

import argparse
import math
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Sequence
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TESTS_DIR = PROJECT_ROOT / "test" / "octo-preprocessor-tests"


def run_command(
    command: list[str], timeout: float, cwd: Path | None = None
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors="replace",
        timeout=timeout,
        check=False,
    )


def build_octopus2(work_dir: Path, timeout: float) -> Path:
    """Build the sibling CLI into the temporary directory."""
    chipcode = PROJECT_ROOT.parent / "chipcode"
    module = chipcode / "octopus2"
    required = [
        chipcode / ".git",
        module / "go.mod",
        module / "src" / "main.go",
        module / "src" / "preprocessor" / "main.go",
    ]
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise ValueError(
            "Octopus2 requires a sibling chipcode working copy at "
            f"{chipcode}. "
            f"Missing: {', '.join(missing)}"
        )
    go = shutil.which("go")
    if go is None:
        raise ValueError("Octopus2 needs Go; 'go' was not found on PATH.")
    executable = work_dir / "octopus2"
    result = run_command(
        [go, "build", "-mod=readonly", "-o", str(executable), "./src"],
        timeout,
        cwd=module,
    )
    if result.returncode != 0:
        raise ValueError(
            "Could not build Octopus2 "
            "(Go and a C compiler for cgo are required):\n"
            + result.stdout
            + result.stderr
        )
    return executable


def find_chiplet(value: str | None) -> Path:
    if not value:
        raise ValueError("Specify --chiplet PATH, or select --octopus2.")
    executable = Path(shutil.which(value) or value).expanduser().resolve()
    if not executable.is_file():
        raise ValueError(f"Chiplet executable not found: {executable}")
    return executable


def load_fixtures(tests_dir: Path, pattern: str) -> list[Path]:
    if not tests_dir.is_dir():
        raise ValueError(f"Tests directory not found: {tests_dir}")
    sources = sorted(
        source for source in tests_dir.glob(pattern)
        if source.is_file() and source.suffix == ".8o"
    )
    if not sources:
        raise ValueError(f"No .8o fixtures match {pattern!r} in {tests_dir}")
    for source in sources:
        references = [
            source.with_suffix(suffix) for suffix in (".ch8", ".err")
            if source.with_suffix(suffix).is_file()
        ]
        if len(references) != 1:
            raise ValueError(
                f"{source.name} must have exactly one .ch8 or .err reference."
            )
        if references[0].suffix == ".err":
            expression = references[0].read_text(encoding="utf-8").strip()
            if not expression:
                raise ValueError(f"Empty error expectation: {references[0]}")
            try:
                re.compile(expression, re.MULTILINE)
            except re.error as error:
                raise ValueError(
                    f"Invalid diagnostic regex in {references[0]}: {error}"
                ) from error
    return sources


def binary_difference(expected: bytes, actual: bytes) -> str:
    offset = next(
        (index for index, pair in enumerate(zip(expected, actual))
         if pair[0] != pair[1]),
        min(len(expected), len(actual)),
    )
    start = max(0, offset - 4)
    end = offset + 12
    return (
        f"binary differs at offset 0x{offset:X}; "
        f"expected {len(expected)} bytes, got {len(actual)}\n"
        f"  expected: {expected[start:end].hex(' ')}\n"
        f"  actual:   {actual[start:end].hex(' ')}"
    )


def check_fixture(
    source: Path, output: Path, result: subprocess.CompletedProcess[str]
) -> str | None:
    error_reference = source.with_suffix(".err")
    diagnostics = result.stdout + "\n" + result.stderr
    if error_reference.is_file():
        # Crashes must not pass negative tests, even if their output matches.
        if result.returncode <= 0:
            return (
                f"expected an error exit, got return code {result.returncode}"
            )
        if "panic:" in diagnostics or "Internal error:" in diagnostics:
            return "compiler crashed instead of reporting the expected error"
        expression = error_reference.read_text(encoding="utf-8").strip()
        if re.search(expression, diagnostics, re.MULTILINE) is None:
            return f"diagnostic does not match {expression!r}"
        return None
    if result.returncode != 0:
        return (
            "expected successful compilation, "
            f"got return code {result.returncode}"
        )
    if not output.is_file():
        return "compiler returned success without writing its binary output"
    expected = source.with_suffix(".ch8").read_bytes()
    actual = output.read_bytes()
    return None if expected == actual else binary_difference(expected, actual)


def positive_timeout(value: str) -> float:
    seconds = float(value)
    if not math.isfinite(seconds) or seconds <= 0:
        raise argparse.ArgumentTypeError(
            "timeout must be a positive finite number"
        )
    return seconds


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    backend = parser.add_mutually_exclusive_group()
    backend.add_argument("--chiplet", help="path to the Chiplet executable")
    backend.add_argument(
        "--octopus2", action="store_true",
        help="build and run Octopus2 from ../chipcode/octopus2",
    )
    parser.add_argument("--tests-dir", type=Path, default=DEFAULT_TESTS_DIR)
    parser.add_argument(
        "--filter", default="*.8o", help="fixture filename glob"
    )
    parser.add_argument("--timeout", type=positive_timeout, default=10.0)
    parser.add_argument(
        "--build-timeout", type=positive_timeout, default=120.0
    )
    args = parser.parse_args(argv)
    try:
        sources = load_fixtures(args.tests_dir.resolve(), args.filter)
        with tempfile.TemporaryDirectory(
            prefix="chiplet-preprocessor-"
        ) as directory:
            work_dir = Path(directory)
            executable = (
                build_octopus2(work_dir, args.build_timeout)
                if args.octopus2 else find_chiplet(args.chiplet)
            )
            backend_name = "Octopus2" if args.octopus2 else "Chiplet"
            failures = 0
            for index, source in enumerate(sources):
                # Fresh output paths prevent stale binaries from passing.
                case_dir = work_dir / str(index)
                case_dir.mkdir()
                output = case_dir / "actual.ch8"
                if args.octopus2:
                    command = [
                        str(executable), "-no-color", "-i", str(source),
                        "-o", str(output),
                    ]
                else:
                    command = [
                        str(executable), "-q", "--no-line-info",
                        "-o", str(output), str(source),
                    ]
                try:
                    result = run_command(command, args.timeout)
                except subprocess.TimeoutExpired:
                    failures += 1
                    print(
                        f"FAIL {source.name}: "
                        f"timed out after {args.timeout:g}s"
                    )
                    continue
                problem = check_fixture(source, output, result)
                if problem is None:
                    print(f"PASS {source.name}")
                else:
                    failures += 1
                    print(f"FAIL {source.name}: {problem}")
                    diagnostics = (result.stdout + result.stderr).strip()
                    if diagnostics:
                        print(diagnostics[:3000])
            print(
                f"{backend_name}: "
                f"{len(sources) - failures}/{len(sources)} passed"
                f", {failures} failed"
            )
            return 1 if failures else 0
    except subprocess.TimeoutExpired:
        print("ERROR: Octopus2 build timed out.", file=sys.stderr)
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
