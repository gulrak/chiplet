#!/usr/bin/env python3
"""Check --octopus-whitespace on cases the CHIP-8 test suite doesn't have.

The expected texts are what Octopus2 produces, except where Octopus joins the
last code line and the first data line into one line.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

CASES = {
    # Octopus makes "loop again: thing" of this, the line end is kept.
    'glue': (
        {'glue.8o': ': main\n  loop again\n:segment data\n: thing\n  1 2\n'},
        ': main\n  loop again\n: thing\n  1 2\n',
    ),
    # The blank line where code and data meet is lost, like in Octopus.
    'seam': (
        {'seam.8o': ': main\n  loop again\n\n:segment data\n: thing\n  1\n'},
        ': main\n  loop again\n: thing\n  1\n',
    ),
    'data-blank': (
        {'data.8o': ': main\n  loop again\n:segment data\n\n: thing\n  1\n'},
        ': main\n  loop again\n: thing\n  1\n',
    ),
    # Chunks of a segment are joined without added blank lines, a file
    # without a final line end leaves no empty line (and Octopus would make
    # "jump main: a" of the last code and the first data line).
    'no-final-newline': (
        {'nofinal.8o': ': main\n  v0 := 1\n:segment data\n: a\n  1\n'
                       ':segment code\n  v1 := 2\n:segment data\n: b\n'
                       '  2\n:segment code\n  jump main'},
        ': main\n  v0 := 1\n  v1 := 2\n  jump main\n: a\n  1\n: b\n  2',
    ),
    # Every included file leaves an empty line in the segment it ends in, an
    # included file starts in the code segment.
    'nested': (
        {'nested.8o': ': main\n  helper\n  jump main\n:segment data\n'
                      ':include "sub/inc.8o"\n: more\n  1\n'
                      '  :include "sub/deep.8o"\n: tail\n  4\n',
         'sub/inc.8o': ': helper\n  return\n:segment data\n: hdata\n  7 8\n',
         'sub/deep.8o': ': deep\n  9\n'},
        ': main\n  helper\n  jump main\n: helper\n  return\n: deep\n  9\n'
        ': hdata\n  7 8\n\n: more\n  1\n: tail\n  4\n',
    ),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--chiplet', required=True)
    chiplet = Path(parser.parse_args().chiplet).resolve()
    failures = []
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        for ending_name, ending in (('lf', '\n'), ('crlf', '\r\n')):
            for name, (files, expected) in CASES.items():
                case = directory / ending_name / name
                for file_name, text in files.items():
                    (case / file_name).parent.mkdir(parents=True, exist_ok=True)
                    (case / file_name).write_bytes(
                        text.replace('\n', ending).encode('utf-8'))
                main_file = next(iter(files))
                result = subprocess.run(
                    [str(chiplet), '-P', '--octopus-whitespace', '-q', main_file,
                     '-o', 'out.8o'],
                    cwd=case, capture_output=True, text=True, timeout=30,
                )
                # Accept either platform's line endings, but expose stray CRs
                # and CRCRLF rather than hiding them with universal newlines.
                output_file = case / 'out.8o'
                actual = None
                if output_file.exists():
                    actual = output_file.read_bytes().decode('utf-8')
                    actual = actual.replace('\r\n', '\n')
                if result.returncode != 0 or actual != expected:
                    failures.append(f'{ending_name}/{name}: rc={result.returncode}\n'
                                    f'expected {expected!r}\nactual   {actual!r}\n'
                                    f'{result.stderr}')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'All {2 * len(CASES)} Octopus whitespace checks passed.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
