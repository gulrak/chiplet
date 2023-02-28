# Chiplet

_A commandline multitool for CHIP-8 variant development_

<!-- TOC -->
* [Chiplet](#chiplet)
  * [Overview](#overview)
  * [Command-line Options](#command-line-options)
  * [Usage Scenarios](#usage-scenarios)
    * [Preprocessing a File](#preprocessing-a-file)
    * [Assembling a File](#assembling-a-file)
    * [Disassembling a Binary](#disassembling-a-binary)
    * [Analyzing a Binary or a Directory](#analyzing-a-binary-or-a-directory)
    * [Finding Opcode Use](#finding-opcode-use)
  * [Compiling from Source](#compiling-from-source)
    * [Linux / macOS](#linux--macos)
    * [Windows](#windows)
<!-- TOC -->

## Overview

Chiplet is the result of my Work on [Cadmium](https://github.com/gulrak/cadmium),
my CHIP-8 emulator environment. It contains a preprocessor that is compatible
with [Octopus](https://github.com/Timendus/chipcode/tree/main/octopus) from
Tim Franssen, combined with an [Octo Assenbly Language](https://github.com/Timendus/chipcode/tree/main/octopus)
compatible assembler based on the Code from [C-Octo](https://github.com/JohnEarnest/c-octo)
from John Earnest.

It combines these two to allow using modular CHIP8 projects with multiple
files, while retaining error message output that refers to those original
files by translating C-Octo errors back to the original locations brefore
the preprocessing step.

## Command-line Options

```
USAGE: chiplet [options] ...

OPTIONS:

Assembler/Preprocessor:
  --no-line-info
    omit generation of line info comments in the preprocessed output

  -I <arg>, --include-path <arg>
    add directory to include search path

  -P, --preprocess
    only preprocess the file and output the result

  -o <arg>, --output <arg>
    name of output file, default stdout for preprocessor, a.out.ch8 for binary

Disassembler/Analyzer:
  --list-duplicates
    show found duplicates while scanning directories

  --round-trip
    decompile and assemble and compare the result

  -d, --disassemble
    dissassemble a given file

  -f <arg>, --find <arg>
    search for use of opcodes

  -p, --full-path
    print file names with path

  -s, --scan
    scan files or directories for chip roms and analyze them, giving some information

  -u, --opcode-use
    show usage of found opcodes

General:
  --version
    just shows version info and exits

  -q, --quiet
    suppress all output during operation

  -v, --verbose
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

Creating Octo source from a CHIP-8 variant binary:

```
chiplet -o output.8o -d some-program.ch8
```

If no output is set, the disassembly will be dumped to stdout.

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
endings: `.ch8`,  `.c8x`, `.ch10`, `.sc8`, `.xo8` or `.mc8`

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
:include "<path-to-image-file>" [<width>x<height>] [no-labels] [debug]
```

Where `<path-to-image-file>` is the image file to import (valid image
formats/extension are `.png`, `.gif`, `.bmp`, `.jpg`, `.jpeg` and `.tga`),
valid sizes are `8x1`, `8x2`, `8x3`, `8x4`. `8x5`, `8x6`, `8x7`, `8x8`,
`8x9`, `8x10`, `8x11`, `8x12`, `8x13`, `8x14`, `8x15` or `16x16`.

The generated sprite data blocks are predixed by a label of the form:

```
<file-basename>-<column>-<row>
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
The image will be cut into sprites automatically, but with an additional .

---

## Compiling from Source

_Chiplet_ is written in C++17 and uses CMake as a build solution. To build it,
checkout or download the source.

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

Currently, _Chiplet_ is not tested to compile with MSVC++ so the recommended
toolchain is [W64devkit](https://github.com/skeeto/w64devkit). I might try to
make it compile on MSVC++, but I am not working on that in the near future.
That being said, I would not per-se reject PRs helping with this.

To build in the Bash from W64devkit:

```
export PATH="$PATH;C:/Program Files/CMake/bin"
cmake -G"Unix Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -S . -B build-w64dev
cmake --build build-w64dev
```
