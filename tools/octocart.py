#!/usr/bin/env python3
"""Reference decoder for Octo cartridges, see cartridge-format.md.

Standalone, no dependencies, written to be read and ported. Usage as a tool:

    octocart.py game.gif                   show options and kind of program
    octocart.py game.gif --rom game.ch8    write the bytes of a binary only one
    octocart.py game.gif --source game.8o  write the program text
"""

import argparse
import json
import re
import sys
from pathlib import Path

HEX_BYTE = re.compile(r'0x[0-9a-fA-F]{2}')

# The values Octo uses if a cartridge doesn't name an option.
DEFAULT_OPTIONS = {
    'tickrate': 20,
    'backgroundColor': '#996600',
    'fillColor': '#FFCC00',
    'fillColor2': '#FF6600',
    'blendColor': '#662200',
    'quietColor': '#000000',
    'buzzColor': '#FFAA00',
    'shiftQuirks': False,
    'loadStoreQuirks': False,
    'clipQuirks': False,
    'jumpQuirks': False,
    'logicQuirks': False,
    'vBlankQuirks': False,
    'screenRotation': 0,
    'maxSize': 3584,
    'touchInputMode': 'none',
    'fontStyle': 'octo',
}


class CartridgeError(Exception):
    """The file is no cartridge that can be read."""


def unpack_lzw(min_code, data):
    """GIF flavored LZW. A missing end code is fine, Octo doesn't write one."""
    if not 1 <= min_code <= 11:
        raise CartridgeError(f'invalid LZW code size {min_code}')
    clear = 1 << min_code
    size, next_code, previous = min_code + 1, clear + 2, None
    table = {}
    accumulator = available = position = 0
    result = bytearray()
    while True:
        while available < size:
            if position >= len(data):
                return bytes(result)
            accumulator |= data[position] << available
            available += 8
            position += 1
        code = accumulator & ((1 << size) - 1)
        accumulator >>= size
        available -= size
        if code == clear:
            size, next_code, previous = min_code + 1, clear + 2, None
            table = {}
            continue
        if code == clear + 1:
            return bytes(result)
        entry = bytes([code]) if code < clear else table.get(code)
        if entry is None:
            if previous is None or code != next_code:
                return bytes(result)
            entry = previous + previous[:1]
        result += entry
        if previous is not None and next_code < 4096:
            table[next_code] = previous + entry[:1]
            next_code += 1
            if next_code == 1 << size and size < 12:
                size += 1
        previous = entry


def read_sub_blocks(gif, position):
    """Returns the data of a sub block chain and the position after it."""
    data = bytearray()
    while gif[position]:
        length = gif[position]
        data += gif[position + 1:position + 1 + length]
        position += length + 1
    return bytes(data), position + 1


def read_color_table(gif, position, count):
    table = gif[position:position + count * 3]
    return [int.from_bytes(table[i:i + 3], 'big')
            for i in range(0, count * 3, 3)]


def decode_gif(gif):
    """Returns width, height and a list of (palette, pixels), one per frame.

    Every frame is complete: one that only covers a part of the image is put
    on top of the frame before it. Transparency doesn't matter, the pixels stay
    the color table indices they are.
    """
    if gif[:6] not in (b'GIF87a', b'GIF89a'):
        raise CartridgeError('not a GIF file')
    width = int.from_bytes(gif[6:8], 'little')
    height = int.from_bytes(gif[8:10], 'little')
    packed = gif[10]
    position = 13
    global_palette = None
    if packed & 0x80:
        count = 1 << ((packed & 7) + 1)
        global_palette = read_color_table(gif, position, count)
        position += count * 3
    frames = []
    previous = bytes(width * height)
    while position < len(gif):
        block = gif[position]
        position += 1
        if block == 0x3B:
            break
        if block == 0x21:
            _, position = read_sub_blocks(gif, position + 1)
        elif block == 0x2C:
            left, top, frame_width, frame_height = (
                int.from_bytes(gif[position + i:position + i + 2], 'little')
                for i in (0, 2, 4, 6))
            flags = gif[position + 8]
            position += 9
            if flags & 0x40:
                raise CartridgeError('interlaced frames are not supported')
            palette = global_palette
            if flags & 0x80:
                count = 1 << ((flags & 7) + 1)
                palette = read_color_table(gif, position, count)
                position += count * 3
            min_code = gif[position]
            data, position = read_sub_blocks(gif, position + 1)
            part = unpack_lzw(min_code, data)
            area = (left, top, frame_width, frame_height)
            whole = area == (0, 0, width, height)
            if whole and len(part) == width * height:
                pixels = part
            else:
                canvas = bytearray(previous)
                columns = min(frame_width, width - left)
                for y in range(min(frame_height, height - top)):
                    row = part[y * frame_width:y * frame_width + columns]
                    start = (top + y) * width + left
                    canvas[start:start + len(row)] = row
                pixels = bytes(canvas)
            frames.append((palette, pixels))
            previous = pixels
        else:
            raise CartridgeError(f'unknown GIF block type {block:#04x}')
    return width, height, frames


def nibble_of_color(color):
    """One bit of red, two of green and one of blue."""
    return ((color >> 13) & 8) | ((color >> 7) & 6) | (color & 1)


def extract_payload(gif):
    """Returns the bytes of the JSON text hidden in the frame colors."""
    _, _, frames = decode_gif(gif)
    nibbles = bytearray()
    for palette, pixels in frames:
        if not palette:
            raise CartridgeError('a frame has no color table')
        table = [nibble_of_color(color) for color in palette]
        try:
            nibbles += bytes(table[pixel] for pixel in pixels)
        except IndexError:
            raise CartridgeError('a pixel is not in its color table') from None
    data = bytes((nibbles[i] << 4) | nibbles[i + 1]
                 for i in range(0, len(nibbles) - 1, 2))
    size = int.from_bytes(data[:4], 'big')
    if len(data) < 4 or size + 4 > len(data):
        raise CartridgeError('the stored length does not fit, a plain GIF')
    return data[4:4 + size]


def read_cartridge(gif):
    """Returns the JSON object of a cartridge from the bytes of its file."""
    payload = extract_payload(gif)
    try:
        text = payload.decode('utf-8')
    except UnicodeDecodeError:
        text = payload.decode('latin-1')  # how Octo stores non ASCII text
    try:
        content = json.loads(text)
    except ValueError as error:
        raise CartridgeError(f'the payload is not JSON: {error}') from None
    program = content.get('program') if isinstance(content, dict) else None
    if not isinstance(program, str):
        raise CartridgeError('the payload has no program')
    return content


def normalized_options(content):
    """Returns all options, with defaults and the types they should have."""
    options = dict(DEFAULT_OPTIONS)
    options.update(content.get('options') or {})
    for name, default in DEFAULT_OPTIONS.items():
        if isinstance(default, bool):
            options[name] = bool(options[name])
        elif isinstance(default, int):
            try:
                options[name] = int(options[name])
            except (TypeError, ValueError):
                options[name] = default
    if options['screenRotation'] not in (0, 90, 180, 270):
        options['screenRotation'] = 0
    if options['maxSize'] == 3216:
        options['maxSize'] = 3232  # the value C-Octo uses for the same limit
    elif options['maxSize'] not in (3232, 3583, 3584, 65024):
        options['maxSize'] = 65024
    palette = options.get('palette')
    if isinstance(palette, list) and palette:
        # The addition for four planes: up to 16 colors, the first four win over the old fields.
        options['palette'] = [str(color) for color in palette[:16]]
        for name, color in zip(('backgroundColor', 'fillColor', 'fillColor2', 'blendColor'), options['palette']):
            options[name] = color
    else:
        options.pop('palette', None)
    return options


def binary_program(program):
    """Returns the bytes of a binary only program, None if it is source."""
    tokens = program.split()
    if tokens[:2] != [':', 'main']:
        return None
    if not all(HEX_BYTE.fullmatch(token) for token in tokens[2:]):
        return None
    return bytes(int(token, 16) for token in tokens[2:])


def main():
    parser = argparse.ArgumentParser(description='Read an Octo cartridge.')
    parser.add_argument('cartridge', type=Path)
    parser.add_argument('--rom', type=Path,
                        help='write the bytes of a binary only cartridge')
    parser.add_argument('--source', type=Path, help='write the program text')
    arguments = parser.parse_args()
    try:
        content = read_cartridge(arguments.cartridge.read_bytes())
    except (CartridgeError, IndexError, OSError) as error:
        print(f'ERROR: {arguments.cartridge}: {error}', file=sys.stderr)
        return 1
    rom = binary_program(content['program'])
    if rom is None:
        print('program: source, needs an assembler')
    else:
        print(f'program: binary only, {len(rom)} bytes')
    print(json.dumps(normalized_options(content), indent=2))
    if arguments.source:
        arguments.source.write_text(content['program'], encoding='utf-8')
    if arguments.rom:
        if rom is None:
            print('ERROR: This cartridge holds source code.', file=sys.stderr)
            return 1
        arguments.rom.write_bytes(rom)
    return 0


if __name__ == '__main__':
    sys.exit(main())
