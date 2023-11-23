#!/usr/bin/env python3
from __future__ import annotations
import argparse
import filecmp
import re
import subprocess
import sys
from pathlib import Path
from collections.abc import Sequence


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    # Add arguments here
    parser.add_argument('assembler')
    args = parser.parse_args(argv)

    # Implement behaviour here
    for source_file in Path(__file__).parent.absolute().joinpath('c-octo-tests').glob('*.8o'):
        print(f"Testing '{source_file.name}'")
        result = subprocess.run([args.assembler, '-o', 'temp.ch8', '--no-line-info', source_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if source_file.with_suffix('.ch8').exists():
            # positive test
            if result.stderr:
                print(f"Error while assembling '{source_file}': {result.stderr}")
                return result.returncode
            elif not filecmp.cmp(source_file.with_suffix('.ch8'), 'temp.ch8'):
                print(f"Reference binary doesn't match for '{source_file}'")
                return -1
            elif result.returncode != 0:
                print(f"Return code not null for '{source_file}'!")
                return result.returncode
        elif source_file.with_suffix('.err').exists():
            # negative test
            if not result.stderr:
                print(f"No error output for '{source_file}'!")
                return -1
            with source_file.with_suffix('.err').open('r') as ref_err:
                expected_msg = ref_err.read().rstrip()
            expected = re.match(r"\((\d+):(\d+)\) \s*(.*)", expected_msg)
            test_msg = result.stderr.decode('utf-8').rstrip()
            generated = re.match(r".*?:(\d+):(\d+):\s*(.*)", test_msg)
            if not expected:
                print(f"Couldn't parse expected output for '{source_file}!\nreference: {expected_msg}")
                return -1
            if not generated:
                print(f"Couldn't parse generated output for '{source_file}!\ngenerated: {test_msg}")
                return -1
            if expected.group(1) != generated.group(1):
                print(f"Error line differs for '{source_file}'!\nreference: {expected_msg}\ngenerated: {test_msg}")
                return -1
            if expected.group(2) != generated.group(2):
                print(f"Warning: Error column differs for '{source_file}'!\nreference: {expected_msg}\ngenerated: {test_msg}")
            if expected.group(3) != generated.group(3):
                print(f"Error message differs for '{source_file}':\n  expected: {expected.group(3)}\n  got:      {generated.group(3)}")
                return -1
            if result.returncode == 0:
                print(f"Returncode was 0 for '{source_file}'!")
                #return -1
        else:
            print(f"no reference file found for test {source_file}!", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())