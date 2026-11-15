#!/usr/bin/env python3
"""Checks that chiplet picks the assembler by the source when no --arch is given."""
from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

M6800_SOURCE = """; count down
START   LDAA    #$10
        STAA    $20
LOOP    DEC     $20
        BNE     LOOP
        RTS
"""

SATURN_SOURCE = """; copy a field
copy:   move.a  @d0,a
        add.a   5,d0
        move.a  a,@d1
        retclrc
"""

OCTO_SOURCE = """: main
    v0 := 1
    loop again
"""


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def report(result: subprocess.CompletedProcess[str], description: str) -> None:
    print(f"{description}: return code {result.returncode}", file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    print(result.stderr, file=sys.stderr)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--chiplet", required=True)
    parser.add_argument("--tests-dir", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args(argv)

    chiplet = args.chiplet
    tests_dir = Path(args.tests_dir)
    work_dir = Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)
    ok = True

    # CDP1802 sources assemble like with --arch cdp1802
    for source in sorted((tests_dir / "cdp1802-tests").glob("*.asm")):
        output = work_dir / f"{source.stem}.bin"
        result = run([chiplet, str(source), "-o", str(output)])
        if result.returncode != 0 or "CDP1802" not in result.stdout:
            report(result, f"auto assembly of {source.name}")
            ok = False
        elif output.read_bytes() != source.with_suffix(".bin").read_bytes():
            print(f"auto assembly of {source.name} differs from the reference binary", file=sys.stderr)
            ok = False

    # Octo sources stay Octo, also with another extension
    octo_asm = work_dir / "octo-program.asm"
    octo_asm.write_text(OCTO_SOURCE)
    result = run([chiplet, "-q", str(octo_asm), "-o", str(work_dir / "octo-program.ch8")])
    if result.returncode != 0:
        report(result, "auto assembly of an Octo source named .asm")
        ok = False

    # recognized sources without an assembler in Chiplet are refused with a clear message
    source = work_dir / "m6800.asm"
    source.write_text(M6800_SOURCE)
    result = run([chiplet, "-q", str(source), "-o", str(work_dir / "out.bin")])
    if result.returncode == 0 or "M6800" not in result.stderr:
        report(result, "auto assembly of m6800.asm")
        ok = False

    # Saturn sources are assembled with asacpp, by extension or by their statements, like with --arch saturn
    outputs = []
    for name, extra in (("saturn.asap", []), ("saturn.asm", []), ("forced.asm", ["--arch", "saturn"])):
        source = work_dir / name
        source.write_text(SATURN_SOURCE)
        output = work_dir / (name + ".bin")
        result = run([chiplet, "-q", *extra, str(source), "-o", str(output)])
        if result.returncode != 0 or not output.exists() or output.stat().st_size == 0:
            report(result, f"Saturn assembly of {name}")
            ok = False
        else:
            outputs.append(output.read_bytes())
    if len(outputs) == 3 and (outputs[0] != outputs[1] or outputs[0] != outputs[2]):
        print("Saturn assembly differs between the ways to choose it", file=sys.stderr)
        ok = False
    # an error names the file and line
    broken = work_dir / "broken.asap"
    broken.write_text("        move.a  @d0,a\n        frobnicate.a c\n")
    result = run([chiplet, "-q", str(broken), "-o", str(work_dir / "broken.bin")])
    if result.returncode == 0 or "broken.asap:2:" not in result.stderr:
        report(result, "Saturn assembly with an error")
        ok = False

    # -l writes a listing next to the output for Octo and Saturn too
    for name, text, expected in (("listed.8o", OCTO_SOURCE, "0200  60 01"), ("listed.asap", SATURN_SOURCE, "move.a  @d0,a")):
        source = work_dir / name
        source.write_text(text)
        output = work_dir / (Path(name).stem + Path(name).suffix.replace(".", "-") + ".bin")
        listing = output.with_suffix(".lst")
        listing.unlink(missing_ok=True)
        result = run([chiplet, "-q", "-l", str(source), "-o", str(output)])
        if result.returncode != 0 or not listing.exists() or expected not in listing.read_text():
            report(result, f"listing of {name}")
            ok = False

    # an explicit --arch still decides, and the other languages are named when asked for
    result = run([chiplet, "-q", "--arch", "octo", str(work_dir / "m6800.asm"), "-o", str(work_dir / "out.ch8")])
    if result.returncode == 0:
        report(result, "--arch octo on a 6800 source")
        ok = False
    result = run([chiplet, "-q", "--arch", "m6800", str(work_dir / "m6800.asm"), "-o", str(work_dir / "out.bin")])
    if result.returncode == 0 or "no assembler for M6800" not in result.stderr:
        report(result, "--arch m6800")
        ok = False

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
