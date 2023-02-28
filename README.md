# Chiplet

_A commandline multitool for CHIP-8 variant development_

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

## Commandline Options

```
USAGE: chiplet [options] ...

OPTIONS:

--no-line-info
    omit generation of line info comments in the preprocessed output

--version
    just shows version info and exits

-I <arg>, --include-path <arg>
    add directory to include search path

-P, --preprocess
    only preprocess the file and output the result

-o <arg>, --output <arg>
    name of output file, default stdout for preprocessor, a.out.ch8 for binary

-q, --quiet
    suppress all output during operation

-v, --verbose
    more verbose progress output

...
    Files or directories to work on (currently only files)
```

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
