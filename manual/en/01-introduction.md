# 1. Introduction

[Contents](README.md) | Next: [2. Starting and playing a first game](02-quick-start.md)

## 1.1 What Edax is

Edax is a program that plays Othello (Reversi). It was written by Richard Delorme. Toshihiko Okuhara publishes a faster version that uses the instruction sets of recent CPUs, such as AVX2 and AVX-512 (Edax 4.5.x, `okuhara/edax-reversi-AVX`).

This **fixed version** (`Nikque/edax-reversi-AVX`) is based on that v4.5.5. Its versions are named `v4.5.5-nikque.N`. It adds:

- bug fixes (book handling, multi-threaded search, reading and writing games, …);
- speed and memory improvements, so that large books (hundreds of millions of positions) can be handled;
- commands to grow and maintain a book (`book deviate2`, `book deviate3`, `book learn`, `book leaf recalculate`, `book leaf expand`, `book fill2`, …);
- a library (libedax) for calling Edax from other programs.

The evaluation data (`eval.dat`) and the book file format are the same as in the original Edax 4.5.5. A book from the original Edax can be read without conversion.

The changes in each version are listed in `RELEASE-NOTES.md` in the package. This manual explains how to use the program.

## 1.2 What you can do with Edax

| What you want to do | Chapter |
|---|---|
| Play against Edax. See how good the moves of a game were | [2](02-quick-start.md), [3](03-playing.md) |
| Find the best move and the score of a position | [3](03-playing.md), [5](05-search.md) |
| Solve an endgame position to the end (exact solving) | [5](05-search.md), [11](11-solve-bench.md) |
| Build, grow and maintain opening data (a book) | [6](06-book-basics.md) to [9](09-large-books.md) |
| Convert and check game files | [10](10-files.md) |
| Use Edax from a graphical program (GUI) or from your own program | [12](12-integration.md) |

Edax itself has no window where you place discs with the mouse. **You operate it by typing.** The board is drawn with text characters in a console window, and you type moves such as `f5`.

To see a graphical board, connect Edax to a GUI program that supports it ([chapter 12](12-integration.md)).

## 1.3 What the package contains

The ZIP file on the Releases page contains the following:

| Place | Content |
|---|---|
| `bin/` | The executables (table below), the libraries (libedax), the settings file `config.ini`, and `README.MS-Windows.txt`, the note for Windows that came with the original Edax |
| `bin/data/eval.dat` | The evaluation data. **Edax does not start without it** |
| `bin/data/book.dat` | A first book (nearly empty: it holds the initial position only) |
| `problem/` | Endgame problem sets (`fforum-1-19.obf` and three more; [chapter 11](11-solve-bench.md)) |
| `README.md` | Overview of this fork (English and Japanese) and the upstream README |
| `RELEASE-NOTES.md`, `RELEASE-NOTES.ja.md` | Changes of each version (tests and measurements: `docs/validation/` of the source) |
| `manual/en/`, `manual/ja/` | This manual (English and Japanese; since v4.5.5-nikque.13) |
| `LICENSE` | The licence (GPL-3.0) |

## 1.4 Which executable to use

`bin/` holds one executable for each operating system and CPU type. They all work the same way and give the same search results. Only the speed differs.

| Your machine | Executable |
|---|---|
| Windows (64-bit), any PC | `wEdax-x86-64.exe` |
| Windows, a CPU with AVX2 | `wEdax-x86-64-v3.exe` (faster than the one above) |
| Windows, a CPU with AVX-512 | `wEdax-x86-64-v4.exe` (faster still) |
| Windows (32-bit) | `wEdax-x86-sse.exe` (CPU with SSE2), `wEdax-x86.exe` |
| Windows (ARM64) | `wEdax-arm64.exe` |
| Linux (64-bit) | `lEdax-x86-64`, `lEdax-x86-64-v3` (AVX2), `lEdax-x86-64-v4` (AVX-512) |
| Linux (32-bit) | `lEdax-x86` |
| macOS | `mEdax-arm64` (Apple silicon), `mEdax-x64-modern` (Intel) |
| Android | `aEdax-arm64-v8a`, `aEdax-armeabi-v7a` |

- **If in doubt, use the executable without `v3` or `v4` in its name** (`wEdax-x86-64.exe` on Windows). It runs on any 64-bit CPU.
- The `v3` executables need a CPU with AVX2, and the `v4` executables need a CPU with AVX-512. They do not run on CPUs without these instruction sets.
- If you do not know which instruction sets your CPU supports, look up its model, or use the executable without a suffix.
- Approximate speed difference: standard 69 million nodes per second, `v3` 81 million, `v4` 92 million. This was measured for the test records (`docs/validation/`) on a Ryzen 9 9950X, with one thread, solving 20 endgame positions exactly.
- According to the test records (`docs/validation/`):
  - the Windows ARM64 and Android executables were built but not tested on a device;
  - the macOS executables come from the automated release build.
- The checks for this manual were made with the 64-bit Windows version.

## 1.5 Key terms used in this manual

See the [glossary](15-glossary.md) for more.

| Word | Meaning |
|---|---|
| Position | The discs on the board and the side to move |
| Move | The square where a disc is placed, written like `f5` |
| Pass | Giving the turn to the opponent because no square can be played. Written `ps` |
| Empties | The number of empty squares: 60 at the start of a game, 0 at the end (a game also ends with empties left when neither side can move) |
| Score | "By how many discs the game should end with best play", **seen from the side to move**. `+4`: the side to move should win by 4 discs; `-2`: lose by 2; `+0`: draw |
| Level | A number (0 to 60) that sets the strength of the search. The larger, the deeper Edax reads and the longer it takes ([chapter 5](05-search.md)) |
| Exact solving | Reading everything to the end of the game, which gives the exact final disc difference |
| Book | Data that stores opening and midgame positions with their scores; an "opening book" ([chapter 6](06-book-basics.md)) |
| Thread | A unit of work that the CPU can run at the same time as others. More threads make the search faster |

Next: [2. Starting and playing a first game](02-quick-start.md)
