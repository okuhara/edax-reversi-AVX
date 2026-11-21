# edax-reversi-AVX

Edax is a very strong othello program. Its main features are:
- fast bitboard based & multithreaded engine.
- accurate midgame-evaluation function.
- opening book learning capability.
- text based rich interface.
- multi-protocol support to connect to graphical interfaces or play on Internet (GGS).
- multi-OS support to run under MS-Windows, Linux and Mac OS X.

This is SSE/AVX optimized version of Edax 4.4.0. Functionally equivalent to the parent project, provided no bugs are introduced.

Thanks to AVX2, x64-modern build solves fforum-40-59.obf 60% faster than official edax-4.4 on Haswell, and runs level 30 autoplay 80% faster.

See http://www.amy.hi-ho.ne.jp/okuhara/bitboard.htm and http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm for optimization details in Japanese.

## Download and run

1. Download the ZIP from the [latest release](https://github.com/okuhara/edax-reversi-AVX/releases/latest) and unpack it.
2. Open the `bin` folder and start the executable for your system from **inside that folder** (Edax looks for `data/eval.dat` in the current folder).

| System | Executable in `bin/` |
|---|---|
| Windows x64 | `wEdax-x86-64.exe` (any CPU), `-v3` (AVX2), `-v4` (AVX-512) |
| Windows x86 / ARM64 | `wEdax-x86.exe`, `wEdax-x86-sse.exe` / `wEdax-arm64.exe` |
| Linux x64 / x86 | `lEdax-x86-64` (any CPU), `-v3`, `-v4` / `lEdax-x86` |
| macOS | `mEdax-arm64` (Apple silicon), `mEdax-x64-modern` (Intel) |
| Android | `aEdax-arm64-v8a`, `aEdax-armeabi-v7a` |

If you are not sure which CPU features you have, use the version without `-v3`/`-v4`. The ZIP also contains the libedax libraries (`.dll`, `.so`, `.dylib`) for GUIs and tools such as [nikque GUI](https://github.com/Nikque/nikque-gui) and [edax_runner](https://github.com/Nikque/edax_runner).

Getting started: [Manual, chapter 2](manual/en/02-quick-start.md).

## Changes in 4.5.7, ported from the [nikque fork](https://github.com/Nikque/edax-reversi-AVX)

- **Book learning fixed**: `book deviate` could crash or lose positions (a dangling pointer); `book merge`, `book fix` and book saving were made safe (temporary file, damaged records rejected, a book file held by another program at the start never replaced).
- **Book commands go on from a saved book**: `book correct`, `book leaf recalculate` and `book learn` continue from a book saved before they ended (marks removed when they end); `book deepen` works.
- **Exact search fixed** (v4.5.6.1): at level 60 from 17 empties on, the exact score (and move) could rarely be wrong (a score cut by probcut after a pass was stored as exact); the same error is in Edax 4.5.5 and 4.5.6 of Toshihiko Okuhara.
- **New learning commands**: `book deviate2` / `book deviate3` (limits on loss per move and in total).
- **Faster learning on many cores**: several book positions are expanded at the same time (`book-expand-tasks`, `book-store-tasks`); with 32 threads at level 18, up to about 4.7 times faster.
- **Less memory**: about 50 bytes per book position (58 before).
- **Book analysis and maintenance**: `book moveinfo`, `book missing`, `book bestlines`, `book reduce`, the `export` of the `book deviate` family (the lines they walk, written without expanding the book) and others (used by nikque GUI).
- **17 other fixes** in file I/O, time control, PGN/GGF and protocols (XBoard, GTP, NBoard).

The complete list is in the [release notes](RELEASE-NOTES.md).

## Build

- **Windows** (Visual Studio 2022, Developer Command Prompt): in `src`, run `nmake -f NMakefile vc-x64-v4` (other targets are in `src/NMakefile`; `build-win-v4.cmd` builds the v4 executable). The release executables use the PGO targets (`vc-pgo-x64-v4`, `vc-pgo-x64-v3`, `vc-pgo-x64`, and `vc-pgo-x86-sse`, `vc-pgo-x86` from an x86 prompt), except ARM64.
- **Libraries**: Windows `nmake -f NMakefile vc-lib-x64` (`-v3`, `-v4`); Linux and others `make libbuild ARCH=<x86-64|x86-64-v3|x86-64-v4> COMP=gcc OS=linux`. See [manual 12.2](manual/en/12-integration.md).
- **Other platforms**: the [release-binaries workflow](.github/workflows/release-binaries.yaml) builds Linux, macOS and Android; `package-release.py` assembles the ZIP.
- The 32-bit Linux build (`lEdax-x86`) needs an i486 or later CPU. The upstream 32-bit macOS `mEdax-x86` is not included: current Xcode SDKs cannot link a corrected i386 binary.

## License

GPL-3.0 ([LICENSE](LICENSE)).  If you redistribute the executables, keep the source and the license available.

The nikque fork was developed with ChatGPT-6 Astra / Sol and Claude Opus 5.5.

## Comparison with [Edax 4.6]( https://github.com/abulmo/edax-reversi/)

Richard Delorme's own Edax 4.6 incorporated most of my changes and almost caught up Edax AVX 4.5.3.  However, I decided to maintain this forked project for further improvements.
- 4.6 dropped some optimizations toward Microsoft compilers on Windows, such as heavy use of vectorcall and keeping the bitboard (vboard) in non-volatile SSE registers.  On Windows(x64), the 4.5.3 MSVC build compiles much faster, and runs a little faster than clang.
- 4.6's algorithm choice is based on his particular environment, which may not be good for others.  For example I avoid PEXT in x86-64-v3 build since it is very [slow](https://boobyreversi.flop.jp/edaxopt.htm#boardscorebench) on some old AMD CPUs, and there are fairly good alternatives.

Now my 4.5.5 runs 13-17% faster than Edax 4.6 on Windows (Core i7-9700), by introducing thread local hash, simul lastflip tables and other refinements.

| fforum-20-39 (1 thread) | 4.4 | 4.6 | 4.5.3 | 4.5.4 | 4.5.5 |
|-----------------|-------|-------|-------|-------|-------|
| wEdax-x86-64    | 3.503 | 2.629 | 2.406 | 2.330 | 2.203 |
| wEdax-x86-64-v3 | 2.956 | 2.141 | 2.035 | 1.998 | 1.828 |

| fforum-40-59 (8 threads) | 4.4 | 4.6 | 4.5.3 | 4.5.4 | 4.5.5 |
|-----------------|--------|--------|--------|--------|--------|
| wEdax-x86-64    | 2:32.5 | 1:41.7 | 1:30.9 | 1:33.0 | 1:26.4 |
| wEdax-x86-64-v3 | 2:21.3 | 1:22.0 | 1:20.2 | 1:20.1 | 1:11.5 |

