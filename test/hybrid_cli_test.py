#!/usr/bin/env python3
"""Exercise opaque native blocks through Chiplet's actual CLI and preprocessor."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class HybridCliTests(unittest.TestCase):
    chiplet: Path

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="chiplet-hybrid-")
        self.directory = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def run_chiplet(self, source, *options):
        input_path = self.directory / "main.8o"
        input_path.write_text(source, encoding="utf-8")
        return subprocess.run(
            [str(self.chiplet), *options, str(input_path)],
            capture_output=True, text=True, timeout=10, check=False,
        )

    def test_preprocessor_preserves_native_payload(self):
        output = self.directory / "preprocessed.8o"
        result = self.run_chiplet(
            ": main\n:asm cdp1802 {\nLDI #39; DB T'{}#' .. } comment\nSEP R4\n}\n",
            "-P", "-o", str(output),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        text = output.read_text(encoding="utf-8")
        self.assertIn("LDI #39; DB T'{}#' .. } comment", text)
        self.assertIn("SEP R4", text)

    def test_native_block_in_include_resolves_a_later_parent_label(self):
        (self.directory / "native.8o").write_text(
            ": helper\n:asm cdp1802 { LDI A.1(data); LDI A.0(data); SEP R4 }\n",
            encoding="utf-8",
        )
        output = self.directory / "main.ch8"
        result = self.run_chiplet(
            ': main\nnative helper\n:include "native.8o"\n: data 0x42\n',
            "-o", str(output),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes(), bytes.fromhex("02 02 F8 02 F8 07 D4 42"))

    def test_deferred_native_error_points_into_the_include(self):
        include = self.directory / "native.8o"
        include.write_text(
            ": helper\n:asm cdp1802 {\nNOP\nLDI missing\n}\n", encoding="utf-8",
        )
        result = self.run_chiplet(': main\nnative helper\n:include "native.8o"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(f"{include}:4:5:", result.stderr)
        self.assertIn("Undefined symbol 'missing'", result.stderr)

    def test_inactive_native_block_is_not_assembled(self):
        output = self.directory / "main.ch8"
        result = self.run_chiplet(
            ": main\n:if MISSING\n:asm m6800 { intentionally unsupported }\n:end\nclear\n",
            "-o", str(output),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes(), bytes.fromhex("00 E0"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chiplet", type=Path, required=True)
    options, unittest_arguments = parser.parse_known_args()
    HybridCliTests.chiplet = options.chiplet.resolve()
    unittest.main(argv=[__file__, *unittest_arguments])
