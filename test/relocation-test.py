#!/usr/bin/env python3
"""CLI relocation checks with generated binary and sparse Intel HEX inputs."""

import argparse
import subprocess
import tempfile
import unittest
from pathlib import Path


def hex_record(address, data, kind=0):
    payload = bytes([len(data), address >> 8, address & 255, kind]) + data
    return ':' + payload.hex().upper() + f'{(-sum(payload)) & 255:02X}\n'


def read_hex(path):
    chunks = {}
    eof = False
    for line in path.read_text().splitlines():
        record = bytes.fromhex(line[1:])
        assert sum(record) % 256 == 0
        assert len(record) == record[0] + 5
        if record[3] == 1:
            eof = True
        else:
            address = int.from_bytes(record[1:3], 'big')
            for index, value in enumerate(record[4:-1]):
                assert address + index not in chunks
                chunks[address + index] = value
    assert eof
    return chunks


class RelocationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='chiplet-relocate-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.input = self.root / 'input.ch8'
        self.output = self.root / 'output.ch8'
        self.program = bytes.fromhex('1206 AAAA 1818 A204 220C 120A 00EE 1B')
        self.expected = bytes.fromhex('1606 AAAA 1818 A604 260C 160A 00EE 1B')
        self.input.write_bytes(self.program)

    def run_chiplet(self, *args, ok=True):
        result = subprocess.run([CHIPLET, '-q', *map(str, args)], capture_output=True, text=True, timeout=10)
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_binary_to_binary(self):
        self.run_chiplet('--relocate', '0x600', '-o', self.output, self.input)
        self.assertEqual(self.output.read_bytes(), self.expected)

    def test_binary_to_hex_and_back(self):
        target = self.root / 'port.HEX'
        self.run_chiplet('--relocate', '1536', '--hex', '-o', target, self.input)
        self.assertEqual(read_hex(target), dict(enumerate(self.expected, 0x600)))
        self.run_chiplet('--start-address', '0x600', '--relocate', '0x200', '-o', self.output, target)
        self.assertEqual(self.output.read_bytes(), self.program)

    def test_hex_to_hex(self):
        source = self.root / 'source.hex'
        source.write_text(hex_record(0x200, self.program) + hex_record(0, b'', 1))
        target = self.root / 'port.hex'
        self.run_chiplet('--relocate', '0x600', '--hex', '-o', target, source)
        self.assertEqual(read_hex(target), dict(enumerate(self.expected, 0x600)))

    def test_sparse_hex_preserves_ranges_and_binary_fills_gaps(self):
        source = self.root / 'source.hex'
        code = bytes.fromhex('A220 1202')
        sprite = bytes.fromhex('AAAA 1818 1B')
        source.write_text(hex_record(0x220, sprite) + hex_record(0x200, code) + hex_record(0, b'', 1))
        target = self.root / 'port.hex'
        self.run_chiplet('--relocate', '0x600', '--hex', '-o', target, source)
        expected = dict(enumerate(bytes.fromhex('A620 1602'), 0x600))
        expected.update(dict(enumerate(sprite, 0x620)))
        self.assertEqual(read_hex(target), expected)
        self.run_chiplet('--relocate', '0x600', '-o', self.output, source)
        self.assertEqual(self.output.read_bytes(), bytes.fromhex('A620 1602') + bytes(28) + sprite)

    def test_hex_prefix_before_entry(self):
        source = self.root / 'source.hex'
        source.write_text(hex_record(0x200, bytes.fromhex('1818 A200 1204')) + hex_record(0, b'', 1))
        target = self.root / 'port.hex'
        self.run_chiplet('--start-address', '0x202', '--relocate', '0x602', '--hex', '-o', target, source)
        self.assertEqual(read_hex(target), dict(enumerate(bytes.fromhex('1818 A600 1604'), 0x600)))

    def test_bad_inputs_do_not_write_output(self):
        self.run_chiplet('--relocate', '0x600', self.input, ok=False)
        self.run_chiplet('--relocate', '0x600', '-o', self.output, self.input, self.input, ok=False)
        self.run_chiplet('--relocate', '0x600', '-o', self.output, self.root, ok=False)
        self.run_chiplet('--relocate', '0x600', '-d', '-o', self.output, self.input, ok=False)
        self.run_chiplet('--relocate', '-1', '-o', self.output, self.input, ok=False)
        self.run_chiplet('--relocate', '0x10000', '-o', self.output, self.input, ok=False)
        self.assertFalse(self.output.exists())

    def test_overflow_is_rejected_without_truncating_output(self):
        self.input.write_bytes(bytes.fromhex('AFFF 1202'))
        self.output.write_bytes(b'keep')
        self.run_chiplet('--relocate', '0x600', '-o', self.output, self.input, ok=False)
        self.assertEqual(self.output.read_bytes(), b'keep')
        self.input.write_bytes(bytes.fromhex('1200'))
        self.run_chiplet('--relocate', '0xFFFF', '-o', self.output, self.input, ok=False)
        self.assertEqual(self.output.read_bytes(), b'keep')

    def test_empty_truncated_and_bad_hex_fail(self):
        for data in (b'', b'\x12', bytes.fromhex('F000 12')):
            self.input.write_bytes(data)
            self.run_chiplet('--relocate', '0x600', '-o', self.output, self.input, ok=False)
        source = self.root / 'source.hex'
        source.write_text(':020200001200FF\n:00000001FF\n')
        self.run_chiplet('--relocate', '0x600', '-o', self.output, source, ok=False)
        self.assertFalse(self.output.exists())

    def test_hex_requires_entry_and_rejects_executed_gaps(self):
        source = self.root / 'source.hex'
        source.write_text(hex_record(0x600, bytes.fromhex('1600')) + hex_record(0, b'', 1))
        self.run_chiplet('--relocate', '0x200', '-o', self.output, source, ok=False)
        source.write_text(hex_record(0x200, bytes.fromhex('1204')) + hex_record(0x206, b'\xAA\xAA') + hex_record(0, b'', 1))
        self.run_chiplet('--relocate', '0x600', '-o', self.output, source, ok=False)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--chiplet', required=True)
    arguments = parser.parse_args()
    CHIPLET = str(Path(arguments.chiplet).resolve())
    unittest.main(argv=['relocation-test.py'])
