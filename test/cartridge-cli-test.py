#!/usr/bin/env python3
"""CLI cartridge checks, reading the results with an independent GIF decoder."""

import argparse
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CHIPLET = None
_SPEC = importlib.util.spec_from_file_location('octocart', Path(__file__).resolve().parents[1] / 'tools' / 'octocart.py')
octocart = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(octocart)
CD48_COLOR_0 = 0x9dc3a3
OCTO_COLOR_1 = 0x806650
DATA_MASK = 0xFEFCFE
PROGRAM = ': main\n  loop again\n'


def unpack_lzw(min_code, data):
    """Plain LZW as described by the GIF specification, expects an end code."""
    clear = 1 << min_code
    size, next_code, old = min_code + 1, clear + 2, None
    table = {}
    position = 0
    result = bytearray()
    accumulator = available = 0
    while True:
        while available < size:
            if position >= len(data):
                raise ValueError('stream ended without an end code')
            accumulator |= data[position] << available
            available += 8
            position += 1
        code = accumulator & ((1 << size) - 1)
        accumulator >>= size
        available -= size
        if code == clear:
            size, next_code, old = min_code + 1, clear + 2, None
            table = {}
            continue
        if code == clear + 1:
            return bytes(result)
        entry = bytes([code]) if code < clear else table.get(code)
        if entry is None:
            if old is None or code != next_code:
                raise ValueError(f'invalid code {code}, next is {next_code}')
            entry = old + old[:1]
        result += entry
        if old is not None and next_code < 4096:
            table[next_code] = old + entry[:1]
            next_code += 1
            if next_code == 1 << size and size < 12:
                size += 1
        old = entry


def read_sub_blocks(gif, position):
    data = bytearray()
    while gif[position]:
        data += gif[position + 1:position + 1 + gif[position]]
        position += gif[position] + 1
    return bytes(data), position + 1


def decode_gif(path):
    gif = path.read_bytes()
    assert gif[:6] in (b'GIF87a', b'GIF89a')
    width = int.from_bytes(gif[6:8], 'little')
    height = int.from_bytes(gif[8:10], 'little')
    packed = gif[10]
    position = 13
    palette = []
    if packed & 0x80:
        count = 1 << ((packed & 7) + 1)
        table = gif[position:position + count * 3]
        palette = [int.from_bytes(table[i:i + 3], 'big') for i in range(0, count * 3, 3)]
        position += count * 3
    frames = []
    while gif[position] != 0x3B:
        if gif[position] == 0x21:
            _, position = read_sub_blocks(gif, position + 2)
        else:
            assert gif[position] == 0x2C
            assert gif[position + 9] & 0x80 == 0
            min_code = gif[position + 10]
            data, position = read_sub_blocks(gif, position + 11)
            frames.append(unpack_lzw(min_code, data))
    return width, height, palette, frames


def read_cart(path):
    width, height, palette, frames = decode_gif(path)
    nibbles = bytearray()
    for frame in frames:
        assert len(frame) == width * height
        for pixel in frame:
            color = palette[pixel]
            nibbles.append(((color >> 13) & 8) | ((color >> 7) & 6) | (color & 1))
    payload = bytes((nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles), 2))
    size = int.from_bytes(payload[:4], 'big')
    return palette, frames, json.loads(payload[4:4 + size].decode('ascii'))


def octo_style_cart(path, payload, local_palette=False):
    """Writes a cartridge the way Octo does: without compression, payload bytes unchanged."""
    width, height = 160, 128
    data = len(payload).to_bytes(4, 'big') + payload
    table = b''.join(((n & 8) << 13 | (n & 6) << 7 | (n & 1)).to_bytes(3, 'big') for n in range(16))
    out = bytearray(b'GIF89a')
    out += width.to_bytes(2, 'little') + height.to_bytes(2, 'little')
    out += bytes([0x70, 0, 0]) if local_palette else bytes([0xF3, 0, 0]) + table
    per_frame = width * height // 2
    for start in range(0, len(data), per_frame):
        chunk = data[start:start + per_frame].ljust(per_frame, b'\0')
        pixels = bytes(n for b in chunk for n in (b >> 4, b & 15))
        out += b'\x2C' + bytes(4) + width.to_bytes(2, 'little') + height.to_bytes(2, 'little')
        out += bytes([0x83]) + table + bytes([7]) if local_palette else bytes([0, 7])
        for offset in range(0, len(pixels), 64):
            block = pixels[offset:offset + 64]
            out += bytes([len(block) + 1, 0x80]) + block
        out += b'\0'
    path.write_bytes(bytes(out + b'\x3B'))


def cropped_cart(path, payload):
    """Like Octo, but a frame after the first only holds the rows that changed."""
    width, height = 160, 128
    data = len(payload).to_bytes(4, 'big') + payload
    table = b''.join(((n & 8) << 13 | (n & 6) << 7 | (n & 1)).to_bytes(3, 'big') for n in range(16))
    out = bytearray(b'GIF89a') + width.to_bytes(2, 'little') + height.to_bytes(2, 'little') + bytes([0xF3, 0, 0]) + table
    per_frame = width * height // 2
    previous = None
    for start in range(0, len(data), per_frame):
        chunk = data[start:start + per_frame].ljust(per_frame, b'\0')
        pixels = bytes(n for b in chunk for n in (b >> 4, b & 15))
        top, rows = 0, height
        if previous is not None:
            changed = [y for y in range(height) if pixels[y * width:(y + 1) * width] != previous[y * width:(y + 1) * width]]
            if changed:
                top, rows = changed[0], changed[-1] - changed[0] + 1
            else:
                top, rows = 0, 1
        previous = pixels
        part = pixels[top * width:(top + rows) * width]
        out += b'\x2C\0\0' + top.to_bytes(2, 'little') + width.to_bytes(2, 'little') + rows.to_bytes(2, 'little') + bytes([0, 7])
        for offset in range(0, len(part), 64):
            block = part[offset:offset + 64]
            out += bytes([len(block) + 1, 0x80]) + block
        out += b'\0'
    path.write_bytes(bytes(out + b'\x3B'))


class CartridgeCli(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.dir = Path(self._dir.name)
        self.source = self.dir / 'test.8o'
        self.source.write_text(PROGRAM)
        self.cart = self.dir / 'test.gif'

    def tearDown(self):
        self._dir.cleanup()

    def chiplet(self, *arguments):
        return subprocess.run([CHIPLET, '-q', *map(str, arguments)], capture_output=True, text=True, check=False)

    def build(self, *arguments):
        result = self.chiplet('--cartridge-label', 'TEST', '-o', self.cart, *arguments, self.source)
        self.assertEqual(result.returncode, 0, result.stderr)

    def assemble(self, source, name):
        target = self.dir / name
        result = self.chiplet('-o', target, source)
        self.assertEqual(result.returncode, 0, result.stderr)
        return target.read_bytes()

    def test_default_base_is_cd48(self):
        self.build()
        palette, _, _ = read_cart(self.cart)
        self.assertEqual(len(palette), 64)
        self.assertEqual(palette[0], CD48_COLOR_0 & DATA_MASK)

    def test_original_cart_uses_octo_image(self):
        self.build('--original-cart')
        palette, _, _ = read_cart(self.cart)
        self.assertEqual(len(palette), 128)
        self.assertEqual(palette[16], OCTO_COLOR_1 & DATA_MASK)

    def test_label_is_visible(self):
        for arguments, ink in ((), 2), (('--original-cart',), 1):
            self.build(*arguments)
            _, frames, _ = read_cart(self.cart)
            area = [frames[0][y * 160 + x] >> 4 for y in range(32, 40) for x in range(16, 40)]
            self.assertIn(ink, area)

    def test_multi_frame_program_assembles_identically(self):
        lines = [PROGRAM]
        lines += [f'  0x{i & 255:02x} 0x{(i * 7) & 255:02x} 0x{(i * 13) & 255:02x}\n' for i in range(3000)]
        self.source.write_text(''.join(lines))
        for arguments in (), ('--original-cart',):
            self.build(*arguments)
            _, frames, content = read_cart(self.cart)
            self.assertGreater(len(frames), 1)
            extracted = self.dir / 'extracted.8o'
            extracted.write_text(content['program'])
            self.assertEqual(self.assemble(extracted, 'a.ch8'), self.assemble(self.source, 'b.ch8'))

    def test_hex_stores_only_the_assembled_program(self):
        self.source.write_text('# SECRET COMMENT\n: main\n  secret_label\n  loop again\n: secret_label\n  v0 := 1\n  return\n')
        expected = self.assemble(self.source, 'expected.ch8')
        for arguments in (), ('--original-cart',):
            self.build('--hex', *arguments)
            _, _, content = read_cart(self.cart)
            program = content['program']
            self.assertTrue(program.startswith(': main\n0x'))
            self.assertNotIn('SECRET', program)
            self.assertNotIn('secret_label', program)
            bytes_in_cart = bytes(int(token, 16) for token in program.split()[2:])
            self.assertEqual(bytes_in_cart, expected)
            extracted = self.dir / 'extracted.8o'
            extracted.write_text(program)
            self.assertEqual(self.assemble(extracted, 'a.ch8'), expected)

    def test_hex_multi_frame(self):
        lines = [PROGRAM]
        lines += [f'  0x{i & 255:02x} 0x{(i * 7) & 255:02x}\n' for i in range(2500)]
        self.source.write_text(''.join(lines))
        self.build('--hex')
        _, frames, content = read_cart(self.cart)
        self.assertGreater(len(frames), 1)
        expected = self.assemble(self.source, 'expected.ch8')
        self.assertEqual(bytes(int(token, 16) for token in content['program'].split()[2:]), expected)

    def test_hex_needs_a_program_that_assembles(self):
        self.source.write_text(': main\n  jump nowhere\n')
        result = self.chiplet('--cartridge-label', 'X', '--hex', '-o', self.cart, self.source)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.cart.exists())
        self.build()  # as source it doesn't have to

    def test_cli_reads_its_own_cart(self):
        self.build()
        result = self.chiplet(self.cart)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), read_cart(self.cart)[2])

    def test_variant_sets_options(self):
        self.build('--cartridge-variant', 'xo-chip')
        self.assertEqual(read_cart(self.cart)[2]['options']['maxSize'], 65024)

    def test_colors_are_plain_rgb(self):
        self.build()
        options = read_cart(self.cart)[2]['options']
        self.assertEqual(options['backgroundColor'], '#996600')
        self.assertEqual(options['fillColor'], '#ffcc00')

    def test_palette_from_options_file(self):
        palette = [f'#{i * 16:02x}{i * 16:02x}{i * 16 + 1:02x}' for i in range(16)]
        options = self.dir / 'options.json'
        options.write_text(json.dumps({'tickrate': 1000, 'fillColor': '#ffffff', 'palette': palette}))
        self.build('--cartridge-options', options)
        written = read_cart(self.cart)[2]['options']
        self.assertEqual(written['palette'], palette)
        # the four colors Octo knows come from the palette
        self.assertEqual([written[name] for name in ('backgroundColor', 'fillColor', 'fillColor2', 'blendColor')],
                         palette[:4])
        content = octocart.read_cartridge(self.cart.read_bytes())
        self.assertEqual(octocart.normalized_options(content)['palette'], palette)

    def test_no_palette_without_one(self):
        self.build()
        self.assertNotIn('palette', read_cart(self.cart)[2]['options'])
        self.assertNotIn('palette', octocart.normalized_options(octocart.read_cartridge(self.cart.read_bytes())))

    def test_octo_written_latin1_is_read(self):
        text = json.dumps({'program': '# Schümann\n' + PROGRAM, 'options': {}}, ensure_ascii=False)
        octo_style_cart(self.cart, text.encode('latin-1'))
        result = subprocess.run([CHIPLET, '-q', str(self.cart)], capture_output=True, check=False)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout.decode('utf-8'))['program'], '# Schümann\n' + PROGRAM)

    def test_local_palette_cart_is_read(self):
        program = PROGRAM + ''.join(f'  0x{i & 255:02x}\n' for i in range(4000))
        octo_style_cart(self.cart, json.dumps({'program': program, 'options': {}}).encode('ascii'), local_palette=True)
        result = self.chiplet(self.cart)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['program'], program)

    def test_reference_decoder_reads_both_kinds_of_program(self):
        self.source.write_text(PROGRAM + ''.join(f'  0x{i & 255:02x}\n' for i in range(20000)))
        self.build('--hex', '--cartridge-variant', 'chip-8')
        content = octocart.read_cartridge(self.cart.read_bytes())
        self.assertEqual(octocart.binary_program(content['program']), self.assemble(self.source, 'expected.ch8'))
        options = octocart.normalized_options(content)
        self.assertEqual(options['maxSize'], 3232)
        self.assertTrue(options['vBlankQuirks'])
        self.assertFalse(options['shiftQuirks'])
        self.build()
        self.assertIsNone(octocart.binary_program(octocart.read_cartridge(self.cart.read_bytes())['program']))

    def test_reference_decoder_and_cli_agree_on_other_writers(self):
        block = '#' + 'x' * 10237 + '\n'
        program = PROGRAM + block * 3
        payload = json.dumps({'program': program, 'options': {'shiftQuirks': 1, 'maxSize': '65024'}}).encode('ascii')
        for name, write in (('plain', octo_style_cart), ('local', lambda p, d: octo_style_cart(p, d, local_palette=True)), ('cropped', cropped_cart)):
            with self.subTest(writer=name):
                write(self.cart, payload)
                content = octocart.read_cartridge(self.cart.read_bytes())
                self.assertEqual(content['program'], program)
                options = octocart.normalized_options(content)
                self.assertIs(options['shiftQuirks'], True)
                self.assertEqual(options['maxSize'], 65024)
                result = self.chiplet(self.cart)
                self.assertEqual(json.loads(result.stdout)['program'], program)

    def test_reference_decoder_rejects_a_plain_gif(self):
        gif = bytes.fromhex('474946383961010001008000000000ffffff2c00000000010001000002024401003b')
        with self.assertRaises(octocart.CartridgeError):
            octocart.read_cartridge(gif)

    def test_options_file_keeps_defaults_for_missing_values(self):
        options = self.dir / 'options.json'
        options.write_text('{"tickrate": 1000, "fillColor": "#123456"}')
        self.build('--cartridge-options', options)
        content = read_cart(self.cart)[2]['options']
        self.assertEqual(content['tickrate'], 1000)
        self.assertEqual(content['fillColor'], '#123456')
        self.assertEqual(content['backgroundColor'], '#996600')

    def test_preprocess_to_stdout_still_works(self):
        result = self.chiplet('-P', self.source)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('loop again', result.stdout)

    def test_errors(self):
        cases = (
            ('--cartridge-label', 'X', self.source),
            ('--original-cart', '-o', self.cart, self.source),
            ('--cartridge-variant', 'octo', '-o', self.cart, self.source),
            ('--cartridge-image', 'x.png', '-o', self.cart, self.source),
            ('--cartridge-label', 'X', '--cartridge-variant', 'nonsense', '-o', self.cart, self.source),
            ('--cartridge-label', 'X', '--hex', '--start-address', '768', '-o', self.cart, self.source),
        )
        for arguments in cases:
            with self.subTest(arguments=arguments):
                result = self.chiplet(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('ERROR', result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--chiplet', required=True)
    args, remaining = parser.parse_known_args()
    CHIPLET = args.chiplet
    unittest.main(argv=[sys.argv[0], *remaining])
