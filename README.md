# Chiplet v2

_A commandline multitool for CHIP-8 variant development_

<!-- TOC -->
* [Chiplet v2](#chiplet-v2)
  * [Overview](#overview)
  * [Command-line Options](#command-line-options)
  * [Usage Scenarios](#usage-scenarios)
    * [Preprocessing a File](#preprocessing-a-file)
    * [Assembling a File](#assembling-a-file)
    * [Disassembling a Binary](#disassembling-a-binary)
    * [Relocating a Program](#relocating-a-program)
    * [Repairing Emma ETI-660 HEX Files](#repairing-emma-eti-660-hex-files)
    * [Analyzing a Binary or a Directory](#analyzing-a-binary-or-a-directory)
    * [Finding Opcode Use](#finding-opcode-use)
  * [The Preprocessor Syntax](#the-preprocessor-syntax)
    * [Conditional Assembly](#conditional-assembly)
    * [Segments and Code Layout Management](#segments-and-code-layout-management)
    * [Inclusion of Files](#inclusion-of-files)
    * [Inclusion of Images](#inclusion-of-images)
    * [Inclusion of External Files](#inclusion-of-external-files)
    * [Inclusion of Sound Files](#inclusion-of-sound-files)
  * [Cartridges](#cartridges)
  * [Experimental Hybrid ROM Assembly](#experimental-hybrid-rom-assembly)
  * [Compiling from Source](#compiling-from-source)
    * [Linux / macOS](#linux--macos)
    * [Windows](#windows)
  * [Used Resources](#used-resources)
    * [Information](#information)
    * [Libraries](#libraries)
<!-- TOC -->

## Overview

Chiplet is the result of my work on [Cadmium](https://github.com/gulrak/cadmium),
my CHIP-8 emulator environment. It contains:

* an [Octo Assembly Language](http://johnearnest.github.io/Octo/docs/Manual.html)
  compatible assembler, written in C++ and originally based on code from
  [C-Octo](https://github.com/JohnEarnest/c-octo) by John Earnest,
* a preprocessor that is compatible with
  [Octopus](https://github.com/Timendus/chipcode/tree/main/octopus) from Tim Franssen,
* an execution path tracing disassembler, that can also relocate programs,
* a CDP1802 assembler, usable on its own and for native code inside of Octo
  programs (see [Experimental Hybrid ROM Assembly](#experimental-hybrid-rom-assembly)),
* and support for writing [Octo cartridges](#cartridges).

It combines these to allow using modular CHIP-8 projects with multiple
files, while retaining error messages that refer to the original files
and lines before the preprocessing step.

The disassembler also has options to search for specific opcode patterns and
give opcode statistics or estimate the CHIP-8 variants that could execute the
code.

## Command-line Options

```
USAGE: chiplet [options] ...
OPTIONS:

Assembler/Preprocessor:
  --cartridge-image <arg>
    generate an Octo compatible cartridge gif with the given image as label

  --cartridge-label <arg>
    generate an Octo compatible cartridge gif with the given text label

  --cartridge-options <arg>
    specifies a JSON file that contains the options to use for the cartridge

  --cartridge-variant <arg>
    specifies a CHIP-8 variant that will be used to set the options (chip-8, schip, octo, xo-chip)

  --[no-]hex
    write Intel HEX output instead of raw binary when compiling or relocating, with a cartridge output, store a hex dump of the assembled program instead of its source

  --no-line-info
    omit generation of line info comments in the preprocessed output

  --[no-]original-cart
    use the original Octo cartridge image for the generated gif instead of the CD48 one

  --start-address <arg>
    the address the program will be loaded to, the ': main' label address, default is 512

  -D <arg>, --define <arg>
    add a defined option to the preprocessor

  -I <arg>, --include-path <arg>
    add directory to include search path

  -P, --[no-]preprocess
    only preprocess the file and output the result

  -a <arg>, --arch <arg>
    select assembler architecture: octo or cdp1802, default octo

  -o <arg>, --output <arg>
    name of output file, default stdout for preprocessor, a.out.ch8 for binary

Disassembler/Analyzer:
  --[no-]deep-scan
    scan a directory tree for any files that look like CHIP-8 variant programs and list them, ignoring extensions

  --[no-]list-duplicates
    show found duplicates while scanning directories

  --relocate <arg>
    relocate one binary or HEX program to this address using execution path tracing

  --[no-]round-trip
    decompile and assemble and compare the result

  -d, --[no-]disassemble
    disassemble a given file

  -f <arg>, --find <arg>
    search for use of opcodes

  -l, --[no-]listing
    generate additional listing with addresses

  -p, --[no-]full-path
    print file names with path

  -s, --[no-]scan
    scan files or directories for chip roms and analyze them, giving some information

  -u, --[no-]opcode-use
    show usage of found opcodes when using -f

General:
  --[no-]version
    just shows version info and exits

  -q, --[no-]quiet
    suppress all output during operation

  -v, --[no-]verbose
    more verbose progress output

...
    Files or directories to work on
```

---

## Usage Scenarios

### Preprocessing a File

Running a file that uses preprocessor directives through the preprocessor
for later assembly by Octo:

```
chiplet -o output.8o -P octo-source-file.8o
```

If the output is not set, it will be written to stdout.

### Assembling a File

This runs the file through the preprocessor and the Octo assembler and generates
a binary program. 

```
chiplet -o output.ch8 octo-source-file.8o
```

If the output is not set, a file named `a.out.ch8` is generated.

### Disassembling a Binary

The Disassembler uses heuristic execution path tracing to detect data
and code parts and automatically generate labels to create an Octo assembly
language source that one can use to regenerate the binary.

Creating Octo source from a CHIP-8 variant binary:

```
chiplet -o output.8o -d some-program.ch8
```

If no output is set, the disassembly will be dumped to stdout.

**Note:** The as is the nature of heuristics, they are not perfect. In
rare cases the disassembler doesn't report an error but doesn't manage 
to generate source that is compilable or is identical to the binary.
In case of doubt, using the call with the option `--round-trip` will
disassemble into memory, run the result through the assembler and report
an error for any difference, so if that reports nothing, the source
generated by the disassembler is guaranteed to generate an identical
binary that was used as input, if assembled. A later version will make
this step implicit to ensure consistent disassembly. Starting with the
enhancements of v1.1.0 Chiplet is able to round-trip every program
file I could find that fits in 64k memory. (I currently only know of two
MegaChip examples that don't.)

### Relocating a Program

Relocate a working CHIP-8 binary from its original load address (default `0x200`)
using the disassembler's execution path tracing:

```
chiplet --relocate 0x600 -o program-eti.hex --hex program.ch8
chiplet --start-address 0x600 --relocate 0x200 -o program.ch8 program-eti.hex
```

Relocation requires exactly one binary or Intel HEX input file and an output
filename supplied with `-o`. `--hex` produces Intel HEX with relocated record
addresses; otherwise output is binary. HEX input retains its record addresses,
so set `--start-address` to the original entry address. Sparse HEX ranges remain
sparse in HEX output; binary output fills gaps with zero bytes and begins at the
relocated lowest input address.

Only address operands in traced instructions are changed; other bytes, including
sprite data that resembles instructions, are preserved. Addresses below the
lowest input address are left unchanged as interpreter/system references; higher
addresses move with the program, including references to scratch RAM beyond its
end. Operands that would overflow their instruction's address range cause an
error rather than being truncated. CHIP-8 addresses, XO-CHIP `i := long`, and
MegaChip `ldhi` operands are supported.

This is a best-effort relocation of a working program, not a repair of an already
corrupted conversion. Self-modifying code, addresses built in registers or stored
in data, computed jump targets and native machine code may need manual changes.
Detected computed jumps, native calls and writes to traced code produce warnings.

### Repairing Emma ETI-660 HEX Files

For the old Emma conversions that relocated every aligned word resembling an
address instruction, undo that transformation in Python, then use Chiplet's
path tracer to relocate the recovered binary:

```
python3 tools/repair-emma-hex.py broken.hex -o repaired.hex
python3 tools/repair-emma-hex.py broken.hex -o repaired.hex --reference original.ch8
```

The output is Intel HEX at the input's first address (normally `0x600`). The
original binary address defaults to `0x200`; `--start-address` and `--relocate`
override these addresses. The script finds a local native Chiplet build or uses
PATH; `--chiplet` selects an executable explicitly. It accepts contiguous HEX
and tolerates the converter's checksum `100` bug when the correct value is `00`.

Overflow in the faulty converter discarded a hex digit, which cannot be
recovered from the damaged word alone. Without a reference, the script warns
and guesses zero (`--lost-nibble` changes that guess). `--tail byte` or
`--tail word` resolves an ambiguous expanded final byte; automatic mode only
folds the unambiguous `0400` case. A `--reference` binary resolves both forms of
information loss, and is accepted only if the faulty converter reproduces the
complete input payload from it. Originals are preserved; a failed Chiplet run
also preserves any existing output file.

The [comparison results](tools/repair-emma-hex-results.txt) record the trial
against the existing ETI repair copies, with per-file details in the adjacent
CSV. These are byte comparisons, not full gameplay checks.

### Analyzing a Binary or a Directory

Analyzing a single binary or a directory of binaries, suppressing unneeded output:

Example:

```
> chiplet -q -s IBM-Logo.ch8

IBM Logo.ch8, 6 opcodes used (0ms)
    possible variants: chip-8, chip-10, chip-48, schip-1.0, schip-1.1, xo-chip
Used opcodes:
00E0: 1
1000: 1
6000: 2
7000: 5
A000: 6
D00F: 6
```

This can run over a directory of files, the opcode statistics are then for
all detected CHIP-8 files in the directory tree. Files scanned can have the
endings: `.ch8`, `.ch10`, `.hc8`, `.c8h`, `.c8e`, `.c8x`, `.sc8`, `.mc8`, `.xo8`,
`.c8` or `.o8`. To find programs regardless of their file extension, use
`--deep-scan`.

It will try to identify which CHIP-8 variants this binary is made for.
The estimation is far from perfect, and mainly excludes variants when
encountering opcode that are only supported by a specific version like
a long `I` load from XO-CHIP.

### Finding Opcode Use

Looking for binaries in a directory tree, that make use of an opcode
with a given pattern (e.g. `Dx0F` looks for binaries that use
`sprite * v0 0xF`):

```
> chiplet -q -f Dx0F my-chip-archive/

Dx0F: Flappy Pong (by cnelmortimer)(2017).ch8
Dx0F: Octo Bird (by Cody Hoover)(2016).ch8

Done scanning/decompiling 534 files, not counting 125 redundant copies, found opcodes in 2 files (50ms)
```

The pattern rule is case-insensitive and any character that is not a
hex digit will be seen as nibble sized wildcard. Multiple `-f` options
can be used to look for multiple opcodes at one run.

---

## The Preprocessor Syntax

### Conditional Assembly

For conditional assembly of parts of the source, the directives `:if`,
`:unless`, `:else` and `:end` can be used. The condition that defines
if the code is used or not is based on options given by `-D` command-line
option or by `:const` directives in the source.

A typical use case  might be:

```
:if XOCHIP
    i := long data
:else
    i := data
:end
```

If compiling with `-D XOCHIP` this sequence will emit the long
variant of the opcode (`F000`) else the normal opcode will be used.
The `:unless` directive has the inverted logic, emitting the following
code if the option is not set.


### Segments and Code Layout Management

XO-CHIP architecture imposes a memory constraint where executable
code must reside within the first 3.5KB of memory space, while the
remaining 60KB can be allocated for data storage. This limitation
exists because while XO-CHIP provides the `i := long <label>`
instruction for loading 16-bit addresses into the index register,
it lacks corresponding instructions for jumping to or calling 16-bit
memory locations.

During development, it's often more intuitive and maintainable to
organize related code and data elements together in the same source 
files. Chiplet addresses this challenge through intelligent 
automatic code reorganization using segment annotations.

By marking sections of your code with `:segment code` and `:segment data`
directives, Chiplet can automatically restructure your program to
meet XO-CHIP's memory layout requirements. The assembler ensures
that all executable instructions are placed in the lower memory region while data elements are relocated to the extended memory space.

Note: Files are implicitly treated as starting with :segment code, so this annotation can be omitted when your file begins with executable instructions.

Example:
```
i := long lookup_table
load v4
# Process the loaded data...

:segment data

: lookup_table
0 1 2 3 4
```

In this example, the executable code will be placed in the restricted
memory area, while the lookup_table data will be automatically 
moved to the extended memory region, maintaining proper program
functionality while adhering to XO-CHIP's architectural constraints.

### Inclusion of Files

To organize a complex project it is often useful to split the
code into multiple files, this can also be used to have some
library of macros that can be used in multiple projects.

Including another source file:

```
:include "my-macros.8o" 
```

### Inclusion of Images

The include-directive can also be used to import image files
that are converted into 1-bit sprite data. The general syntax
is:

```
:include "<path-to-image-file>" <modifiers>
```

Where `<path-to-image-file>` is the image file to import (valid image
formats/extension are `.png`, `.gif`, `.bmp`, `.jpg`, `.jpeg` and `.tga`).

While Octopus supports TIFF import, Chiplet does not, and while it supports TGA,
Octopus does not support TGA import. Also note that due to the way JPEG works,
the resulting images of Chiplet and Octopus are not bit identical, due to
math rounding differences in the used JPEG readers. For modern development
of CHIP-8 variant games I simply recommend to stick to PNG.

#### Image Inclusion Modifiers

To influence behavior of the image importer, the following modifiers can
be used:

* `megachip` - The image will ge seed as a max. 255 color palette based image.
  A palette needs to be given somewhere after the `megachip` modifier, and the
  image will be converted into fitting that palette, defaulting to nearest color
  matching.
* `<width>x<height>` - The image will be split into sprites of the given size.
  The size must be a valid integer divisor of the image size. If no size is given,
  the image will be split into 8 pixels wide chunks that are even dividers of the
  image size and at most 15 pixels high.
* `[<color> ...]` - A palette to be used by theimporter. The colors must be given
  as hex values in the way used in e.g. CSS, so either `RRGGBB` or `RGB`, and the
  latter gets expanded by doubling the nibbles as in CSS. The brackets may be
  attached to the colors (`[000 FFF]`) or separated by whitespace (`[ 000 FFF ]`),
  and the list may span several lines. For XO-CHIP like color
  support the number of colors needs to be a power of two from 2 to 16. For
  `megachip` 2 to 254 colors are allowed. In that mode
  the palette is internally prefixed with a transparent black, and all transparent
  pixels are mapped to that in import.
* `dither` - The image will be Floyd-Steinberg dithered when reducing it to the
  palette.
* `no-labels` - The image will be split into sprites, but no labels are generated.
* `debug` - the generated sprite data will be annotated with ASCII art.

If `no-labels` is not used, the generated sprite data blocks are prefixed by a
label of the form:

```
<file-basename>-<column>-<row>
```

If a palette of more than two colors is used, an additional label section is
added and the form becomes:

```
<file-basename>-<plane>-<column>-<row>
```

If no labels should be generated, the optional argument `no-labels` can
be used.

The optional `debug` option can be used to let _Chiplet_ dump the generated
sprite data on the console as annotated ASCII (actually Unicode) art.

Example:

```
:include "screen.png"
```

This includes the image, converts it into grayscale and every
pixel brighter than 128 is interpreted as a 1, the others are 0.
The image will be cut into sprites automatically, but with the additional
sprite-size parameter a different splitting can be triggered. The
sizes must be valid integer divisors of the image size or an error will
be reported.

### Inclusion of External Files

Binary files with the extension `.bin` or `.ch8` are included as data bytes:

```
:include "<path-to-file>.bin" [no-labels]
```

The bytes are prefixed by a label with the base name of the file, unless
`no-labels` is given.

### Inclusion of Sound Files

WAV files (`.wav`, PCM with 8 or 16 bit, or 32 bit float, mono or stereo) are
included as unsigned 8-bit mono samples, centered at 128:

```
:include "<path-to-file>.wav" [<sample-rate>] [megachip] [no-labels]
```

* `<sample-rate>` - Resample the sound to this rate in Hz. Without it, the rate
  of the file is kept. The rate must be given on the line of the `:include`, a
  number on a later line is data.
* `megachip` - Prefix the samples with the six byte header of the MegaChip
  `digisnd` instruction: the sample rate (16 bit, so at most 65535 Hz), the
  number of samples (24 bit) and a zero byte.
* `no-labels` - Don't generate the label with the base name of the file.

Example, a sound for `ldhi` and `digisnd`:

```
:include "explosion.wav" 8000 megachip
```

---

## Cartridges

An Octo cartridge is a GIF file holding a program together with its quirks,
speed, colors and rotation, so the configuration can't get separated from the
program:

```
chiplet --cartridge-label "MY GAME" --cartridge-variant xo-chip -o game.gif game.8o
```

With `--hex`, the cartridge holds a hex dump of the assembled program instead of
its source, so emulators can read it without an assembler. See
[cartridge-format.md](cartridge-format.md) for the format, written for emulator
authors, and [tools/octocart.py](tools/octocart.py) for a reference decoder.

## Experimental Hybrid ROM Assembly

Chiplet accepts inline CDP1802 assembly in Octo programs:

```octo
:const BG_PORT 1

: main
    native inc_bgcol
    loop again

: inc_bgcol
    :asm cdp1802 {
        SEX R9
        OUT BG_PORT
        SEP R4
    }
```

The block emits its native bytes at the current Octo address. It does not
insert a call, return, alignment, or padding. Use Octo's `native label` for a
CHIP-8 `0nnn` call, and the target interpreter's return convention inside the
block. `SEP R4` is appropriate for the ETI-660 example; the convention is not
chosen automatically from the CPU name.

The existing CDP1802 assembler handles native syntax. Statements are separated
by newlines or semicolons; hexadecimal numbers use `#39` or `X'39'`; native
comments start with `..`. Octo `#` comments, directives, macros, and macro
argument substitutions are not interpreted inside the block. Braces inside
native strings and comments do not terminate the block. The preprocessor
preserves the complete block, including in included files and Octo macros.
Each expansion of a block gets independent native local symbols.

Previously defined Octo numeric constants, calculated values, and labels are
readable inside the block. Their values are captured at that occurrence of the
block. Forward references to Octo labels are also supported, including labels
in later native routines. Forward references to later `:const`/`:calc`
definitions are not supported. Native names use letters, digits, and
underscores, starting with a letter or underscore. Shared symbol spelling is
case-sensitive; CPU mnemonics and register names are case-insensitive. CPU
register names are reserved within native assembly. Imported values must be
finite integers that fit the operand being encoded.

Native labels (`name:`) and equates (`name = expression`, `name EQU expression`)
are local to their block and may shadow imported symbols. They are never
exported into Octo. Put an outer Octo label before each block for externally
callable entry points. Native expressions use the CDP1802 expression grammar,
including `+`, `-`, the current statement address `*`/`$`, and `A.1(expr)` /
`A.0(expr)` to select an address's high / low byte.

This first version accepts only `cdp1802` and requires contiguous blocks with a
fixed size in the 16-bit address space. Native `ORG`, `PAGE`, and `END` are
rejected; use outer Octo `:org` for placement. No instruction relaxation is
performed. Short branches must target the page containing their operand;
otherwise use a long branch. Data width is explicit: bare `DB`/`DC` expressions
and `A.0`/`A.1` emit one byte, `A(expr)` emits a two-byte big-endian address,
and text emits its bytes. An out-of-range byte is an error instead of widening
it after layout.

Octo reserves a native block's size during layout and reassembles it at its
actual address once the program's labels are known. Errors point into the
native source, including included files; source maps cover individual native
instructions.

The example `test/hybrid/wipeout-eti660.8o` reconstructs W. F. Kreykes's
ETI-660 Wipeout hybrid using three native blocks and symbolic CHIP-8 references:

```sh
chiplet --start-address 1536 -o wipeout.ch8 test/hybrid/wipeout-eti660.8o
```

The result is exactly 606 bytes, identical to the supplied original image:
SHA-256 `e7c71e4f3fd9839204429f6d00b1361b83ceb71fd4c6c08aae4db5fa46e13913`.
The original-image comparison is included in the hybrid regression tests.

---

## Compiling from Source

_Chiplet_ is written in C++20 and uses CMake as a build solution. To build it,
checkout or download the source. The dependencies that are not part of the
repository are fetched during configuration.

### Linux / macOS

Open a terminal, enter the directory where the code was extracted and run:

```
cmake -S . -B build
```

to configure the project, and

```
cmake --build build
```

to compile it.

### Windows

The CI builds _Chiplet_ with MSVC and Ninja. In a developer command prompt of
Visual Studio run:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

It also builds with MinGW, e.g. with [W64devkit](https://github.com/skeeto/w64devkit).
To build in the Bash from W64devkit:

```
export PATH="$PATH;C:/Program Files/CMake/bin"
cmake -G"Unix Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -S . -B build-w64dev
cmake --build build-w64dev
```

---

## Used Resources

### Information

* [Octopus](https://github.com/Timendus/chipcode/tree/main/octopus) - This is the
  preprocessor that defined the syntax and feature set, we talked about this idea
  during it's creation time and I followed his specification to implement the
  preprocessor in _Chiplet_, and it should behave the same, with the only exception
  that _Chiplet_ supports TGA but doesn't support TIFF.
* [Octo assembler syntax](http://johnearnest.github.io/Octo/docs/Manual.html) - The
  preprocessor needs to make sure it doesn't break Octo, so it's lexer is implemented
  with respecto to the Octo specification by John Earnest.


### Libraries

* [c-octo assembler](https://github.com/JohnEarnest/c-octo) - the octo
  assembler that Chiplet's assembler for [Octo assembler syntax](http://johnearnest.github.io/Octo/docs/Manual.html)
  was originally based on, Chiplet now uses its own C++ implementation
* [fmtlib](https://github.com/fmtlib/fmt) - a modern formatting library in
  the style of C++20 std::format _(automatically fetched during configuration)_
* [fast_float](https://github.com/fastfloat/fast_float) - fast and exact parsing
  of floating point numbers _(automatically fetched during configuration)_
* [JSON for Modern C++](https://github.com/nlohmann/json) - the JSON library by
  Niels Lohmann, used for cartridge options _(contained in `external/nlohmann`)_
* [ghc::filesystem](https://github.com/gulrak/filesystem) - An implementation
  of std::filesystem for C++11/14/17/20 that also works on MingW variants
  that don't have a working std::filesystem in C++17 and with better utf8
  support _(automatically fetched during configuration)_
* [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) - An image
  format loader from Sean Barrets great `nothings` collection, used to support
  image import, together with `stb_image_write` _(contained in `external/nothings`)_
* [libresample](https://github.com/minorninth/libresample) - A fast resampler
  library for audio data by Dominic Mazzoni, based on resample-1.7 by by Julius
  O. Smith III
* [tl::expected](https://github.com/TartanLlama/expected) - A lightweight
  implementation of the C++23 std::expected type for error handling by TartanLlama
  _(automatically fetched during configuration)_
* [doctest](https://github.com/doctest/doctest) and the
  [CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite) by Timendus
  are used by the tests _(automatically fetched during configuration)_
