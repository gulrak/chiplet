# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v2.0.0] - 2026-11-10

This release covers everything since v1.0.1 (March 2023), including the never released v1.1.0.

### Added

#### Assembler

- New `-a`/`--arch` option, besides the default `octo` it supports `cdp1802`, a standalone
  assembler for the RCA CDP1802 used in the COSMAC VIP, ETI-660 and similar machines, see
  [CDP1802 Assembler Quick Reference.md](CDP1802%20Assembler%20Quick%20Reference.md)
- Experimental hybrid programs: `:asm cdp1802 { ... }` blocks put native CDP1802 code inside of
  Octo programs, with access to Octo constants and labels (including forward references to
  labels), block local labels and equates, and error messages pointing into the native source;
  the ETI-660 Wipeout hybrid can be rebuilt byte identical from source
- `--start-address` sets the load address of a program (the address of `: main`), default `512`
- `--hex` writes Intel HEX instead of a raw binary, with 16 data bytes per record
- `-l`/`--listing` generates an additional listing with addresses
- Assembly for the experimental CHIP-8R variant with PC relative jumps, calls through `I` and
  16 bit `I` loads
- The library gives access to Octo's `:breakpoint` and `:monitor` information, so debuggers
  like Cadmium's can use them

#### Octo cartridges

- Programs can be written as Octo compatible cartridge GIFs with `--cartridge-label` (text
  label) or `--cartridge-image` (image as label)
- `--cartridge-options` reads the options to store from a JSON file, `--cartridge-variant`
  sets them from a variant (`chip-8`, `schip`, `octo` or `xo-chip`)
- `--original-cart` uses the original Octo cartridge image instead of the CD48 one
- With `--hex` a cartridge stores a hex dump of the assembled program instead of its source,
  so emulators can read it without an assembler
- `tools/octocart.py`, a standalone reference decoder for cartridges

#### Preprocessor

- `:segment code` and `:segment data` let code and data be written together, Chiplet moves the
  data behind the code, so XO-CHIP programs keep their code in the lower 3.5k
- Binary files (`.bin`, `.ch8`) can be included as data bytes
- WAV files can be included as unsigned 8 bit mono samples, optionally resampled to a given
  rate, and with `megachip` prefixed with the header of the MegaChip `digisnd` instruction
- Image import supports palettes: XO-CHIP style multi plane sprites with 2 to 16 colors, and
  with `megachip` palette based MegaChip images with up to 254 colors
- New image modifier `dither` for Floyd-Steinberg dithering when reducing to the palette

#### Disassembler and analyzer

- `--relocate` moves a working binary or HEX program to another address, using the execution
  path tracing to change only real address operands
- `--deep-scan` searches a directory tree for anything that looks like a CHIP-8 variant
  program, regardless of the file extension
- Disassembly of CHIP-8E programs
- New variants: SCHIP 1.0 beta, modern SCHIP as defined by Octo, more two page display
  variants, CHIP-8R and programs for a COSMAC VIP without CHIP-8

### Changed

- The Octo assembler is now Chiplet's own C++ implementation instead of the embedded C-Octo
  code, with a lexer shared by the preprocessor and the assembler, and with faster assembly
- The disassembler is table based; since the changes of the never released v1.1.0 it can
  round-trip every program found that fits into 64k (only two MegaChip examples are known that
  don't)
- Chiplet now needs C++20 to build and is also built with MSVC on Windows
- Release packages contain only `chiplet`, the README and the LICENSE, Linux packages are now
  TGZ archives

### Fixed

- Assembler errors when assembling the output of the disassembler (round-trip)
- An abort, and line info comments inside of imported image data
- Several SCHIP opcode issues
- Saving and loading of Octo cartridges did not round-trip
- Crashes of the WebAssembly build caused by stack overflows
- Builds with MinGW and MSVC on Windows
- Preprocessor corner cases
- The version output showed the commit of the enclosing project when Chiplet was built as part
  of it (e.g. in Cadmium)

## [v1.0.1] - 2023-03-01

- Preprocessing of .8o files
- Compiling (with preprocessing) of .8o files
- Disassembling of CHIP-8 variant binaries
- Analyzing CHIP-8 binaries for variant and opcode use
- Search for opcode patterns in a directory tree

[Unreleased]: https://github.com/gulrak/chiplet/compare/v2.0.0...HEAD
[v2.0.0]: https://github.com/gulrak/chiplet/compare/v1.0.1...v2.0.0
[v1.0.1]: https://github.com/gulrak/chiplet/releases/tag/v1.0.1
