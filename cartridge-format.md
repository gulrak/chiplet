# The Octo Cartridge Format

An Octo cartridge is a GIF file that carries a CHIP-8 program **together with
everything needed to run it correctly**: quirks, speed, colors, rotation and
font. It looks like a little picture of a cartridge and can be shared wherever
images can be shared.

The format comes from [Octo](https://github.com/JohnEarnest/Octo) by John
Earnest. Octo's code has been its only definition so far; this document
describes what it does, what real cartridges contain, and two additions that
stay fully compatible with it: [binary only cartridges](#binary-only-cartridges)
and a [palette of 16 colors](#sixteen-colors).

## Why this matters for emulators

The hard part of running a CHIP-8 program is rarely the interpreter, it is
knowing what the program expects. A `.ch8` file can't say which quirks it needs
or how fast it has to run. The
[CHIP-8 Program Database](https://github.com/chip-8/chip-8-database) helps with
known programs, but it can't know a program that was released yesterday.

A cartridge doesn't need a lookup: **the author ships the configuration with the
program**, and it can't get separated from it.

The traditional obstacle was that cartridges hold source code, so reading one
meant embedding an Octo assembler. [Binary only
cartridges](#binary-only-cartridges) remove that: supporting them takes a GIF
decoder and a JSON parser, no assembler.

## Overview

```
GIF file
 └ frames, each pixel looks up a color
    └ 4 data bits are in the low bits of each color
       └ 2 pixels give 1 byte, frames are simply concatenated
          └ 4 bytes big endian length, followed by that many bytes of JSON
             └ {"program": "...", "options": {...}}
```

## Reading a cartridge

### 1. Decode the GIF

Get the color of every pixel of every frame. A cartridge usually has a global
color table of 128 entries and frames that fill the whole image, but readers
should handle what is valid in a GIF, as cartridges may have passed through
other tools:

- A frame can have a local color table, which replaces the global one.
- A frame can cover only a part of the image. The rest comes from the frame
  before it.
- LZW data may be packed (Chiplet, Cadmium), or be nothing but literals with
  frequent clear codes (Octo). The end code may be missing, as Octo doesn't
  write one.
- Interlaced frames aren't supported by Octo and don't occur.

A GIF library is fine as long as it returns the **exact** colors of each frame
in order, without color management, gamma correction or premultiplied alpha. A
per-frame RGB read with Python's Pillow gave correct results for every cartridge
it was tested with.

### 2. Extract the bytes

The data is in the colors, so it doesn't matter which index a pixel uses. Each
pixel holds one nibble in four bits of its 24-bit color `0xRRGGBB`:

```c
nibble = ((color >> 13) & 8)  // lowest bit of red
       | ((color >>  7) & 6)  // lowest two bits of green
       | ( color        & 1); // lowest bit of blue
```

Pixels go left to right, top to bottom, the first of two pixels is the high
nibble of a byte. Frames follow each other without any gap or header. A frame of
the usual 160×128 pixels holds 10240 bytes.

### 3. Read the JSON

The first four bytes are the length of the JSON text, most significant byte
first. Anything after that many bytes is padding. **If the length doesn't fit
into the frames, this is a plain GIF and no cartridge**, which is what most GIF
files will show: check this before allocating memory, as the value can be huge.

Decode the text as UTF-8. If that fails, decode it as Latin-1: Octo writes one
byte per character, so a `»` arrives as the single byte `0xBB`. Characters
above 255 lose their upper bits when Octo writes them, nothing can restore those.

```json
{
  "program": ": main\n0x00 0xe0 0x12 0x02\n",
  "options": { "tickrate": 20, "shiftQuirks": false }
}
```

Only `program` is required. Ignore everything you don't know, real cartridges
also have fields like `key` and `created`.

## The options

Every option is optional, use the default if it is missing. Be tolerant with the
types, real cartridges do vary. Of 13 cartridges examined, flags were `true`
and `false` in 7 and `1` and `0` in 6, `maxSize` was a string in 4, and
`fontStyle` and `touchInputMode` were missing in some.

### Quirks

All of these default to `false`, which is Octo's and XO-CHIP's behavior.

| Option            | If true                                                                                          |
|-------------------|--------------------------------------------------------------------------------------------------|
| `shiftQuirks`     | `8xy6` and `8xyE` shift `vx` itself and ignore `vy`. Otherwise `vx = vy` shifted.                |
| `loadStoreQuirks` | `Fx55` and `Fx65` leave `i` unchanged. Otherwise `i += x + 1`.                                   |
| `jumpQuirks`      | `Bnnn` jumps to `nnn + vx`, with `x` being the high nibble of `nnn`. Otherwise `nnn + v0`.       |
| `logicQuirks`     | `8xy1`, `8xy2` and `8xy3` set `vf` to 0. Otherwise they leave it alone.                          |
| `clipQuirks`      | Sprites are clipped at the screen edge. Otherwise they wrap around.                              |
| `vBlankQuirks`    | A sprite draw ends the instructions of the current frame, as on an interpreter waiting for vblank. |

A start position outside of the screen always wraps, also with `clipQuirks`.
`vfOrderQuirks` is deprecated and can be ignored.

There is no field for the variant, the quirks are what counts. These are the
profiles of Octo's options dialog, also available with Chiplet's
`--cartridge-variant`:

| Variant  | shift | loadStore | jump | logic | clip | vBlank | `maxSize` |
|----------|-------|-----------|------|-------|------|--------|-----------|
| CHIP-8   |       |           |      | ✓     | ✓    | ✓      | 3232      |
| SCHIP    | ✓     | ✓         | ✓    |       | ✓    |        | 3583      |
| Octo     |       |           |      |       |      |        | 3584      |
| XO-CHIP  |       |           |      |       |      |        | 65024     |

### Everything else

| Option            | Default     | Meaning                                                                       |
|-------------------|-------------|-------------------------------------------------------------------------------|
| `tickrate`        | `20`        | Instructions per frame, at 60 frames per second.                              |
| `maxSize`         | `3584`      | Size limit for the program: 3232, 3583, 3584 or 65024. See below.             |
| `screenRotation`  | `0`         | Clockwise display rotation: 0, 90, 180 or 270. Treat other values as 0.       |
| `backgroundColor` | `#996600`   | Color 0, no plane set.                                                        |
| `fillColor`       | `#FFCC00`   | Color 1, first plane.                                                         |
| `fillColor2`      | `#FF6600`   | Color 2, second plane.                                                        |
| `blendColor`      | `#662200`   | Color 3, both planes.                                                         |
| `quietColor`      | `#000000`   | Around the display, while there is no sound.                                  |
| `buzzColor`       | `#FFAA00`   | Around the display, while there is sound.                                     |
| `palette`         | none        | Up to 16 colors for programs with four planes. See [Sixteen colors](#sixteen-colors). |
| `fontStyle`       | `octo`      | `octo`, `vip`, `dream6800`, `eti660`, `schip` or `fish`.                      |
| `touchInputMode`  | `none`      | `none`, `swipe`, `seg16`, `seg16fill`, `gamepad` or `vip`.                    |

- **Numbers may be strings:** `"65024"` occurs as well as `65024`.
- **`maxSize`** limits the assembled program, it is not the amount of memory:
  Octo always provides 64K of RAM and all XO-CHIP instructions. Treat unknown
  values as 65024. C-Octo writes 3216 for what is 3232 elsewhere.
- **Colors** are CSS colors. Writers should use `#rrggbb`, but names occur in real
  cartridges (`white`, `red`, `gray`...). C-Octo's `octo_cartridge.h` has a table
  of the names if you want them all, otherwise handle the common ones and fall
  back to the default.

A cartridge doesn't hold a start address, programs are always loaded at `0x200`.

### Sixteen colors

XO-CHIP programs can use four planes, but Octo's options only name the colors of
two. `palette` is a list of up to 16 colors: entry `n` is the color of a pixel
whose plane bits are `n`, plane 1 being the lowest bit.

```json
"palette": ["#000000", "#e8c870", "#2e8b2e", "#8cd850", "#9a5a28", "#ffffff", ...]
```

- Its first four entries win over `backgroundColor`, `fillColor`, `fillColor2`
  and `blendColor`. Writers put the same four colors there as well, so that
  readers without `palette` still show those right.
- Entries that are missing keep what the reader would use otherwise.
- `quietColor` and `buzzColor` are not part of it.

Octo ignores the field, like every reader has to ignore what it doesn't know.
This is an addition of Chiplet and Cadmium, which use it for a cartridge whose
program needs more than four colors.

## The two kinds of program

### Source cartridges

This is what Octo writes: `program` is Octo assembly language and needs an
assembler. Chiplet stores the preprocessed source, which resolves includes and
never needs other files.

### Binary only cartridges

`program` consists of a `: main` followed by nothing but the program's bytes as
two digit hex numbers:

```
: main
0x00 0xe0 0x61 0x01 0x60 0x08 0xa2 0x50 0xd0 0x1f 0x60 0x10 0xa2 0x5f 0xd0 0x1f
0x60 0x18 0xa2 0x6e 0xd0 0x1f 0x60 0x20 0xa2 0x7d 0xd0 0x1f
```

No assembler is needed for reading this, split it on whitespace:

```python
tokens = program.split()
if tokens[:2] == [":", "main"] and all(re.fullmatch("0x[0-9a-fA-F]{2}", t) for t in tokens[2:]):
    rom = bytes(int(t, 16) for t in tokens[2:])  # load at 0x200
```

It is still valid Octo source: `: main` at the very start emits no code and the
numbers assemble to these same bytes, so such a cartridge also works in Octo and
in everything else that can assemble cartridges. This form is an addition of
Chiplet and Cadmium.

## Minimal support in an emulator

1. Decode the frames, extract the payload, parse the JSON (steps above).
2. If `program` is binary only, load the bytes at `0x200`. If it isn't and you
   have no assembler, tell the user this cartridge needs one.
3. Apply `tickrate` and the six quirks. These decide whether a program runs
   correctly at all.
4. Apply the colors, `palette` and `screenRotation` if you can, they are what the
   author wants the program to look like.

[`tools/octocart.py`](tools/octocart.py) is a standalone reference decoder in
Python without any dependencies, meant to be read and ported. It handles
everything listed here and was compared to Octo's own reader on the cartridges
available during its creation.

```
octocart.py game.gif --rom game.ch8
```

## Creating cartridges

With Chiplet, the label text also creates the picture:

```
chiplet --cartridge-label "MY GAME" --cartridge-variant xo-chip -o game.gif game.8o
```

| Option                     | Effect                                                                           |
|----------------------------|----------------------------------------------------------------------------------|
| `--hex`                    | Store a binary only cartridge instead of the source.                             |
| `--cartridge-variant <v>`  | Use the quirks of `chip-8`, `schip`, `octo` or `xo-chip`.                        |
| `--cartridge-options <f>`  | Read the options from a JSON file, as in the tables above. Missing ones keep their default. |
| `--original-cart`          | Use Octo's cartridge picture instead of the CD48 one Chiplet and Cadmium default to. |

A variant only sets quirks and `maxSize`, so `tickrate` and colors need
`--cartridge-options` if the defaults don't fit. In Cadmium, a cartridge saved
from the save page takes these from the current settings.

### Writing your own

- Take any picture of up to 16 colors, usually 160×128 pixels. For each of its
  colors, add 16 entries to the color table: the color with the data bits set to
  the entry's position (`(color & 0xFEFCFE) | ((n & 8) << 13) | ((n & 6) << 7) | (n & 1)`).
- Each pixel's index is `pictureColor * 16 + nibble`. This repeats the picture
  in all frames while the data changes.
- Use as many frames as needed and pad the last one with nibble 0.
- Write no transparent color, and use "do not dispose" as disposal method.
- LZW compression is optional. With up to 128 table entries, LZW code size 7 makes
  every code exactly 8 bits as long as a clear code (`0x80`) comes often enough.
  Octo sends one per 64 pixels, so each pixel simply is one byte.
- Prefer ASCII-only JSON and escape other characters as `\uXXXX`, then every
  reader gets the right text.

### Don't optimize the files

GIF optimizers can destroy the data, as nearby colors are not interchangeable
here. `gifsicle -O1` only crops frames and is safe, `gifsicle -O3` replaces pixels
with transparent ones and corrupted the program of a test cartridge, for Octo's
own reader as well. Reducing colors or converting the file breaks it for sure.
