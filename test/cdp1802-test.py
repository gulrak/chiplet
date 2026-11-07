#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path


def normalize_text(data: bytes) -> str:
    return data.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")


def run_chiplet(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def compare_binary(expected: Path, actual: Path) -> bool:
    if expected.read_bytes() == actual.read_bytes():
        return True
    print(f"Binary output differs for {expected.name}", file=sys.stderr)
    return False


def compare_text(expected: Path, actual: Path) -> bool:
    if normalize_text(expected.read_bytes()) == normalize_text(actual.read_bytes()):
        return True
    print(f"Text output differs for {expected.name}", file=sys.stderr)
    return False


def check_run(result: subprocess.CompletedProcess[bytes], description: str) -> bool:
    if result.returncode == 0:
        return True
    print(f"{description} failed with return code {result.returncode}", file=sys.stderr)
    if result.stdout:
        print(result.stdout.decode("utf-8", errors="replace"), file=sys.stderr)
    if result.stderr:
        print(result.stderr.decode("utf-8", errors="replace"), file=sys.stderr)
    return False


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--chiplet", required=True)
    parser.add_argument("--tests-dir", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args(argv)

    chiplet = Path(args.chiplet)
    tests_dir = Path(args.tests_dir)
    work_dir = Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    ok = True
    for source in sorted(tests_dir.glob("*.asm")):
        name = source.stem
        generated_bin = work_dir / f"{name}.bin"
        generated_hex = work_dir / f"{name}.hex"
        generated_lst = work_dir / f"{name}.lst"

        print(f"Testing CDP1802 fixture '{source.name}'")

        result = run_chiplet([
            str(chiplet),
            "-q",
            "-a",
            "cdp1802",
            "-l",
            "-o",
            str(generated_bin),
            str(source),
        ])
        if not check_run(result, f"Binary/listing generation for {source.name}"):
            ok = False
            continue

        result = run_chiplet([
            str(chiplet),
            "-q",
            "-a",
            "cdp1802",
            "--hex",
            "-o",
            str(generated_hex),
            str(source),
        ])
        if not check_run(result, f"HEX generation for {source.name}"):
            ok = False
            continue

        ok = compare_binary(tests_dir / f"{name}.bin", generated_bin) and ok
        ok = compare_text(tests_dir / f"{name}.lst", generated_lst) and ok
        ok = compare_text(tests_dir / f"{name}.hex", generated_hex) and ok

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
