#!/usr/bin/env python3

import argparse
import difflib
import pathlib
import subprocess
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class CheckSpec:
    name: str
    expected_suffix: str
    actual_suffix: str
    extra_args: tuple[str, ...]
    kind: str  # "text" or "binary"


def run_command(
        cmd: list[str],
        cwd: pathlib.Path | None = None,
) -> tuple[int, str, str]:
    result = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd is not None else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.returncode, result.stdout, result.stderr


def normalize_preprocessed_text(text: str) -> list[str]:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    return [line for line in text.split("\n") if line.strip() != ""]


def format_text_diff(expected_file: pathlib.Path, actual_file: pathlib.Path) -> str | None:
    expected_text = expected_file.read_text(encoding="utf-8", errors="replace")
    actual_text = actual_file.read_text(encoding="utf-8", errors="replace")

    expected_lines = normalize_preprocessed_text(expected_text)
    actual_lines = normalize_preprocessed_text(actual_text)

    if expected_lines == actual_lines:
        return None

    diff_lines = list(
        difflib.unified_diff(
            [line + "\n" for line in expected_lines],
            [line + "\n" for line in actual_lines],
            fromfile=str(expected_file),
            tofile=str(actual_file),
            n=3,
        )
    )

    if not diff_lines:
        return "normalized text differs, but no unified diff could be produced"

    max_diff_lines = 200
    if len(diff_lines) > max_diff_lines:
        shown = "".join(diff_lines[:max_diff_lines])
        return (
            f"unified diff of normalized text (truncated to {max_diff_lines} lines):\n"
            f"{shown}\n"
            f"... diff truncated ..."
        )

    return "unified diff of normalized text:\n" + "".join(diff_lines)

def format_binary_diff(expected_file: pathlib.Path, actual_file: pathlib.Path) -> str | None:
    expected_bytes = expected_file.read_bytes()
    actual_bytes = actual_file.read_bytes()

    if expected_bytes == actual_bytes:
        return None

    min_len = min(len(expected_bytes), len(actual_bytes))
    first_diff = None
    for i in range(min_len):
        if expected_bytes[i] != actual_bytes[i]:
            first_diff = i
            break

    if first_diff is None:
        first_diff = min_len

    context = 8
    start = max(0, first_diff - context)
    expected_end = min(len(expected_bytes), first_diff + context + 1)
    actual_end = min(len(actual_bytes), first_diff + context + 1)

    expected_slice = expected_bytes[start:expected_end]
    actual_slice = actual_bytes[start:actual_end]

    return (
        f"binary mismatch:\n"
        f"  expected size: {len(expected_bytes)} bytes\n"
        f"  actual size:   {len(actual_bytes)} bytes\n"
        f"  first differing byte offset: {first_diff}\n"
        f"  expected bytes[{start}:{expected_end}]: {expected_slice.hex(' ')}\n"
        f"  actual bytes[{start}:{actual_end}]:   {actual_slice.hex(' ')}"
    )


def format_diff(expected_file: pathlib.Path, actual_file: pathlib.Path, kind: str) -> str | None:
    if kind == "text":
        return format_text_diff(expected_file, actual_file)
    if kind == "binary":
        return format_binary_diff(expected_file, actual_file)
    raise ValueError(f"unknown comparison kind: {kind}")


def make_output_name(stem: str, suffix: str) -> str:
    return f"{stem}{suffix}"


def run_check(
        assembler: pathlib.Path,
        source_file: pathlib.Path,
        expected_file: pathlib.Path,
        work_dir: pathlib.Path,
        check: CheckSpec,
) -> str | None:
    if not expected_file.exists():
        return f"[{check.name}] missing expected output: {expected_file}"

    work_dir.mkdir(parents=True, exist_ok=True)

    assembler = assembler.resolve()
    source_file = source_file.resolve()
    expected_file = expected_file.resolve()

    actual_name = make_output_name(source_file.stem, check.actual_suffix)
    actual_file = (work_dir / actual_name).resolve()

    cmd = [
        str(assembler),
        str(source_file),
        *check.extra_args,
        "-o",
        actual_name,
    ]

    rc, stdout, stderr = run_command(cmd, cwd=work_dir)
    if rc != 0:
        return (
            f"[{check.name}] assembler failed for {source_file.name}\n"
            f"command: {' '.join(cmd)}\n"
            f"cwd: {work_dir}\n"
            f"exit code: {rc}\n"
            f"stdout:\n{stdout}\n"
            f"stderr:\n{stderr}"
        )

    if not actual_file.exists():
        return (
            f"[{check.name}] assembler reported success but did not create output\n"
            f"expected generated file: {actual_file}\n"
            f"command: {' '.join(cmd)}\n"
            f"cwd: {work_dir}"
        )

    diff = format_diff(expected_file, actual_file, check.kind)
    if diff is not None:
        return (
            f"[{check.name}] output mismatch for {source_file.name}\n"
            f"expected: {expected_file}\n"
            f"actual:   {actual_file}\n"
            f"{diff}"
        )

    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--assembler", required=True)
    ap.add_argument("--tests-dir", required=True)
    ap.add_argument("--bin-dir", required=True)
    ap.add_argument("--work-dir", required=True)
    args = ap.parse_args()

    assembler = pathlib.Path(args.assembler)
    tests_dir = pathlib.Path(args.tests_dir)
    bin_dir = pathlib.Path(args.bin_dir)
    work_dir = pathlib.Path(args.work_dir)

    checks = (
        CheckSpec(
            name="preprocess",
            expected_suffix=".8o",
            actual_suffix=".pp.8o",
            extra_args=("-P","--no-line-info","-q"),
            kind="text",
        ),
        CheckSpec(
            name="assemble",
            expected_suffix=".ch8",
            actual_suffix=".ch8",
            extra_args=("--no-line-info","-q",),
            kind="binary",
        ),
    )

    test_files = sorted(tests_dir.glob("*.8o"))
    if not test_files:
        print(f"No .8o files found in {tests_dir}", file=sys.stderr)
        return 1

    failures: list[str] = []
    executed = 0

    for source_file in test_files:
        stem = source_file.stem

        for check in checks:
            expected_file = bin_dir / f"{stem}{check.expected_suffix}"

            failure = run_check(
                assembler=assembler,
                source_file=source_file,
                expected_file=expected_file,
                work_dir=work_dir,
                check=check,
            )
            executed += 1

            if failure is not None:
                failures.append(failure)

    if failures:
        print(f"{len(failures)} of {executed} checks failed:\n", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}\n", file=sys.stderr)
        return 1

    print(f"All {executed} checks passed across {len(test_files)} source files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
