# 12. Using Edax from other programs

[Contents](README.md) | Previous: [11. Solving problems and measuring speed](11-solve-bench.md) | Next: [13. Troubleshooting](13-troubleshooting.md)

Edax is operated through text, but it can also serve as the engine (the "brain") of another program. There are three ways to connect it (12.1 to 12.3); 12.4 describes the book analysis commands that GUI programs use.

| Way | When it fits |
|---|---|
| **Connecting a GUI program** (12.1) | You want to see the board graphically and play or analyse with the mouse |
| **Calling the library (libedax)** (12.2) | You are writing your own program and want to use Edax's features |
| **Using edax_runner** (12.3) | You want to list the lines to learn and have the book grown automatically |
| **The book analysis commands** (12.4) | You want a GUI program to have Edax count the precision stages, umigame numbers, positions and so on of the book |

## 12.1 Connecting a GUI program

A GUI program starts Edax in the background and exchanges messages with it, such as "play in this position" or "this move was played". These messages follow a fixed set of commands called a **protocol**. Edax supports these protocols:

| Startup option | Protocol | The other side |
|---|---|---|
| (none) | Edax's own words (the commands explained in this manual) | A person, or a program that sends Edax commands directly |
| `-gtp` | GTP (Go Text Protocol) | A GUI that supports GTP |
| `-nboard` | NBoard | NBoard |
| `-xboard` | XBoard (WinBoard) | XBoard or WinBoard with Reversi support |
| `-cassio` | The engine words of Cassio | Cassio (macOS) |
| `-ggs` | Edax itself connects to GGS (the internet game server) | GGS |

Even without an option, Edax switches to a protocol when the first command it receives is one of these:

- `nboard 1` (NBoard)
- `xboard` (XBoard)
- `protocol_version` (GTP)
- `engine-protocol init` (Cassio)

**Basic connection steps** (settings on the GUI side)

1. In the GUI's engine registration screen, specify the Edax executable (`wEdax-x86-64.exe` or another one in `bin`).
2. **Set the working folder (the folder it runs in) to `bin`.** Edax looks for `data/eval.dat` from there ([2.1](02-quick-start.md)). If the GUI cannot set a working folder, add `-eval-file` and `-book-file` to the startup options, with paths that include the folder.
3. If needed, add the protocol (`-gtp` and so on), the strength (`-l 18`) and the number of threads (`-n 4`) to the startup options. `config.ini` is read when a GUI starts Edax too.

**What was checked for this manual**: connecting to the GUI programs themselves was not tried. Only this was checked: Edax starts with the three protocols (GTP, NBoard, XBoard) and responds to their basic commands.

A GTP example (lines with `>>>` are what was sent; the lines below them are Edax's responses):

```
>>> protocol_version
= 2

>>> name
= Edax

>>> boardsize 8
=

>>> play black f5
=

>>> genmove white
= d6
```

`list_commands` shows the commands available in GTP: `protocol_version`, `name`, `version`, `known_command`, `list_commands`, `quit`, `boardsize`, `clear_board`, `komi`, `play`, `genmove`, `undo`, `time_settings`, `time_left`, `set_game`, `list_games`, `loadsgf`, `reg_genmove`, `showboard`.

An NBoard example:

```
>>> nboard 1
>>> set depth 4
set myname Edax4
>>> go
status Edax is thinking
nodestats 257 0.00
=== d6 2.00 0.0
status Edax is waiting
```

An XBoard example (the response to `protover 2` lists the supported features):

```
>>> protover 2
feature setboard=1 playother=1 ping=1 draw=0 sigint=0 sigterm=0 analyze=1 myname="Edax 4.5.5" variants="reversi" colors=0 nps=1 memory=1 smp=1 done=1
```

- Under a protocol too, playing strength is determined by `level` and by the time limits, and the book is used. `level` can also be set by NBoard's `set depth` and XBoard's `sd`.
- `bin/README.MS-Windows.txt` in the package (notes from the original Edax, in English) explains how to connect NBoard and WinBoard. It is old: file names and other details differ from the current package.
- GGS (`-ggs`) takes its server from the settings `ggs-host`, `ggs-port`, `ggs-login` and `ggs-password` (not tried for this manual).

## 12.2 The library (libedax)

**libedax** lets other programs call Edax's features as functions. It has the same functions as the library that lavox made for Edax 4.4, so a program written for libedax can use this fixed version of Edax just by replacing the library file.

The libraries in `bin/` of the package:

| File | Environment |
|---|---|
| `libedax-x64.dll` | Windows (64-bit). Runs on any CPU |
| `libedax-x64-v3.dll`, `libedax-x64-v4.dll` | Windows. For CPUs with AVX2, with AVX-512 |
| `libedax-x86-64.so`, `-v3.so`, `-v4.so` | Linux |
| `libedax.universal.dylib` | macOS (for both Apple silicon and Intel) |
| `libedax-arm64-v8a.so`, `libedax-armeabi-v7a.so` | Android (only the build was checked; not run on a device) |

Steps:

1. Place the library and `data/eval.dat` (required) in your program's folder. Add `data/book.dat` and `config.ini` if needed.
2. From C or C++, include `src/libedax.h` from the source. From another language, load the library through that language's foreign function interface (for Dart there is a package named libedax4dart).
3. Call `libedax_initialize` first (the settings are read there), start a game with `edax_init`, and call `libedax_terminate` at the end.
4. The functions correspond to Edax commands (`edax_play`, `edax_go`, `edax_hint`, `edax_book_deviate`, …).

A short example (shortened from `tests/libedax_example.c` in the source):

```c
#include <stdio.h>
#include "libedax.h"

int main(void)
{
	char *args[] = {"", "-eval-file", "data/eval.dat", "-book-file", "data/book.dat", "-level", "12", "-n-tasks", "2"};
	char moves[] = "f5d6c3";
	static LibedaxHintList hints;
	LibedaxMove last;
	int i;

	libedax_initialize(9, args);  /* the settings are read from edax.ini, config.ini, then these arguments */
	edax_init();                  /* new game */
	edax_play(moves);
	edax_hint(2, &hints);         /* the 2 best moves: hint[1] to hint[n_hints] */
	for (i = 1; i <= hints.n_hints; ++i)
		printf("%c%c %+d\n", 'a' + hints.hint[i].move % 8, '1' + hints.hint[i].move / 8, hints.hint[i].score);
	edax_go();                    /* Edax plays a move */
	edax_get_last_move(&last);    /* squares: A1 = 0, B1 = 1, ..., H8 = 63 */
	libedax_terminate();
	return 0;
}
```

Notes:

- One process can use one Edax.
- Call the functions one after another from a single thread. Only `edax_stop` and `edax_book_stop_count_bestpath`, which stop a running operation, may be called from another thread.
- The book is loaded into memory, and a save replaces the file. If two processes learn into the same book file and save it, only the last save remains. Let only one process write to a given book file.
- **Settings** are read from `edax.ini` and `config.ini` in the current folder, then from the arguments of `libedax_initialize` (the last one wins). The setting names and syntax are the same as in the edax program.
- **While a function that changes the book, or one that writes a game file (`edax_base_complete`, `edax_base_correct`), is running, `edax_stop` does not stop the search.** This prevents the result of an interrupted search from going into the book or the file.
- The move sequence returned by `edax_get_moves` holds at most 80 plies (moves and passes together).
- On Windows, file names are read as UTF-8. When a name is not valid UTF-8, it is read in the PC's code page (ANSI).
  - A Japanese name passed in the PC's code page (CP932) may, in rare cases, be read as a different name. This happens when its bytes also happen to be valid UTF-8: for example, the two half-width katakana "ﾐｽ" are read as "н".
  - In that case, a file created under that name gets the name read as UTF-8 ("н" here). When opening a file, the ANSI name is tried if no file has the UTF-8 name.
  - This was checked during the audit of v4.5.5-nikque.13. The library cannot know the encoding of its caller, so this behaviour is unchanged.
  - Keeping file names to ASCII letters and digits is safe.

### Details (differences from the original libedax, added functions, building)

**Compatibility.** The functions, and the layout of the data exchanged with the caller, are those of the original libedax (`src/libedax.h`). Edax's internal structures changed in 4.5, so they are not passed directly: the data are copied into structures with the original layout.

**Added functions**

- `edax_book_deviate2` and `edax_book_deviate3`: `book deviate2` and `book deviate3`.
- `libedax_cpu_level`: returns which build the CPU can run. A program can query `libedax-x64.dll` first, then load the v3 or v4 library.
- `edax_book_store_games`: plays and learns several games together, as `book learn` does. The games are given as a string, one game per line.
- `edax_book_store_tasks`: returns the number of games learned at the same time (`n-tasks` when `book-store-tasks` is `auto`).
- Added in v4.5.5-nikque.9:
  - `edax_book_save_checked`: saves the book and reports whether it was saved.
  - `edax_book_failed`: reports whether the last book function failed to add a position.
- Added in v4.5.5-nikque.12: `edax_book_leaf_recalculate`, `edax_book_leaf_recalculate2`, `3` and `4` (the `book leaf recalculate` commands, named `book leaf-recalculate` up to v4.5.5-nikque.13).
- `edax_book_keep_unread_file(file)` (added in v4.5.5-nikque.16): before a book saved to another file replaces a book file that could not be read, keeps that file as `<file>.damaged`.
  - It returns 1 if the file may be replaced, 0 if it must not.
- Added in v4.5.5-nikque.27: `edax_book_file_status(file)`. It returns what became of a book file that was to be loaded.
  - 0: it may be replaced (it was loaded, or no load of it failed).
  - 1: it could not be read as a book (damaged). It is kept as `.damaged` before a save.
  - 2: it could not be opened (another program had it open without sharing, …), even after waiting about 2 seconds. No save replaces it, and `edax_book_keep_unread_file` returns 0.
  - Before the initialization, it returns −1.
  - Called after the initialization, it lets a program notice a book file that could not be opened, and stop instead of learning into an empty book.
- Added in v4.5.5-nikque.26: `edax_book_deviate_export(base, x, y, file, which)`, the `export` of `book deviate`, `book deviate2` and `book deviate3` ([7.3](07-book-learning.md)).
  - It writes the lines that the command walks from the current position to the file `file`, without expanding the book.
  - `base` is 1 for `book deviate`, 2 for `book deviate2` and 3 for `book deviate3`.
  - `which` is 0 for `leaf`, 1 for `ends` and 2 for `all`.
  - `x` and `y` have the ranges of the commands. For `book deviate2` and `book deviate3`, `y` is at most 254; a larger `y` is reduced to 254 with the warning of the commands.
  - It returns the number of lines written. When the current position is not in the book, it returns 0 and makes an empty file.
  - It returns −1 when an argument is wrong, the file cannot be written or there is not enough memory.

**Differences from the original libedax (Edax 4.4)**

- The default level is 18 (it was 21). At the same level, search scores and moves can differ from Edax 4.4.
- `edax_book_merge` does what `book merge` does in this version: it also rebuilds the links, fixes the book and runs negamax. The original only added the positions.
- The best-path counts (`edax_book_count_bestpath`, `edax_book_count_board_bestpath`):
  - The original libedax kept them in every position of the book.
  - This version keeps them separately from the book, and only while they are in use. A book position still takes 48 bytes.
  - They are recounted when the limits change or when the book changes.
- The `link` array of a `Position` stays valid until 63 more positions are requested (8 up to v4.5.5-nikque.8).
- The lock that followed the result of `edax_bench` is not used.
- Calls that crashed the original libedax now do nothing or return an empty result:
  - a second initialization or termination,
  - `edax_get_last_move` before any move,
  - `edax_book_show` on a position not in the book,
  - a read-only string passed to `edax_get_bookmove_with_position_by_moves`.

**The edax program is not affected.** The libedax code is compiled only when the library is built. An edax built from the sources with libedax added is byte-identical to one built from the v4.5.5-nikque.6 sources (except for the build time and the name of the source folder).

**Speed and memory** (one thread, the 20 endgame positions of `bench`, Ryzen 9 9950X):

| | Speed | Peak memory |
|---|---|---|
| Original libedax (Edax 4.4, x86-64) | 53 million nodes/s | 159 MB |
| `libedax-x64.dll` | 69 million nodes/s | 82 MB |
| `libedax-x64-v3.dll` | 81 million nodes/s | 82 MB |
| `libedax-x64-v4.dll` | 92 million nodes/s | 82 MB |

The library and the edax program (for the same CPU) search the same number of nodes at the same speed.

**Tests**

- `tests/libedax_test.c` calls every function.
  - Number of checks: 147.
    - 191 in v4.5.5-nikque.9, 193 in v4.5.5-nikque.10, 197 in v4.5.5-nikque.12, 198 in v4.5.5-nikque.13.
    - 218 in v4.5.5-nikque.26 (14 of them for the export).
    - 279 in v4.5.5-nikque.27. These 61 checks were added (28 of them on Windows only):
      - an export after the book changes: 9;
      - a book that cannot be opened, in a folder with a non-ASCII name: 8;
      - the marks of the files that could not be loaded, names written another way, and `edax_book_file_status`: 21;
      - a book held by another program at the start: 17;
      - the depth of `edax_book_new`: 4;
      - the stop of `edax_hint`: 2.
  - The 3 Windows and the 3 Linux libraries pass.
  - The original libedax gives the same results on the checks it supports.
- 28 of the 29 tests of libedax4dart 7.67.0 pass. The remaining one compares a search score. The score differs because of the default level and the state left by previous searches; a fresh search at the same level gives the same score as the original libedax.
- Books saved by the Edax 4.4 libedax can be read by this version, and books saved by this version can be read by the Edax 4.4 libedax (all the positions of a 270,000-position book matched).

**Building**

- Windows: `nmake -f NMakefile vc-lib` (`vc-lib-x64`, `vc-lib-x64-v3`, `vc-lib-x64-v4`).
- Other systems: `make libbuild ARCH=<x86-64|x86-64-v3|x86-64-v4> COMP=gcc OS=linux`.
- Android: `ndk-build -C src NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application-lib.mk NDK_OUT=./obj-lib NDK_LIBS_OUT=./libs-lib`. It produces `src/libs-lib/<ABI>/libedax.so` (checked with NDK r27d).
- Test: `tests\build-libedax-test.cmd`.
- macOS: the release-binaries workflow builds the arm64 and x86-64 libraries and combines them into one file, `libedax.universal.dylib` (the name that programs written for libedax load).

## 12.3 edax_runner

**edax_runner** (`Nikque/edax_runner`) is a small program that **takes a list of lines to learn and has the book learn them from top to bottom**. It is sensuikan1973's edax_runner, adapted to run with the libedax of this fixed version. Instead of typing commands in Edax, you just write a text file.

How to use it (from the edax_runner README):

1. Download the package from the edax_runner Releases page and unpack it.
2. Write what is to be learned in `learning_list.txt`.
3. Edit `config.ini` as you like (same syntax as Edax; [chapter 4](04-settings.md)).
4. If you have a book to grow, place it at `data/book.dat`.
5. Start edax_runner.
6. The lines that have been learned are recorded in `learned_log.txt`.

How to write `learning_list.txt`:

| Purpose | Syntax | Example |
|---|---|---|
| From the given moves, play one game Edax against Edax and learn it (as one line of `book learn`) | `value of book-randomness,moves` | `2,F5F6F7F8` |
| Run `book deviate` from the position after the moves | `[relative-error absolute-error] moves` | `[1 1] F5F6F7F8` |
| Run `book fix` | `fix` | `fix` |
| A comment | `// comment` | `// the tiger opening starts here` |
| End edax_runner | `exit` | `exit` |

- Omitting `2,` and writing only the moves is the same as `0,moves`.
- Each time a line is done (or a group, when several are learned at the same time), edax_runner saves the book to `data/book.dat` and reads the list again. You can edit the list while it runs.
- A second edax_runner started in the same folder exits without doing anything (since 5.3.0+nikque.10).
- When `book-store-tasks` is not 1 (including the default `auto`), the "one game Edax against Edax" lines are grouped by that number and learned at the same time.
- The rest of this section, and the behaviour of edax_runner itself, were not rechecked for this manual (edax_runner is tested separately). For details, see the edax_runner `README.md`.

## 12.4 The book analysis commands for a GUI

These commands let a GUI program delegate to Edax the counts that require traversing the book (since v4.5.5-nikque.19).

- They all run in the background, alongside other commands. During an analysis, Edax still accepts `hint`, moves and other commands.
- They only read the book. Before a command that may change the book (`book store`, …), Edax stops them.
- Each one is sent with a number, `id=…`.
- The responses are lines starting with `@`. Each line is written whole, in one piece. If another output (a warning, …) is partly written at that moment, a response line may appear in the middle of it, as in `WARNING: @end id=5 ms=0`, with the rest of the warning on the next line.
- Values are disc differences from the point of view of the side to move in the current position.

| Command | Answer |
|---|---|
| `book moveinfo id=N` | For every legal move: the book value and level, the precision stage, the leaves missing for the next stage, the umigame numbers, the book positions reached after the move |
| `book missing id=N` [`sq=move`] | The lines to the leaves missing for the next stage (with `sq=`, after that move) |
| `book levels id=N` | The positions after the current position, per level (`scope=all`: the whole book) |
| `book bestlines id=N` | The number of lines where both sides play best moves only (or moves losing at most `loss=` discs); with `file=`, the moves to their ends are written |
| `book findline id=N target=position` [`n=`] | The lines from the current position to the `target` position through book positions only (the `n` lines where the value changes least, 1 by default) |
| `book reducecount id=N` [`x=`, `y=`] | The number of positions that `book reduce X Y` ([8.4](08-book-maintenance.md)) would keep and remove from the current position (the book is not changed; since v4.5.5-nikque.20) |
| `stop id=N` | Stop the analysis N (the other analyses and the commands waiting are kept) |

**Precision stage** (`stage=`)

| Stage | Meaning |
|---|---|
| 0 | The position after the move is not in the book |
| 1 | It is, but the lines of best moves of both sides do not reach a solved position |
| 2 | One does, but the range of stage 3 is not complete |
| 3 to 7 | `book deviate 1 1` to `book deviate 5 5` from the position after the move would add nothing |

- The reference command can be changed to `base=deviate2` or `base=deviate3`, and the ranges with `stages=1/1,2/2,3/3,4/4,5/5`.
- The stages found are kept until the book changes.
- For the position after a move, what the kept stages already tell is not walked again. The kept stages used are those of the current position and of the next positions (since v4.5.5-nikque.23; the answers are the same).
- `book missing` answers 0 lines without walking when the kept precision stages already tell that the range asked (`x=`, `y=`) finds nothing from that position (since v4.5.5-nikque.24; the answers are the same).

**Umigame numbers** (`umi=black:white`)

- For black and for white: the smallest number of line ends that must be learned to know every line meeting the condition after the move.
  - On its own turn, the learning side chooses one move.
  - On the opponent's turn, it has to know every move.
- A position reached by different lines is counted once, together with what follows it.
- By default, the condition is best moves for both sides. These can be used too:
  - `umi=loss x=… y=…`: loss per move, and total loss.
  - `umi=range lo=… hi=…`: a range of values.
- With a wide range, a number that may not be the smallest is marked `exact=0`.
  - A number marked `exact=0` is the same on every build (Windows, Linux, …) (since v4.5.5-nikque.25).
- The umigame numbers found are kept until the book changes (since v4.5.5-nikque.24).
  - When the position after a move is counted, the values of the positions 1 and 2 moves further are kept too. They are not counted again when those positions become the current position.
  - These positions are kept. No value is kept from a count marked `exact=0`.
    - `umi=best`: every position;
    - `umi=loss`: the positions reached with a loss of 0;
    - `umi=range`: the positions with the same side to move as the position after the move (all of them when `lo=-hi`).
  - The answers are the same, with one exception. A move marked `exact=0` when counted from its position alone may get the exact value kept from the position before. That value is not marked, and is smaller than or equal to the value marked `exact=0`.

Example (a test book):

```
>book moveinfo id=1
@begin id=1 ply=0 turn=black moves=4 book=1
@move id=1 sq=d3 book=1 value=-2 level=4 lines=505 stage=1 missing=1
...
@move id=1 sq=d3 part=umi umi=1:1
...
@move id=1 sq=d3 part=count positions=1546
...
@end id=1 ms=0
```

**`book moveinfo`**

- It first answers the moves out of the book, then the precision stages, the umigame numbers (`part=umi`) and the numbers of positions (`part=count`).
- Near the start of a large book, the numbers of positions take time. A part can be requested alone, as in `parts=stage,umi`.
- `lines=` is the number of unterminated lines recorded in the book (counted at once; a transposition is counted once for each line).
- The numbers of positions (`part=count`) and the counts of `book levels` (except `scope=all`) are kept for each position until the book changes (since v4.5.5-nikque.24).
  - In these cases, the book is not walked again, and the answers are the same:
    - `book levels` at a position already counted, when it has become the current position;
    - the same move counted again.

**`book bestlines`**

- It answers one line: `@end id=N lines=… ends=… incomplete=… incomplete_ends=… positions=… written=…`.
  - `lines`: the number of lines.
  - `ends`: the number of end positions.
  - `incomplete`: the ends that do not reach a solved position.
- The file given by `file=` gets one line per end position: the moves of the game up to the current position, then the moves to the end. With `which=incomplete`, only the ends that do not reach a solved position are written.

**`book findline`**

- `target=` has 65 characters: the 64 squares from a1 to h8 (`X` black, `O` white, `-` empty, as for `setboard`), then the side to move (`X` or `O`).
- Every position in a line is in the book: the current position, the target and every position between them.
- The lines only play on squares that hold a disc in the target. They pass (`pa`) when the side to move has no move and the other side has one.
- The lines are sorted by the sum of the changes of the book value (from black's point of view) at each move, smallest first.
- The answer is one `@line id=N moves=f5d6c3… cost=…` per line, then `@end id=N count=… paths=… positions=…`.
  - `paths`: the number of all the lines.
  - `positions`: the number of positions examined.
- When the target is not in the book, the answer has `notinbook=1`. When the current position is not in the book, it has `startnotinbook=1`. Both come with `count=0`.
- `n=` is 1 to 100.

**`book reducecount`**

- It answers one line: `@end id=N kept=… removed=… positions=… ms=…`.
  - `kept`: the positions kept.
  - `removed`: the positions removed.
  - `positions`: all the positions of the book.
- `x=` and `y=` are the X (0 to 129, 2 by default) and Y (0 to 254, 4 by default) of `book reduce X Y`.
- Like `book reduce`, it first recomputes the values after the current position, without changing the book. The numbers are therefore those of the book after `book reduce`.
- When the current position is not in the book, the answer has `notinbook=1`. When it is two or more moves beyond the book depth, it has `deep=1`. In both cases `kept=` is the whole book and `removed=0` (`book reduce` does not change the book).
- When the book after the current position is damaged (a link to an occupied square, or a link back to the same position; since v4.5.5-nikque.21), it does not count. It answers `@error id=N damaged=1` and `@end id=N error=1`.
- A GUI program can show these numbers and ask for confirmation before it sends `book reduce`.
- The result of the count is used by the `book reduce` after it ([8.4](08-book-maintenance.md)).
- When another analysis needs memory for its marks, the result is discarded. `book reduce` then recomputes (with the same result).

**Errors, stopping and syntax**

- An unknown word gets `@error id=N word=…` and `@end id=N error=1`. A stopped analysis gets `@cancel id=N`.
- When there is not enough memory, the answer is `@error id=N memory=1` and `@end id=N error=1`.
  - The `@move` lines before it are only a part of the analysis.
  - Up to v4.5.5-nikque.26, `book moveinfo` alone behaved differently. It left out the lines of the moves it could not analyse, and gave an `@end` without error.
- If `id=` is not an int, the answer is `@error word=id` alone, and nothing is analysed or stopped.
- The words of a command are split at spaces. A value between `"…"` is one value, spaces included (an `id=` inside it is not the number).
- The names (`id=`, …) are case-insensitive.
- A `stop id=N` that arrives after analysis N has ended, or before it has started, gets `@cancel id=N state=none`.
  - An analysis that has ended has already answered `@end id=N`.
  - One that has not started yet still runs to the end and answers `@end id=N`.
  - Since v4.5.5-nikque.26, this answer ends with `export=1` (`@cancel id=N state=none export=1`).
    - `export=1` means that the engine has the `export` of `book deviate`, `deviate2` and `deviate3` ([7.3](07-book-learning.md)).
    - An earlier version does not read that word, and expands the book.
    - So a GUI program should send a `stop id=N` at the start, and check for `export=1` in the answer before it sends an `export`.

**What was checked for this manual**

- On small test books (made for the tests, up to 19 million positions), the answers were compared with independently written computations, and the precision stages by running `book deviate`.
- `book reduce` and `book reducecount` were compared with an independently written computation (the traversal of `book deviate3` and the recomputed values). The comparison covered the positions kept and removed, the contents of the kept positions, and the leaves that `book deviate3` selects first afterwards (computed independently; 4 test books, 546 cases).
- Two damaged books, and `book reduce` with invalid words, were tried too (since v4.5.5-nikque.21).
- Connecting to a GUI program itself was not tried.

Next: [13. Troubleshooting](13-troubleshooting.md)
