# Edax 4.5.5 nikque release notes

[日本語](RELEASE-NOTES.ja.md) · [README](README.md) · [Manual](manual/en/README.md) · [Tests and measurements](docs/validation/)

User-facing changes of each version. The "Compatibility" part of each entry says whether search results, `eval.dat` and the book file format changed from the previous version. Detailed test and measurement records are linked at the end of each entry (`docs/validation/`).

- [nikque line](#nikque-line) (main line, default branch `edax-4.5.5-fixes`)
- [eval2 line](#eval2-line) (new evaluation function, branch `eval2`)
- For the C++23 port, see the [Heath (formerly newedax) release notes](https://github.com/Nikque/Heath/blob/main/RELEASE-NOTES.md)

## Versions

Each eval2 and Heath (formerly newedax) version contains all changes of the nikque version on the same row.

| nikque | eval2 | Heath (newedax) |
|---|---|---|
| nikque.33 | eval2.20 | heath.20 |
| nikque.33-pre1 | eval2.20-pre1 | heath.20-pre1 |
| nikque.32 | — | heath.19 |
| nikque.31 | eval2.19 | heath.18 |
| nikque.30 | eval2.18 | heath.17 |
| nikque.29 | eval2.17 | heath.16 |
| nikque.28 | eval2.16 | newedax.15 |
| nikque.27 | eval2.15 | newedax.12 |
| nikque.26 | eval2.14 | newedax.11 |
| nikque.25 | eval2.13 | newedax.10 |
| nikque.24 | eval2.12 | newedax.9 |
| nikque.23 | eval2.11 | newedax.8 |
| nikque.22 | eval2.10 | newedax.7 |
| nikque.21 | eval2.9 | newedax.6 |
| nikque.20 | eval2.8 | newedax.5 |
| nikque.19 | eval2.7 | newedax.4 |
| nikque.18 | eval2.6 | newedax.2, newedax.3 |
| nikque.16, 17 | eval2.5 | newedax.1 |
| nikque.15 | eval2.4 | — |
| nikque.14 | eval2.3 | — |
| nikque.10 to 13 | eval2.2 | — |
| nikque.9 | eval2.1 (first eval2 version) | — |
| nikque.2 to 8 | — | — |

---

# nikque line

## v4.5.5-nikque.33

The release of v4.5.5-nikque.33-pre1 with the better search of Heath. The time of a move and the order of the moves change. The three changes of the probcut are off by default and can be turned on in `config.ini`.

**Changes**

- All the changes of v4.5.5-nikque.33-pre1.
- Two changes have no setting and are always on. They change the time and the order of the moves only.
  - Time of a move in a game: the time of Edax a move. Only after a fail low, it may stretch up to 2 times.
  - Order of the moves: sorted with the evaluation from the depth 7; the weight shifts of the evaluation are 1,0,0.
- Three new settings. Their default is `off`. `on` is the value of Heath; the numbers can be given too.

| Setting | What it does | Value of `on` |
|---|---|---|
| `pc-slope` | Probcut of the searches to the end: a shallow score is read wider by the slope, and the margin is 0.85 times | `1.3,12` |
| `pc-dev` | A narrower probcut off the principal variation | `2,0.04,0.75` |
| `pc-mob` | The margin of the probcut depends on the mobility | `2.219,-0.184,-0.350,0,0,0,0,0,0` |

- `lazy` takes `auto`: one helper thread searches the root position deeper in a midgame search of 4 threads or more. The default is `0`.
- The test options are deleted: `-corr`, `-corr-end`, `-time-model`, `-time-ext-hard`, `-time-sep`, `-time-sep-depth`, `-time-close` and `-time-close-empties`.
- As the two changes are fixed, their options are deleted: `-time-ext`, `-sort-eval-shift` and `-sort-min-depth`.

**Compatibility**

- With the default settings too, the nodes change, as the order of the moves changes. The scores and the moves are nearly the same, but a score may change now and then in a search with probcut. A book learned with it may then have another score now and then.
- In a game with a time, the depth reached changes.
- With `pc-slope`, `pc-dev` or `pc-mob` on, the scores and the moves change, and so does a book learned with them.
- The `eval.dat` and the book file format are the same as in the version before.

**Notes**

- With a deleted option on the command line, the program shows its usage and ends. Fix the scripts that pass one.

**Not checked**: the values of the three settings come from the games of Heath; their effect and the strength in this version are not measured.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque33)

---

## v4.5.5-nikque.33-pre1

A pre-release with the book and base functions of Edax 4.7, new search settings and fixes.

**Changes**

| Command | Description |
|---|---|
| `book play n` | Extends the ends of the lines from the current position down to n empty squares. It can be resumed |
| `book extend` | Expands, from the current position, the Leaves that are no worse than any Link |
| `book exhaust` | Adds every move from the current position down to the book depth |
| `book extract all`, `book extract draw` | Writes all lines, or the best lines, to a game file |
| `base append` | Appends games to another file |

- `book play` expands several positions at the same time as set by `book-expand-tasks`, and saves at each `book-save-interval`.
- `base complete` completes several games at the same time as set by `book-store-tasks`, and can be resumed.
- Five corresponding functions were added to libedax.
- `symetry` can also be spelled `symmetry`.

- The midgame search of the AVX-512 builds is faster: 0.90 of the CPU time in a test at level 21 with one thread. Memory use grows by about 28 MB.
- Books load faster: 0.94 of the CPU time in a test with a book saved in order, 0.63 with one that is not.
- Sorting in the book and base functions is faster. The results are unchanged.

The following search settings are new. All of them are off by default. Their playing strength has not been checked.

| Setting | Description |
|---|---|
| `-lazy n` | n idle threads search the root position ahead |
| `-sort-eval-shift a,b,c`, `-sort-min-depth n` | Move ordering |
| `-lmr 1`, `2` | Searches late moves less deeply |
| `-corr on` | Corrects the static evaluation used by the ProbCut test |
| `-pc-r`, `-pc-end`, `-pc-dev`, `-pc-mob`, `-pc-gate` | ProbCut margins and test |
| `-time-model 1`, `-time-ext 1`, `-time-sep n`, `-time-close 1` | Use of time in games with a clock |
| `-split-probe off` | Does not split the search inside a ProbCut probe |

**Fixes**

- In a timed search with several threads, the whole search could stop just after its time had been extended. Split searches now use the extended time.
- Reading a `.pgn` with a pass (`pass`) could lose the rest of that game and the next game. Passes are now read.
- `stop` of the standard input, and `?` of XBoard, could be lost when they arrived just before a search began. They now stop it. This fix does not cover GGS or `edax_stop` of the DLL.
- Starting a new game while pondering waited for the pondering to end. The pondering is now stopped first.
- `book learn` could fail to continue after a stop around a save to `.store` (since v4.5.5-nikque.29, eval2.17 and heath.16). It can now continue wherever it stops. A `.learn` written by the previous versions can still be used.
- In NBoard, a `ping` just after `go` did not stop the search. It now stops it and the move is sent.
- Loading a book that was not saved in order could take up to about twice the memory of its positions. It now takes the same as a book saved in order.
- Failing to allocate memory while reading `eval.dat` ended the program without a message. It now ends with an error.
- No warning was given for the `eval.dat` of eval2. A warning is now shown.
- `base convert` and `base unique` now write nothing in these cases.
  - The games of the input or of the output file cannot be read to their end.
  - The input and the output are the same file.
- Game files are written to another file that then replaces the original, which remains if writing stops.
- Undefined bytes were written after the names in `WTHOR.JOU`. They are now zeros.
- `book extract` reports a failed write and keeps the original file.
- `book play` always searched one position at a time whatever the setting, and searched some positions more than once. Both are fixed.
- `book deviate`, `deviate2`, `deviate3`, `book enhance` and `book leaf expand` searched some positions more than once. Each is now searched once.
- `book enhance` followed the book with one thread and saved at every round whatever the setting. It now uses `n-tasks` threads and saves as set by `book-deviate-save-rounds`.
- `book extract` wrote the rest of a line again at each join. A line now ends where it joins another.
- A book with a damaged board (two discs on one square, or fewer than four discs) could be loaded. It is now refused.
- On Windows, `WTHOR.JOU` was not put in the folder of a game file given with a `\` separator. It now is.

**Compatibility**

- The previous release's `eval.dat` and books remain compatible. The book file format is unchanged.
- With the default settings, searches with a given level or depth give the same scores and best moves as the previous version.
- Timed searches with several threads can give different results because of the fix.
- `book play` and `base complete` can give results different from the previous version when they run in parallel.
- Books made by the `book deviate` family, `book enhance` and `book leaf expand` can differ slightly from the previous version, as the repeated searches are gone.
- The lines written by `book extract` end where they join, so they can be shorter and fewer than before.
- `book play` can give other values to positions that cannot be reached from the initial position.
- `book play` no longer saves at every round. An interrupted run can lose up to `book-save-interval` (60 minutes by default).
- Saving games needs free space for two copies of the file.
- `book play n` creates a `.play-to` file and `base complete` a file with `.completing` for their progress. Do not delete them before resuming.
- `book play` without a number, `book extract`, `base convert` and `base unique` keep their previous meaning, which differs from Edax 4.7.

**Notes**

- The Linux builds need glibc 2.38 or later.
- This is a pre-release for testing. Setting names and defaults may change before the final version.
- Keep a copy of important books before use.
- To continue `book learn`, load `.store` with `book load` first. Without this, `.learn` is overwritten and it may no longer be possible to continue from that `.store`.

**Not checked**: the time settings, `-pc-end` and `-split-probe` alone, playing strength, 32 threads, large production books, Android devices, running the Linux and macOS builds, and use from a GUI.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque33-pre1)

---

## v4.5.5-nikque.32

The exact endgame search is faster.

**Speed**

- At 5 and 6 empty squares, a quadrant's only empty square is played first, and a small thread-local transposition table (2^14 entries) is probed.
- With one thread, exact solves of fforum-40-59 take 97.8% of the time (431.4 s to 421.7 s), and fforum-20-39 searches 89.1% of the positions. Every score and best move is the same as in the previous release.
- The midgame search is unchanged: positions that are not solved search the same number of positions as before.

**Compatibility**

- The previous release's `eval.dat`, books, and search and learning settings remain compatible. Solve results are the same; the number of positions searched and the time change.

**Not checked.** The speed with many threads (such as 32), production books, and playing strength.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque32)

---

## v4.5.5-nikque.31

This release fixes database summaries, move-count estimates, protocol handling, and command help.

**Fixes**

- `base complete` reports the correct total number of games in its final summary.
- `estimate` computes its overall error from the total for each trial. The overall error could previously be `NaN`.
- GTP `genmove` and `reg_genmove` return `pass` at game end. Previously, `genmove` could crash if the game ended with an empty move history.
- XBoard accepts `q` during analysis. GTP and NBoard keep reading input after reporting `q` as an unknown command.
- GGS stays connected when standard input closes. The end-of-input notification is handled locally.
- `help test` now lists `solve <file> [hard-file]` for solving OBF positions.
- The help for the game commands `load` and `save` marks the filename as required.

**Compatibility**

- The previous release's `eval.dat`, books, and search and learning settings remain compatible.

**Not checked.** Production books, playing strength, and communication with a live GGS server.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque31)

---

## v4.5.5-nikque.30

This release fixes the transposition-table bounds used by exact endgame search. It also reduces work when counting the final flip and evaluating stability at the start of a search.

**Changes**

- BMI2 builds use one diagonal bit extraction in place of two for final moves outside the central 16 squares.
- AVX2 builds share the filled lines between the two stability calculations at the start of a search.
- SVE-enabled builds use SVE to update evaluation features when the vector length is at least 256 bits.

**Fixes**

- Normalizing the color of solid discs in an endgame transposition-table key could produce an invalid exact-search bound. When the winner changes, empty squares are awarded to the other side. The key now uses the actual board.

**Compatibility**

- The fix can change search results or node counts in affected positions. The evaluation function, `eval.dat`, and the book file format are the same as in the previous release.

**Not checked**. Playing strength, a production book, and SVE behavior and speed on hardware.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque30)

---

## v4.5.5-nikque.29

`book correct`, `book leaf recalculate` and `book learn` can now continue from a book saved before they finished. `book deepen` now works.

**Changes**

- `book correct` can now continue from a book saved before it finished.
  - Each solved position it checks again gets a mark in the book (the mark is saved with the book).
  - When you run it again on a book saved before it finished (the timed saves to `<book-file>.err`, or the save at exit), it searches only the positions without a mark.
  - When every solved position has been checked, all the marks are removed.
  - Up to v4.5.5-nikque.28, every run searched all the solved positions again from the beginning. On a large book this takes days, so a run that stopped lost all its work.
- `book leaf recalculate` (2, 3, 4) can continue in the same way.
  - Each leaf it recomputes gets a mark. A new run skips the marked leaves. The marks are removed when the command ends.
  - Within one run, a leaf recomputed in one round is not recomputed in the next rounds (when `book-leaf-recalculate-rounds` is 2 or more). With one-thread searches it got the same value.
- `book learn <file>` can now continue from the lines already learned.
  - Each time the book is saved to `<book-file>.store`, a small text file `<book-file>.learn` records how many lines of the game file have been learned. It also records a check value of these lines, and the number of positions, the level and the depth of the book.
  - `book learn` with the same file on this book skips the lines already learned.
  - When the whole file has been learned, `.learn` is removed.
  - Up to v4.5.5-nikque.28, a new run played all the games of the file again (with `book-randomness`, they became different games).

**Fixes**

- `book deepen` made nothing deeper (the code of the original Edax 4.3). It searched the positions whose level differs from the book again with their own level, and did not change their level. A new run searched them all again.
- Now positions whose level is shallower than the book level get the book level and are searched again with it. A new run does not search them again. Positions with a deeper level are left as they are.

**Compatibility**

- The marks are the two upper bits of the level byte of each position in the book file (the level is 0 to 60). The file size and format do not change. A book without marks is the same as before.
- **A book saved before `book correct` or `book leaf recalculate` finished holds marks. Versions up to v4.5.5-nikque.28 treat it as a damaged book and cannot load it.** A finished book has no marks.
- Other tools that read book files directly need to ignore these two bits (nikque GUI 0.4.6 does).
- `eval.dat` and the search do not change. Scores, moves and node counts are the same as in v4.5.5-nikque.28.
- Books made by the other commands are the same as with v4.5.5-nikque.28.

**Not checked**: a real large book; the strength (the search is not changed).

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque29)

---

## v4.5.5-nikque.28

Fixed a rare wrong score (and move) in exact searches at level 60 with 17 or more empties. Nothing else changes.

**Fixes**

- About 1 to 2 in 100,000 exact searches at 17 empties gave a wrong root score and move (wrong by a few discs). This happened with 1 or 4 threads and with any hash table size, on the same positions every time.
- Cause: an exact search (a level whose depth is the number of empties, with no selectivity) first searches with selectivity (probcut), then exactly. Between those iterations, a node of the midgame search at the depth where the endgame search starts (`DEPTH_MIDGAME_TO_ENDGAME`, 15 empties) was stored in the hash table as exact, because its children are searched by the endgame search.
- When that node is a pass, the opponent's search stays in the midgame search at the same depth, where probcut can cut it. That value was stored as exact and used by the exact iteration.
- A pass node is now stored with the selectivity of the search (`NWS_midgame` in `midgame.c`).

**Compatibility**

- `eval.dat` and the book file format do not change.
- Exact scores that were right do not change. Node counts change a little (fforum-20-39 with 1 thread: 77,639,600; 77,639,567 with v4.5.5-nikque.27; the same scores and moves).
- Midgame searches that do not reach the endgame depth do not change.

**Notes**

- A book with positions searched exactly by an earlier version (for example, leaves with 20 empties or fewer at level 18) may rarely hold such wrong scores.
- The same error is in Edax 4.5.5 and 4.5.6 by Toshihiko Okuhara (edax-reversi-AVX, including its master of 2026-11-03), on which this version is based. Edax 4.6 and 4.7 by Richard Delorme do not have it.

**Not checked**: the strength; a real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque28)

---

## v4.5.5-nikque.27

A book file that another program holds open when Edax starts is no longer treated as damaged and replaced. Other bugs found in a review are fixed, and the code was reorganized without changing its behaviour.

**Changes**

- libedax: added `edax_book_file_status(file)`. It tells what happened to a book file that was to be loaded.
  - 0: it may be replaced. 1: it could not be read (damaged). 2: it could not be opened (it is not replaced).
  - Called after initialization, it lets a program stop instead of starting to learn into an empty book.
- The `export` of the `book deviate` family: a file name in double quotes (`"…"`) may contain spaces.
- The result counted by `book reducecount` is released when another analysis needs the memory (a following `book reduce` then counts again, with the same result).
- The code of `book.c`, `book_analyze.c`, libedax, `play.c`, `options.c`, `base.c`, `util.c`, a few other files and the tests was reorganized without changing its behaviour (shared functions instead of code written several times).
- Manual: 4.1, 6.6, 7.3, 8.4, 8.5, 12.2, 12.4 and 13.1.

**Fixes**

- Up to nikque.26, a book file that exists but cannot be opened at startup (another program has it open without sharing, no permission to read it, …) was treated as damaged. A later save moved it to `.damaged` and replaced it with the new book.
  - Edax now waits about 2 seconds and tries again.
  - If it still cannot open the file, it starts with a new book as before, but **never saves to that file** (neither `book save` nor the save at exit). It makes no `.damaged` file either.
  - The message is `Cannot open opening book …; it is not loaded, and no save replaces it`. Once the other program has closed the file, `book load` loads it.
- When another book file failed to load, a later save replaced the first file that could not be loaded, without keeping it as `.damaged`. Now each book file that could not be loaded is remembered until it is loaded or set aside. The same file is also found under a name written another way (`./`, `\` for `/`, different case on Windows, a full path).
- libedax: a book file with a non-ASCII name (for example, Japanese) that could not be opened at startup was replaced at the end by a book of one position.
- libedax: after `edax_book_load` or `edax_book_merge`, `edax_book_deviate_export` could leave lines out without an error. It now numbers the positions of the current book for each export.
- libedax: an `edax_stop` that arrives between the two searches of `edax_hint` is no longer lost.
- libedax: `edax_book_add_board` clears the failure flag of the previous book function.
- `edax_book_new` and `book new` keep the depth between 1 and 61, as `book depth` does.
- `book import`: a line whose leaf score is outside -64 to 64 was read wrongly (300 was read as 44). It is now skipped as an unreadable line.
- `book merge`: after a merge, no move led to a position written in another orientation by another tool. That position is now stored where it is looked up, and the moves to it are linked.
- `book merge`: a file that holds the same position in two orientations is refused (`duplicated position`). In the other order it was already refused.
- The `export` of the `book deviate` family: a typo such as `exprot` was skipped and the book was expanded, and a long name was silently cut. Now a name that is too long, an unclosed quote, or any word other than `export` after the numbers is an error, and the book is not expanded.
- The `export` of the `book deviate` family: when the total loss exceeds 254, libedax gives the same warning as the commands.
- `book moveinfo`: without enough memory, it left out the lines of the moves it could not analyse and ended without an error. It now ends with `@error id=N memory=1` and `@end id=N error=1`.
- A walk of more than 2^31 items stops with an error instead of following a wrong parent.
- A time written `5:` (game time, the xboard level, the GGS clocks) was read as 5 minutes 5 seconds. It is now read as 5 minutes.
- When the folder name of the executable is too long to build the path of `config.ini`, the `config.ini` of the current folder was read, with a wrong warning. Now no `config.ini` is read, and the warning says so.
- Upstream parts:
  - The table and the statistics (`histogram`) of `weval` no longer divide by 0 (`nan`, `inf`) for a row, a column or a table with 0 or 1 position.
  - The usage text of `eval_builder` now matches how it actually reads its arguments.
  - GTP `time_left` reports a wrong color, as the other commands do.

**Compatibility**

- The search results, `eval.dat` and the book file format do not change.
- Answers differ from nikque.26 only in the cases listed in **Changes** and **Fixes** of this version.
- The source line numbers in book error messages (`book.c : book_load : …`) changed.
- libedax: when the book file of the settings could not be opened at startup, no save replaces it (`edax_book_save_checked` and `edax_book_keep_unread_file` return 0).

**Notes**: a program that saves the book itself through libedax should check `edax_book_file_status` after initialization.

**Not checked**: time and memory on a real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque27)

---

## v4.5.5-nikque.26

`book deviate`, `book deviate2` and `book deviate3` gain `export`, which writes the lines they walk to a text file without expanding the book.

**Changes**

- With `export <file> [leaf|ends|all]` after the numbers, the command writes the lines it walks from the current position, without expanding the book (for example `book deviate2 2 4 export lines.txt ends`).

| Option | Positions written |
|---|---|
| `leaf` (default) | Positions with a leaf the command would expand in its first round |
| `ends` | The ends of the lines |
| `all` | All the positions of the lines |

- One position per line, written as the moves from the start in lowercase (passes are not written).
- A position reached by several lines is written once. Its line is the one with the fewest moves (`book deviate`) or the smallest total loss in discs (`book deviate2`, `book deviate3`).
- libedax: added `edax_book_deviate_export`.
- The answer to an analysis `stop id=N` with nothing to stop is now `@cancel id=N state=none export=1`. `export=1` marks a version that supports the export.
- Manual: 7.3 (the export) and 12.4 (`export=1`). `help book` describes it too.

**Compatibility**

- Commands without `export` behave as in nikque.25.
- The answer `@cancel id=N state=none` now ends with `export=1`.
- The search results, `eval.dat` and the book file format do not change.

**Notes**: earlier versions ignore the words from `export` on and expand the book. A GUI should check for `export=1` before sending `export`.

**Not checked**: time and memory on a real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque26)

---

## v4.5.5-nikque.25

Umigame numbers marked `exact=0` are now the same whatever the build environment (the C library).

**Changes**

- Manual: 12.4.

**Fixes**

- An umigame number marked `exact=0` could differ between Windows and Linux, for example. The sort (`qsort`) did not fix the order of terms with equal values.
- When terms (the candidates of the count) are dropped because there are more than 128, terms of the same size are now put in a fixed order. The terms kept are the same in every environment.

**Compatibility**

- Umigame numbers not marked `exact=0`, and all other answers, are the same as in nikque.24.
- A number marked `exact=0` may differ from nikque.24 (both are upper bounds).
- The search results, `eval.dat` and the book file format do not change.

**Not checked**: speed on a real book; behaviour in the GUI.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque25)

---

## v4.5.5-nikque.24

The umigame numbers and position counts of the GUI's candidate-move table, `book levels` and `book missing` are taken over from the answers for the previous position, so the same counts are not computed again.

**Changes**

- Umigame numbers (`part=umi` of `book moveinfo`): when the graph of the position after a move is counted, the values of the positions 1 and 2 moves further in it are also taken out and kept. When one of those positions becomes the current position, its graph is not built again.
  - With `umi=best`, every position is kept.
  - With `umi=loss`, the positions reached with a loss of 0 are kept.
  - With `umi=range`, the positions with the same side to move as the position after the move are kept (all of them when `lo=-hi`).
  - Nothing is kept from a graph whose terms were dropped (`exact=0`). A value whose terms are dropped when it is taken out is not used either.
  - The nodes of the graphs built went down by about 38% to 46% with `umi=best`, and by about 4% to 53% with `umi=loss x=2 y=4` (on the games of 3 test books).
- Position counts (`part=count`) and `book levels`: the answer for each position (the number of positions and the number per level) is kept until the book changes. When the position after a counted move becomes the current position, `book levels` does not walk the book.
- `book missing`: when the kept precision stages already show that the range asked (`x=`, `y=`) finds nothing, it answers 0 lines without walking.
  - With the GUI defaults (`base=deviate`, ranges `1/1` to `5/5`, `x=2 y=4`) this does not apply, because there is no range 2/4.
  - It applies with `base=deviate2` or `deviate3`, or when the ranges include 2/4.
- Manual: 12.4 describes these three changes.

**Compatibility**

- The answers of `book moveinfo`, `book levels` and `book missing` are the same as in nikque.23.
- One exception: an umigame number marked `exact=0` when counted from its position alone may become the exact value kept from the previous position (not marked, smaller or equal).
- The search results, `eval.dat` and the book file format do not change.

**Not checked**: speed on a real book; behaviour in the GUI.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque24)

---

## v4.5.5-nikque.23

The precision of the GUI's candidate-move table (`stage=` of `book moveinfo`) is taken over from the answers for the positions before and after, so less of the book is walked. A book command typed without `book` now gets a hint, and the manual and documents were rewritten.

**Changes**

- Precision stages of `book moveinfo`: whatever the kept stage of the current position and the kept stages of the positions after the next move already tell is decided without walking.
  - If the current position is complete for a range, the position after a move with loss 0 is complete for that range too (with `base=deviate`, when the score of the position after the move is the negated score of the current position).
  - With `base=deviate2` and `deviate3`, the smallest loss to a leaf is kept for each position. The loss to a leaf is the total loss to its position plus the loss of the leaf move. It is built from the values of the next positions and passed on to them.
  - Whether the line of best moves reaches a solved position is decided from the answers of the next positions.
  - Walks of the ranges went down by about 14% with `deviate` and about 29% with `deviate2` and `deviate3`, and walks of the best lines by about 99% (on a test book; the reduction depends on the order in which positions are viewed).
- A book command typed at the top level (`leaf expand 30 30`, `fill2 2`, …) adds `(book command: type "book leaf expand 30 30" or "b leaf expand 30 30")` to the warning.
- Names that are also top-level commands, settings or `base` commands (`new`, `load`, `save`, `play`, `info`, `check`, …) get no hint.
- Manual:
  - Every book command is written in the form `book …` (`book leaf expand 30 30` and so on, including each row of the table in 14.2).
  - The wording was reviewed in Japanese and English (the facts are the same).
  - Mistakes in English chapters 12 and 14 (the name of `book leaf recalculate`, the rule for `"…"`, …) were fixed.
  - The save time in 9.4 was fixed (about 30 seconds → 18 to 28 seconds).
  - 12.4 and 13.2 describe the precision-stage takeover and the hint in the warning.
- Documents:
  - The README and the release notes were rewritten.
  - The test and measurement records moved to `docs/validation/`.
  - `README-NIKQUE.*` was removed. The release package contains `README.md` instead of `README-NIKQUE.*`.
  - The first comment lines of `config.ini` point to chapter 4 of the manual.

**Compatibility**

- The answers of `book moveinfo` (`stage=`, `missing=`) are the same as in nikque.22.
- The warning line for an unknown command gets a hint at its end only for names of book commands. Its first part, `WARNING: Unknown command/Illegal move: "…"`, is the same.
- The search results, `eval.dat` and the book file format do not change.

**Not checked**: speed on a real book; behaviour in the GUI.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque23)

---

## v4.5.5-nikque.22

`book reduce` and `book reducecount` are faster and use less memory on large books.

**Changes**

- `book reducecount` is about 2 times faster (on a 29 GB book of about 700 million positions, 8 threads: 462 s → 237 s).
- When `book reduce` runs just after `book reducecount` with the same conditions, it reuses the counted result and skips the computation. In the GUI flow "count, check, then reduce", the same computation no longer runs twice.
- After `book reduce` removes many positions, it frees the memory it no longer uses at once (about 1 GB on a book of about 700 million positions).
- New settings (all work as they are by default):

| Setting | Default | Meaning |
|---|---|---|
| `book-reduce-index` | `table` | `binary` looks positions up as nikque.21 does |
| `book-reduce-reuse` | `on` | Reuse the counted result in `book reduce` |
| `book-reduce-shrink` | `on` | Free the memory after the reduction. `off` saves the positions in the same order as nikque.21 |
| `book-reduce-walk` | `loss` | The order in which positions are walked. `group` can be chosen too, but it was 3–9% slower in the measurements |

**Compatibility**

- The positions kept and their contents are the same as in nikque.21.
- With `book-reduce-shrink = on`, the order of the positions inside the saved book may change (the contents are the same).
- The search results, `eval.dat` and the book file format do not change.

**Not checked**: the speed and memory of `book reduce` itself (not measured on a real book).

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque22)

---

## v4.5.5-nikque.21

Fixed bugs in `book reduce` and `book reducecount`, and rebuilt how they work.

**Changes**

- `book reducecount` now runs in parallel with the number of threads of `n-tasks` (before, 1 thread). The result is the same as with 1 thread.
- `book reduce` now removes positions in one single pass. This removes wasted work when most positions are removed.

**Fixes**

- On a damaged book (for example, moves that lead back to the same position), `book reducecount` did not end (nikque.20). This was fixed. It now returns an error (`@error id=N damaged=1`) without counting.
- Wrong input such as `book reduce 0 0x` or `book reduce 0 0 junk` removed positions as `book reduce 0 0` (nikque.20). This was fixed. It now shows a warning and does not change the book.

**Compatibility**

- On a sound book, the positions kept and the saved book are the same as in nikque.20.
- The search results, `eval.dat` and the book file format do not change.

**Correction**: "the scores of the positions kept do not change" in nikque.20 holds when compared with the book after the recomputation of the scores that `book reduce` does first.

**Not checked**: speed and memory (not measured, because the CPU was busy at measurement time), the 29 GB real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque21)

---

## v4.5.5-nikque.20

Added `book reduce`, a command that makes the book smaller, and `book reducecount`, which checks the numbers first.

**Changes**

| Command | What it does |
|---|---|
| `book reduce X Y` | Keeps only the positions in the range that `book deviate3 X Y` walks from the current position (loss of one move X or less, total loss Y or less), and removes the others. Usually used at the initial position |
| `book reducecount id=N x=X y=Y` | Counts the positions that `book reduce X Y` would keep and remove (does not change the book). For GUIs |

- After a reduction, `book deviate3 X Y` from the same position expands the same Leaf as before the reduction.
- A reduced book is saved to `book-file` when Edax is then ended (as with `book subtree` and `book prune`).
- How to use: [manual 8.4](manual/en/08-book-maintenance.md), [12.4](manual/en/12-integration.md)

**Compatibility**: The search results, `eval.dat` and the book file format do not change.

**Not checked**: the speed of the new commands, the 29 GB real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque20)

---

## v4.5.5-nikque.19

Added commands that analyse the book, for GUI programs. The search results, `eval.dat` and the book file format are the same as in nikque.18.

**Changes**

| Command | Answer |
|---|---|
| `book moveinfo id=N` | For every legal move: the book score and level, the precision stage (0–7), the number of Leaf missing for the next stage, the umigame numbers, the number of book positions after the move |
| `book missing id=N` | The moves to the Leaf missing for the next stage |
| `book levels id=N` | The number of positions per level after the current position (`scope=all`: the whole book) |
| `book bestlines id=N` | The number of lines where both sides choose only best moves (or moves within `loss=` discs). With `file=`, the moves to the ends are written to a file |
| `book findline id=N target=position` | The lines from the current position to that position that pass only through book positions. `n=` lines (1 by default), in increasing order of the total change of the score (seen from Black) |
| `stop id=N` | Stops only the analysis N (the other analyses and the commands waiting are kept) |

- The analysis commands run in a background thread and only read the book. They answer with lines that start with `@`.
- Precision stages 3–7 mean that `book deviate 1 1` to `book deviate 5 5` from the position after the move adds nothing. 0 means not in the book, 1 means the best line does not reach an exactly solved position, 2 means the range of stage 3 is incomplete.
- The criteria of the precision stages can be changed with `base=deviate2`, `base=deviate3` and `stages=`.
- The umigame number is the smallest number of ends to remember to know all the lines after the move (by default, the best moves of both sides). It is given for Black and for White. The same position reached by different move orders is counted once.
- `umi=loss` and `umi=range` can also be used as conditions of the umigame numbers. Over a wide range, a number that cannot be proved to be the smallest gets `exact=0`.
- During an analysis, `hint`, moves and other commands are accepted. Before a command that changes the book (`book store` and so on), the analysis is stopped.
- `stop` without a number works as before (it stops the thinking and clears the commands waiting).
- Analysis lines no longer come in the middle of lines of the standard output (the prompt, the moves of the game, the lines of book moves).
- Added the 5 commands to `help book`.
- The words of the analysis commands are separated by spaces. Inside `"…"`, everything, spaces included, is one value (`file="C:\a b\lines.txt"`).
- When `id=` is not an integer (in the range of int), the command does nothing and only returns `@error word=id`.
- How to use: [manual 12.4](manual/en/12-integration.md)

**Compatibility**

- The search results (scores, node counts), `eval.dat`, the book file format and the books made by learning are the same as in nikque.18.

**Notes**

- When another output (a warning and so on) is half written, an analysis line may come in the middle of that line (the GUI reads `@…` in the middle of a line).
- The extra memory used by the analysis is at most about 7% of the book (about 86 MB on a book of 19.14 million positions).

**Not checked**: the 29 GB real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque19)

---

## v4.5.5-nikque.18

The Windows executables and libraries no longer contain the folder where they were built.

**Changes**

- The Windows executables and libraries contained the location of the source files on the build PC, for the error messages. They now contain the file name only, as on Linux, macOS and Android.
- The error messages show `book.c` instead of the build folder and `src\book.c`.
- `tests/check-build-paths.py` fails on an executable that contains the build folder. The workflow that makes the Release packages runs it on the executables made in each job.
- Removed from the documents the folders of the work PC and the wording about instructions and permissions.
- The earlier Releases were fixed in the same way, and their ZIP files and `SHA256SUMS` were replaced. What was fixed: the build folders in the Windows executables (the file names are kept, nothing else changed) and the folders of the work PC in the documents.
- edax_runner 5.3.0+nikque.10 (published separately) uses the library of this release. It keeps a `book.dat` that could not be read as `book.dat.damaged` (with `edax_book_keep_unread_file`), and a second edax_runner started in the same folder stops.

**Compatibility**

- The search results (scores, node counts), `eval.dat`, the book file format and the books made by learning are the same as in nikque.17.
- The speed and peak memory do not change from nikque.17 either.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque18)

---

## v4.5.5-nikque.17

Added 2 settings to make the same book every time. Without them, Edax works as nikque.16.

**Changes**

| Setting | Values | Default | Meaning |
|---|---|---|---|
| `book-seed` | 0 to 2^64−1, `auto` | `auto` (clock) | The random seed of the book. With a number, `book learn` learns the same games every time |
| `book-reproducible` | `on`/`off` | `off` | `on` runs every search of the book commands in 1 thread, and puts the results expanded at the same time into the book in the order they were chosen. If the Link of a new position changed since it was searched, only that position is searched again |

- With `book-reproducible = on`, `book deviate`, `deviate2`, `deviate3`, `enhance` and `fill` make the same book as with `n-tasks 1`, whatever the number of threads (except the save date).
- With `book-reproducible = on`, `book store`, `add` and `learn` with several threads make the same book every time, but it differs from the book made with `n-tasks 1`.
- To make `book learn` the same every time, set a number in `book-seed`.
- `book learn` that processes several games at the same time (`book-store-tasks` 2 or more) now gives the random seed for choosing book moves per game, not per thread. The seeds come from the clock, so the choice is as random as before.
- "positions expanded at the same time do not see each other" in manual 7.6 was wrong. It now reads "they see only those that entered the book first".
- How to use: [manual 7.6](manual/en/07-book-learning.md)

**Compatibility**

- Without the settings, the books made, the speed and the memory are the same as in nikque.16.
- The search results, `eval.dat` and the book file format do not change.
- With `book-reproducible = on` and 32 threads, the time is 0.84 to 1.20 times that of before depending on the conditions, and the peak memory grows by 7 to 82%. This is because each 1-thread search has its own hash table (57 MB with `hash-table-size = auto`).

**Notes**

- With the default settings, the book may change a little each time the same command runs (behaviour from the old versions).
- One cause: the random seed for choosing among book moves with the same score comes from the clock, even with `book-randomness = 0`. So `book learn` learns different games each time it runs.
- Another cause: when expanding at the same time (`book-expand-tasks` 2 or more, `auto` included), the Link of a new position depends on the results that other threads put into the book first. Which enters first depends on the order in which the searches end.
- `book-reproducible` has no effect on the searches for the moves of a game (`play`, `go`) or on searches with a set time.
- `book store` with `book-reproducible = on` does not take over the hash table of the search of the game just before (the old behaviour with `n-tasks 1` does).

**Not checked**: the 29 GB real book, real Android and Windows ARM64 devices.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque17)

---

## v4.5.5-nikque.16

Fixed bugs found in the second reading of the source (cross-check) after nikque.15 was published (many of them from upstream). An Android build was also published.

**Changes**

- Published an Android build in addition. The Release now has a full ZIP and a ZIP for Android.
- `base unique` is about 9.8 times faster (measured on the whole command). `base save` and the recount of positions whose numbers of wins and losses stopped at the limit are faster too.
- Added the function `edax_book_keep_unread_file` to libedax. It moves the book file that the last `book load` could not read to `<file name>.damaged` (it does nothing when the file was read). It returns 1 on success, 0 when the file could not be moved. Call it before saving, so that a book that could not be read is not overwritten with an empty book.

**Fixes**

- The last game of a game file (txt, pgn) was sometimes not read. This was fixed (from upstream).
- Stopping `base correct` or `base complete` midway damaged the original game file. They now write to another file and then swap the files.
- `base save` no longer writes when it cannot read the existing file, and returns a save failure.
- Truncated edx and Wthor files and read errors are now treated as failures (before, what could be read was used). Damaged game files and Wthor files are not loaded either.
- In games with an odd number of moves, the sign of the score was reversed. This was fixed. `base complete` sometimes got the side to move wrong from the second iteration on. This was fixed too (from upstream).
- `base unique` now compares not only the moves but also the first position and the side to move.
- FEN, XBoard `setboard`, GGF `BO`, the FEN of PGN and libedax (`edax_setboard_from_obj`) accepted boards that cannot be used (fewer than 4 discs, overlaps, wrong numbers of rows or columns). They now refuse them and keep the current game.
- Fixed a locking error when several threads read and write the hash table (from upstream).
- A position whose Link could not be added for lack of memory was treated as if it had been added. This was fixed. The `book expand` family keeps the Leaf and records the failure.
- On a damaged book, `book stats` counted out of range. This was fixed (from upstream). Positions out of range are counted and shown separately.
- `book depth` is now limited to 1–61. Also fixed the search on a book without a header and the handling of positions without legal moves in `book check` and others (from upstream).
- Fixed the reading of time strings (`1:30:00` and so on), coordinates (`P@`, `@`), `mode` (kept within 0–3) and the depth in the settings file (from upstream).
- With Cassio, Edax did not end when the input ended without a newline after the last input line. This was fixed.
- Fixed races between the input thread and the exit (Cassio, XBoard, command line), and bugs of XBoard, Cassio and GGS (from upstream).
- In the 32-bit AVX2 build made with MSVC 19.44, wrong scores and stack overflows occurred. This was fixed.
- In aarch64 builds with `MOVE_GENERATOR_NEON`, the flipped discs were wrong. This was fixed (from upstream). The default configuration of the release packages does not use it.
- Fixed the places that did not build in 32-bit builds and statistics builds, and tools such as eval_builder (from upstream).

**Compatibility**

- The search results (scores, node counts), `eval.dat` and the book file format are the same as in nikque.15 (for the release packages; if you built the 32-bit AVX2 configuration yourself with MSVC, the results change because the wrong scores were fixed).
- Books made by learning: with 1 thread, the contents of the positions are the same as in nikque.15, except under conditions that use random numbers. With 32 threads, under some conditions the difference from nikque.15 is larger than the difference between old versions, and whether it comes from the fixes could not be fully checked.

**Notes**

- As before, call the libedax functions in order from a single thread. Let only one process write to one book file.

**Not checked**: real books, real Android and Windows ARM64 devices.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque16)

---

## v4.5.5-nikque.15

Fixed bugs found by reading the source again after nikque.14 was published (including some from upstream). Also improved the freeing of memory on Linux and macOS and the speed of `book import` of large texts.

**Changes**

- On Linux and macOS too, the unused part of the area that holds positions together (the pool) is freed after `book prune`, `book subtree` and `book load` (before, only on Windows).
- Adding positions by learning left unused space in the pool. After book commands and libedax calls, when the unused space is more than a quarter of the pool, the pool is compacted and freed.
- `book fill`, `fill2` and `leaf expand` use less memory for their lists of targets.
- `book import` of large texts is faster (4 million positions: 9.7 s → 7.9 s).

**Fixes**

- After `book import` of a text with boards written in a non-canonical orientation, the following `book prune` and `book subtree` did not end (from upstream). This was fixed. `book import` now puts the boards in canonical form before adding them. When two lines have the same board, the earlier line is kept. `book prune`, `book subtree` and `book merge` also no longer have paths that do not end.
- `book fix` searched non-canonical positions (book files written by other tools) again. It now puts them, with their Link and Leaf, in canonical form and keeps them. When a position with the same board already exists, the existing one is kept.
- On damaged book files (too many moves in a position, a Link move that is not a square, and so on), `book show` and `play` crashed (from upstream). This was fixed. `book load` and `book merge` show "Truncated or damaged opening book" and do not load the file when it has such positions or positions with a level above 60 (the current book is kept as it is).
- The check of the mark of the evaluation file of 8 bytes passed when only one of its two halves matched (from upstream). This was fixed. The file is read only when both match.
- When memory ran short or the file could not be read, `book import` still replaced the book with the partial book (from upstream). This was fixed. It now fails and keeps the current book.
- Memory leaked each time the size of the hash table was changed (from upstream; XBoard `memory`, and since nikque.13 also changes of `hash-table-size`). This was fixed. When the memory cannot be allocated, Edax ends with an error.
- When memory ran short, the processing that reduces Links (after `book prune`, `book fix`) could remove all the Links of a position (since an intermediate nikque version). This was fixed. It now reduces them without allocating memory.
- libedax: after `libedax_terminate`, `libedax_initialize` with another `eval-file` kept the previous evaluation data. This was fixed. The file is read again when its name changed.
- The counters used for the progress display are now read and written correctly between threads (they are used only for display).

**Compatibility**

- On a correct book, the books made by the `book deviate` family, learning from games, `book fill`, `fill2`, `leaf expand`, `book fix`, negamax and cutting are the same as in nikque.14.
- `book import` of large texts adds buckets as the positions grow. Because the order of the buckets changes, learning right after the import in the same run may make a different book.
- The behaviour changes with books that have non-canonical boards, damaged book files, evaluation files with only one half of the mark correct, and failures of `book import`.
- The search results, `eval.dat` and the book file format do not change.

**Notes**

- Without a range, the negamax of `book fill`, `fill2` and `leaf expand` is done from the initial position, as before.

**Not checked**: checks on real books, the freeing of memory on macOS, whether all races between threads are gone.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque15)

---

## v4.5.5-nikque.14

Added `book fill2`, which fills the gaps of the book, and `book leaf expand`, which expands a Leaf better than the Links, and made `book fill` faster with several threads.

**Changes**

| Command | What it does |
|---|---|
| `book fill2 n` | As `book fill n`, when the position n moves after a book position is a book position, adds the positions in between. The first move follows only the Leaf move. Without n, 2 |
| `book fill n X Y`, `book fill2 n X Y` | Only the range that `book deviate3 X Y` walks from the current position is concerned (X is the largest loss of one move, Y the largest total). The Leaf of exactly solved positions are concerned too |
| `book leaf expand` | Expands, as `book deviate` does, the Leaf of the positions whose Leaf score is larger than every Link of the position. Repeats until no such position remains. Positions without a Link are not concerned |
| `book leaf expand X Y` | Runs `book leaf expand` only in the range that `book deviate3 X Y` walks |

- `book leaf expand` was added so that the book side can fix positions whose Leaf score is larger than the largest Link, because such positions make learning with Egaroucid go wrong.
- At the start of every round, `book fill` and `fill2` add, for every move, the moves whose resulting position is in the book but which are not Links (missing Links).
- `book leaf expand` adds all the missing Links of the positions whose Leaf move leads to a book position. Add the missing Links of the other positions with `book fix`.
- When they end, no missing Link is left in the range.
- A Link is added at once to the position from which a position was added. When its Leaf was the added move, the Leaf is searched again.
- `book fill`, `fill2` and `leaf expand` run with several threads, and the number of searches at the same time follows `book-expand-tasks`. `book fill 2` went from 94.8 s to 6.74 s on a book of 83,000 positions (32 threads).
- Saving along the way follows `book-deviate-save-rounds` (the old `book fill` saved every round).
- Positions added by `book fill` are filled from in the next round (before, within the same round).
- Renamed `book leaf-recalculate`, `leaf-recalculate2`, `3`, `4` to `book leaf recalculate`, `leaf recalculate2`, `3`, `4` (the digit is written right after, as in `book leaf recalculate4 5 5`). The display becomes `Book leaf recalculate4 5 5...` too. The setting name `book-leaf-recalculate-rounds` does not change.
- libedax: added `edax_book_fill2`, `edax_book_fill_limited`, `edax_book_fill2_limited`, `edax_book_leaf_expand` and `edax_book_leaf_expand_limited`. `edax_book_fill` becomes the new `book fill`.

**Compatibility**

- The book made by `book fill n` (n of 2 or more) changes. Because it searches at the same time, it may differ a little from searching one at a time (as `book store`).
- The book made by `book fill 1` (`book fill`) is the same as in nikque.13.
- The books made by the `book deviate` family, learning from games, `book fix`, negamax and cutting are the same as in nikque.13.
- `book leaf-recalculate` and the other hyphenated names can no longer be used ("Unknown book command" is shown).
- The search results, `eval.dat` and the book file format do not change.

**Notes**

- The n of `book fill n` means "exactly n moves after". Where only one position is missing, n = 2 fills it. `book fill2 1` and distance 1 with a range (`book fill 1 5 5` and so on) do nothing ("nothing to fill" is shown).
- Without a range, `book fill`, `fill2` and `leaf expand` do the negamax from the initial position. On a book without the initial position (one cut out with `book subtree` and so on), use a range from the position that was cut out.
- After `book fill` and `fill2`, the positions with a Leaf better than the Links may increase. To fix both, run `fill2` → `leaf expand` in this order.
- On a real book of 670 million positions (level 18, 32 threads), the first round alone is expected to take about 55 minutes for `book leaf expand 5 5`, about 4 hours for `book fill2 2 5 5`, about 18 hours for `book leaf expand` (whole book) and about 4 days for `book fill2 2` (whole book). From the second round the targets decrease, but the total time is longer than the first round.
- On book files written by programs other than Edax, or damaged ones, `book fix` may fail to look positions up correctly or may corrupt memory. Work on a copy (fixed in nikque.15).

**Not checked**: the full test suite and speed measurements on the final source, complete runs on a real book (the times on the real book are estimates from the speed of a few minutes of running).

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque14)

---

## v4.5.5-nikque.13

We checked the changes of nikque.10 to 12 and fixed bugs that broke the scores of the book, overwrote book files without notice, and more. The user manual now comes with the release.

**Changes**

- The user manual (Japanese and English, 16 pages each) is in the `manual` folder ([Japanese](manual/ja/README.md), [English](manual/en/README.md); it is also in the Release ZIP).
- New setting `book-leaf-recalculate-rounds = n` (default 1): the `book leaf-recalculate` commands repeat "walk → recalculate → negamax" up to n times, and stop at the first round in which no Leaf changed. This is because recalculating Leaves also moves the range that is walked. The default 1 behaves the same as nikque.12.
- `book save` without a file name now saves to `book-file` (before, it gave an error and did not save).
- Changing `hash-table-size` (and `n-tasks` when it is `auto`) while Edax runs now rebuilds the search tables (before, only the displayed value changed).

**Fixes**

- `book correct` and `book deepen` deleted the Leaf of positions with no legal move and no Link (positions at the end of the game, and positions whose pass leads to a position not in the book), which broke the scores of the book (from upstream; the same for the parallel search of nikque.11) → they now search these positions again. A book broken by an earlier release is repaired by `book correct` of this release.
- The `book leaf-recalculate` commands did not recalculate the Leaf of pass positions (nikque.12) → they are now included. `book leaf-recalculate4` did nothing when run on a position in the deepest layer of the book (nikque.12); it now recalculates, the same as `3`.
- After `book new`, after `book load` changed the book by reading another file, and after `book import`, saving without a file name (at exit, or `book save` without a name) overwrote the file of `book-file` (from upstream) → the previous file is now kept as `<book-file>.old`. When the file read was `book-file` plus an extension (`.dev2`, `.store` and so on), it is replaced as before.
- A book cut down with `book subtree` or `book prune` was not saved at exit unless `book save` was run (from upstream) → it is now saved to `book-file`.
- `book subtree` run on a position 2 or more moves deeper than the depth of the book emptied the book (from upstream) → it now shows a warning and does nothing.
- Typing `stop` (`quit`) while a book command or a `base` command ran put the result of the stopped search into the book (from upstream) → the search is no longer stopped while these commands run. `quit` exits after the command ends. `edax_stop` during libedax's `edax_base_complete` and `edax_base_correct` was fixed the same way.
- `base complete` (`edax_base_complete`) ran with the depth of the previous search (from upstream) → it now searches with the `level` setting.
- Lines of more than 80 moves (moves plus passes) were not learned when several games were learned at the same time (`book-store-tasks` 2 or more, or `auto`) (left unfixed in nikque.10) → they are now learned. edax_runner 5.3.0+nikque.7 also fixes the case of learning one game at a time.
- Searching a pass position rarely crashed (from upstream; for example when the first problem of `-solve` is a pass position) → fixed.
- Loading games: an SGF saved from a game with a pass could not be loaded, and `load` read a game with a move that flips no disc and kept the impossible board (a `book store` on that board put an impossible position into the book). Both were fixed (both from upstream). Saved `.edx` files contained memory with no value set; it is now filled with 0.
- With `hash-table-size` set to a small number (19 or less when `level` is 18 or less), memory grew in the rounds of `book-expand-tasks = auto` with many targets (nikque.10) → the amount is back to that of nikque.9. Nothing changes with the bundled settings (`level = 18`, `hash-table-size = auto`).
- Fixed display errors (from upstream): `auto-store`, `auto-swap` and `auto-quit` in `options`, `-nan(ind)` in `analyze`, the totals at the end of XBoard, and the default values in `help options`. A failed `load` and a move that cannot be played in a `.txt` game now give a warning. `book info` shows the positions with a different level up to 10, with their count.

**Compatibility**

- The search results, `eval.dat` and the book file format do not change. No result showed that speed or memory got slower or larger than nikque.12.
- The books made by the `book deviate` commands, game learning, `book fix`, negamax and cutting down are the same as nikque.12. The exception is the rounds of `book-expand-tasks = auto` with many targets when `hash-table-size` is set to a small number: the result can change a little.
- The file of `book export` now ends with a `% depth n` line, and `book import` sets that depth. nikque.12 shows a warning and skips this line. Importing a file without a `% depth` line gives a depth 1 smaller than before.

**Notes**

- To keep the book from before cutting it down, copy the file before `book subtree` or `book prune`. Edax does not delete the `.old` files, so delete them when you no longer need them.
- A game that starts from a midgame position cannot be read back from `.txt` or `.sgf`. Save it as `.ggf`, `.pgn` or `.edx`.
- On Windows, keep the full path of `book-file` within 240 characters. If it is longer, `.store` and other files may not be saved.
- Not fixed: a Leaf can change value when recalculated, even right after it was made (this depends on the orientation of the board that is searched); `book deepen` does not raise the level of positions; positions that cannot be reached from the initial position are not negamaxed.

**Correction**

- Games of more than 80 moves come "from a position made with `setboard`" (nikque.10): they also happen from the usual initial position (games with 21 or more passes).
- For Leaf recalculation, "running the same command again changes nothing" (nikque.12): this holds when the range is the whole book.
- The work after `book subtree` is the same as `book fix` (nikque.7): since nikque.12 it does not re-link.
- A command called "`book link`" (nikque.10 and 12): there is no such command. Re-linking is a step done inside `book fix` and other commands.

**Not checked**: speed on Linux, macOS, 32-bit and ARM64; the parallel search with books of level 19 or more; `book correct` on a real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque13)

---

## v4.5.5-nikque.12

Added `book leaf-recalculate`, a command that recalculates the scores of Leaves, and made `book subtree` and `book prune` faster.

**Changes**

| Command | How it walks | Leaves it recalculates |
|---|---|---|
| `book leaf-recalculate X Y` | Same as `book deviate X Y` | The Leaves of the positions that `book deviate X Y` would choose to expand |
| `book leaf-recalculate2 X Y` | Same as `book deviate2 X Y` | The Leaves of the positions that `book deviate2 X Y` would choose to expand |
| `book leaf-recalculate3 X Y` | Same as `book deviate X Y` | All Leaves of the positions walked (the deepest layer too) |
| `book leaf-recalculate4 X Y` | Same as `book deviate2 X Y` | All Leaves of the positions walked (the deepest layer too) |

- These commands replace the old scores of the book with the values of the new evaluation function after `eval.dat` is changed. They add no positions. They walk once and end.
- The Leaves of exact-solving positions (21 empties or fewer at level 18) are not recalculated.
- When the work is done with single-thread searches (`book-expand-tasks = auto`, level 18 or less, and targets at least 32 times `n-tasks`), the result is the same whatever the number of threads.
- They save to `book.dat.leaf` to `.leaf4` every `book-save-interval` minutes and at the end. To use a save made on the way, run `book negamax`. There is no way to resume from where it stopped.
- The book changes even without changing `eval.dat`. Leaves made by multi-thread searches and values brought in by cutting out with `book subtree` change.
- libedax: added `edax_book_leaf_recalculate`, `2`, `3` and `4`.
- `book subtree` and `book prune` now mark the positions to keep with `n-tasks` threads. On a book of 661.62 million positions, cutting out to reduce the depth from 40 to 39 went from 603.6 seconds to 176.6 seconds.
- `book subtree` no longer re-links after cutting out.
- The words after `book` and `base` are accepted in capitals too (`book Deviate 5 5`). Arguments such as file names are used as written.

**Compatibility**

- In a book that already lacked Links before cutting out, they stay missing after `book subtree` (up to nikque.11 they were added). Run `book fix` if needed. For a book with no missing Links, the resulting book is the same. `book prune` does not change.
- The search results, `eval.dat` and the book file format do not change.

**Not checked**: level 19 or more; a full run on a book of 600 million positions (time); `book prune` on a real book.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque12)

---

## v4.5.5-nikque.11

Made the commands that cut down the book, `book subtree` and `book prune`, faster, and also `book correct` and `book enhance`. There are no new features or settings.

**Changes**

- `book subtree` and `book prune` no longer walk the same position again and again (from upstream). On a book of 6.49 million positions with 32 threads, cutting out to reduce the depth by 1 (`book subtree` on the initial position after `book depth 19`) went from 120.4 seconds to 5.5 seconds.
- The negamax before cutting down, and the removal of Links to deleted positions, now run in several threads (with `-n 1`, the same work as before).
- After cutting down, the memory of the deleted positions is returned (Windows version only). After reducing the depth of a real book of 661.62 million positions by 1, memory went from 31.8 GB to 24.3 GB.
- `book correct` now searches as many exact-solving positions at the same time as `book-expand-tasks` (decided the same way as the expansion of `book deviate`). On a small book of 120 positions with 32 threads, 2.0 seconds became 0.47 seconds. Memory grows by the searches run at the same time. With `book-expand-tasks = 1`, `-n 1` or `-cpu`, and when memory is short, it works one position at a time as before.
- The negamax at the start and in each round of `book enhance` now runs in several threads.
- Display: the N in `Book subtree N... done` and `Book prune N... done` no longer counts walking again, so it becomes smaller. The progress line `Book prune N to keep` appears once every 100,000 positions.

**Fixes**

- When `book subtree` was run from a position other than the initial position while also reducing the depth with `book depth`, negamax did nothing, and the counts of wins, draws, losses and lines and the score range stayed as in the book before cutting out (from upstream). When the initial position is not in the book, negamax now runs from the position that was cut out.
- In `book check`, when no move of the game was in the book, the rate of bad moves was 0÷0. It now shows 0%.

**Compatibility**

- The search results, `eval.dat` and the book file format do not change.
- The book saved by `book subtree` and `book prune` is the same as nikque.10. But when `book subtree` is run from a position other than the initial position while also reducing the depth, the counts of wins, draws, losses and lines and the score range change. The moves and scores are the same, and only the `book show` display uses these values.
- When `book correct` finds several moves with the same score, which one becomes the Leaf can change from run to run (the same in nikque.10).

**Notes**

- `book correct` deletes the Leaf of positions with no legal move and no Link (positions at the end of the game, and pass positions whose pass leads to a position not in the book), which breaks the scores (from upstream; fixed in nikque.13).
- In a book without the initial position, the negamax of `book negamax`, `book fix`, `book prune` and others does nothing, as before (`book deviate` and `book enhance` work from the current board).

**Not checked**: speed of the distributed (PGO) build; `book prune`, `book correct` and `book enhance` on a real book; `book correct` when wrong scores are found; speed on Linux, macOS, 32-bit and ARM64; `book subtree` and `book prune` on a broken book with Links that go back.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque11)

---

## v4.5.5-nikque.10

Fixed 3 bugs found after nikque.9, and made `book negamax`, `book fix` and `book merge` on large books, and the expansion of `book deviate`, `deviate2` and `deviate3`, faster. There are no new features or settings.

**Changes**

- Made multi-thread `book negamax` and the re-linking of `book fix` and `book merge` faster. On a real book of 661.62 million positions with 32 threads, the re-linking of `book fix` went from 153.7 seconds to 136.3 seconds.
- With `book-expand-tasks = auto` (the value in the bundled `config.ini`), at level 18 or less and when the targets of the round to expand are at least 32 times `n-tasks`, Edax now runs `n-tasks` single-thread searches at the same time (before, `n-tasks / 2` searches of 2 threads). On a book of 6.49 million positions with 32 threads, the number of expansions in 60 seconds grew about 1.25 times, and the peak memory went from 1,432 MB to 1,001 MB.
- Rounds with fewer targets, level 19 or more, and `book-expand-tasks` set to a number work the same as nikque.9.
- To go back to the old way, set `book-expand-tasks` to half of `n-tasks` (`16` for 32 threads).

**Fixes**

- In games of more than 80 moves counting moves and passes, Edax wrote outside the game record and broke the move count and the time left (from upstream; this happens in games with many passes). The record now holds 128 moves. The `force` sequence, saving games and libedax (`edax_play`, `edax_move` and others) were also fixed.
- Each time a string setting (`book-file`, `eval-file`, `game-file`, `name`, log file names and so on) was set again, the memory of the previous string was not freed (from upstream).
- libedax: calling `edax_stop` during `edax_bench` did not stop the remaining problems, and the resulting time was abnormal (from the original libedax). `edax_stop` now ends the benchmark. Stopped problems are not counted in the positions, nodes and time.

**Compatibility**

- `eval.dat`, the book file format, and the results and node counts of single-thread searches do not change.
- The results of `book negamax`, `book fix` and `book merge` are the same as nikque.9.
- Expanding rounds with many targets with `book-expand-tasks = auto` makes a book slightly different from nikque.9 (in one round, the Leaf moves and scores of 0.065% of all positions; most score differences are 1).

**Notes**

- `edax_get_moves` still returns up to 80 moves.
- Learning several games at the same time still uses a record of 80 moves (fixed in nikque.13).
- Positions expanded at the same time do not refer to each other (the number running at the same time grows from 16 to 32).

**Not checked**: whether the new `auto` way gives the same book every time (runs made one after another gave the same); speed on the book of 661.62 million positions with the distributed build, and on 32-bit, Linux, macOS and ARM64.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque10)

---

## v4.5.5-nikque.9

Fixed the bugs found in a full review of nikque.8 and edax_runner v5.3.0-nikque.2, and made game learning (`book learn`) a little faster.

**Changes**

- In `book learn` (libedax's `edax_book_store_games`), the threads of games that ended early are added, from the next move, to searches of games still in play (only when a game has 7 threads or fewer). With 128 games at level 18 and 32 threads, 33.6 seconds became 30.3 seconds.
- New setting `book-store-auto-save` (`on`/`off`, default `on` = the same as before): with `off`, Edax does not save to `<book file>.store` after `book store` and `book learn`. It is for programs that save the book themselves after each learning (edax_runner).
- Settings with a value that is not a number, or not on/off (`level = abc` and the like), now give a warning and are ignored.
- Lowering `n-tasks` while Edax runs keeps the numbers of `book-store-tasks` and `book-expand-tasks`.
- When Edax is started by name only from PATH and there is no `config.ini` in the working folder, it gives a warning.
- `-cpu` on Linux: the threads of book work are also bound to CPUs. With `-cpu`, searches at the same time are not used.
- Windows: `book.dat.tmp.<number>` files left by a forced exit are deleted at the next save.
- libedax: added the functions `edax_book_save_checked` (returns 1 if the save worked, 0 if not) and `edax_book_failed` (1 if the last book operation could not add positions because memory was short). Added `'2'` (failed; that line is not learned yet) to the status characters of `edax_book_store_games`.
- The Linux library is about 8% faster. The Linux and Android libraries use their own functions inside the library, even if the program that uses them has functions with the same names.

**Fixes**

- In multi-thread searches, some nodes were written to the transposition table without their moves being searched, and a request to stop the search could be lost (from upstream). With nikque.8's "stop, add threads and continue", a Leaf of the book could get a move and score that were not the best.
- Running `book merge` 2 or more times in the same run failed with "duplicated position", and could even delete the positions added by the previous merge (since nikque.3).
- On a broken book (Links that go back to the position itself or to a higher position), multi-thread `book negamax` crashed (since nikque.3).
- When `book import` failed, the current book was lost and the book file was overwritten at exit (from upstream). The current book is now kept.
- A book file that could not be read at start-up was overwritten after learning. It is now kept as `<name>.damaged` before saving.
- `base complete` and `base correct` deleted files they could not read (from upstream).
- A level out of range in `book new`, a book file name that is too long, and a broken book with positions of level above 60 could crash or hang.
- Learning: fixed 2 bugs (since nikque.7): lines longer than 255 bytes were cut, and the time left did not go down in learning with a time limit. Also removed the upper limit (127) on the randomness at the start of a line.
- When memory was short or a thread could not be created, Edax could exit or hang. It now continues with fewer searches.
- With `edax < file`, the command before `quit` was sometimes not run.
- Fixed: `book store` did not save when only Links were added, and the search log closed partway with searches at the same time.
- libedax: `edax_stop` no longer stops the search while a book-changing operation runs (partial results went into the book). Also fixed: a hang in book work during pondering, a crash on NULL arguments, and others.

**Compatibility**

- `eval.dat`, the book file format, and the results and node counts of single-thread searches do not change.
- With `book learn` (`book-store-tasks` other than 1), the moves of the games near the end of a group can change from run to run. To make the same book as earlier releases, set `book-store-tasks = 1`.

**Notes**

- In the 32-bit version, the peak memory of work that uses searches at the same time grows by about 110 to 120 MB. Books of 89.47 million positions or more cannot be read.

**Not checked**: behaviour and speed of the macOS, Android and Windows ARM64 versions; levels above 30.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque9)

---

## v4.5.5-nikque.8

This release fixes a problem with `book-store-tasks = auto`, made the default in nikque.7: it was slower than nikque.6 when there were few searches or the level was high. This is the only change.

**Changes**

- When there are fewer searches than threads, each search uses `n-tasks ÷ number of searches` threads from the start.
- When no search is left to start, the threads of finished searches are added to the searches still running, which then continue. The work those searches have done so far is not wasted.
- When there is only one search, it searches exactly as with `book-store-tasks = 1`. It uses no extra memory.
- The search work areas are created only while searches are left to start. Edax no longer creates one per thread when there are only a few short searches.
- In every condition measured, `auto` is as fast as `1` or faster (`book store` after playing 1 game at level 24: 13.4 s with `1`, 26.0 s with `auto` in nikque.7, 5.8 s and 10.6 s with `auto` in nikque.8).

**Fixes**

- With `auto` in nikque.7, in the learning of games (`book store`, `book add`, `book learn`) and in the relinking of Links (`book fix`, `book merge`, `book import` and others), the other threads sat idle until the longest search finished on its single thread. This happened when there were fewer searches than threads, or when long searches were mixed with short ones. The higher the level, the larger the effect. → The way threads are shared among the searches run at the same time was fixed.

**Compatibility**

- `eval.dat`, the book file format, the search results and the behaviour with `book-store-tasks = 1` do not change.
- This was not a problem that damages the content of a book. Books made with nikque.7 can be used as they are.
- The results of searches that finished on a single thread are the same as in nikque.7.
- Searches that got extra threads on the way, and searches run with several threads from the start, may give slightly different results from run to run (as with any multi-threaded search). `auto` in nikque.7 gave the same book on every run.

**Notes**

- `auto` uses more memory than `1`. With 32 threads, the hash tables of the searches run at the same time take up to about 450 MB in total at level 18 or below, about 0.9 GB at level 21 or below, and about 1.8 GB above that (in proportion to the number of threads).
- If you do not want more memory to be used, set `book-store-tasks = 1`.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque8)

---

## v4.5.5-nikque.7

This release adds libedax, a library that lets other programs use Edax, and learns games with several threads. `book fix` is faster too.

**Changes**

- libedax is added. It has the same 93 functions as the libedax of lavox and sensuikan1973, with the same layout of the data passed. Programs for libedax (libedax4dart, edax_runner and others) work by just replacing the library file.

| File | For |
|---|---|
| `libedax-x64.dll` (Linux: `libedax-x86-64.so`) | Any x86-64 CPU |
| `libedax-x64-v3.dll` (`libedax-x86-64-v3.so`) | CPUs with AVX2 |
| `libedax-x64-v4.dll` (`libedax-x86-64-v4.so`) | CPUs with AVX-512 |
| `libedax.universal.dylib` | macOS (both Apple silicon and Intel) |
| `libedax-arm64-v8a.so`, `libedax-armeabi-v7a.so` | Android (ARM64, 32-bit ARMv7) |

- libedax reads its settings from `edax.ini` in the working folder, then `config.ini`, then the arguments of `libedax_initialize` (later ones take priority).
- Functions added to libedax: `edax_book_deviate2`, `edax_book_deviate3`, `libedax_cpu_level`, `edax_book_store_games` (plays and learns several games at once), `edax_book_store_tasks`.
- The single-thread endgame search of libedax is 1.3 to 1.7 times as fast as the original libedax (Edax 4.4), with about half the peak memory.
- The new setting `book-store-tasks` makes the learning of games (`book store`, `book add`, `book learn`, and `edax_book_store_games` in libedax) use several threads.

| `book-store-tasks` | Behaviour |
|---|---|
| `auto` (default; also in the bundled `config.ini` and when there is no `config.ini`) | Learns `n-tasks` games at the same time (1 thread per game) |
| A number n (2 or more) | Learns n games at the same time (`n-tasks / n` threads per game) |
| `1` | Same behaviour as nikque.6, giving the same book |

- Learning 128 games (32 logical CPUs) took 112.9 s with `1` and now takes 39.4 to 41.6 s with `auto`. This is about as fast as starting many edax_runner processes at once and merging later, but with one process and one book.
- New command `book learn <file>`: for each line of the file, it plays the moves, has Edax play itself to the end of the game, and runs `book store` (the same format as the "Edax vs Edax" lines of edax_runner). Write the moves as `f5d6c3`, or a `book-randomness` value and the moves as `2,f5d6c3`. Empty lines, lines that start with `#` and lines that contain `//` are skipped.
- `book fix` (also used by `book import`, `correct`, `prune`, `subtree` and after `book store`) checks positions, relinks Links and sorts with several threads. On a book of 6.49 million positions with 32 threads, it takes 2.3 s instead of 22.9 s.
- The Leaf searches needed to relink Links (`book fix`, `book merge`, `book import` and others) are also run at the same time when `book-store-tasks` is not 1.
- For macOS, `mEdax-arm64` and `libedax.universal.dylib` for Apple silicon are added.

**Fixes**

- When a learning command saved its progress to another file (`.store`, `.dev` and so on), the book was marked as "saved", so it was not saved to `data/book.dat` if you quit without `book save` (since nikque.2). → Saving progress no longer marks the book as "saved".
- "Fixing book..." of `book fix` showed nothing until the end when there were no positions to fix. → It now shows the number of positions checked every second (for example `Fixing book...3137536/6491163 positions checked`).

**Compatibility**

- The search results, `eval.dat` and the book file format do not change.
- **The book made by learning games differs slightly from nikque.6.** The difference is about the same as when the number of threads was changed in the old learning. `auto` gives the same book on every run. If you need the same book as nikque.6, set `book-store-tasks = 1`.
- The checks and Link relinking of `book fix` give the same book as processing one at a time. When Leaf searches are redone at the same time, the Leaves may differ slightly.
- `book deviate`, `deviate2`, `deviate3`, `enhance`, `play` and `book fill` do not change.
- edax itself is not affected. The libedax code is only included when the library is built.

**Notes**

- `auto` uses a lot of memory (learning 128 games, the peak memory goes from 290 MB to 722 MB). Each search run at the same time has its own hash table (14 MB each at level 18 or below, 28 MB at level 21 or below, 57 MB above that).
- With `book store` of a single game, or with small `book-store-tasks` values such as 2 or 4, the gain is small (nikque.8 fixed the slowness when there are few searches).
- In `book learn`, what is learned from the games of a group played at the same time is not used in the games of that group.
- The default level of libedax is 18 (21 in the original libedax). `edax_book_merge` goes as far as relinking Links, fixing and negamax (the original libedax only adds positions).
- There is one libedax per process. Call the functions in order from a single thread. Have only one process write to a given book file (only the one saved last remains).

**Not checked**: libedax for Android on real devices (only the build was checked).

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque7)

---

## v4.5.5-nikque.6

This release lets `config.ini` set the startup settings and makes `config.ini` easier to write by hand. The default level changes from 21 to 18.

**Changes**

- `config.ini` (or the command line) can set the following values at startup. The values in the bundled `config.ini` are the same as the defaults when there is no `config.ini`.

| Setting | Default | Meaning |
|---|---|---|
| `level` | `18` | Search level at startup (0 to 60) |
| `n-tasks` (`n`) | `auto` | Number of search threads. 1 to the number of logical CPUs; `auto` is the number of logical CPUs |
| `book-depth` (new setting) | `auto` | Book depth at startup. `auto` keeps the depth of the book file; 1 to 60 sets that depth |
| `book-usage` | `on` | Whether to play from the book |

- On the command line you can write `-n auto` and `-book-depth <n|auto>`.
- You can check the values of the settings with `options` at the prompt (a "book depth at startup" line is added).
- The format of `config.ini` and `edax.ini` is more relaxed.
  - `level = 18`, `level=18`, `level 18` and `set level 18` are all the same. Tabs, full-width spaces and "＝" can be used too.
  - Names and values such as `on`/`off`/`auto` are not case-sensitive. Spaces, `_` and `-` in names are treated the same (`book depth` is the same as `book-depth`).
  - When `=` is written, the rest of the line is the value (so file names with spaces can be written).
  - Everything from `#` to the end of the line is a comment. A UTF-8 BOM and CRLF line endings can be used.
  - Unknown names give a warning at startup with the file name and line number (for example `WARNING: config.ini:7: unknown or incomplete setting "levle" ignored`).
- The limit on the number of legal moves in one position goes from 33 to 34. Some positions that cannot be reached from the initial position have 34 moves; they can now be handled when entered with `-solve` or `setboard`.

**Fixes**

- "Linking book..." and "Fixing book..." of `book merge` and `book fix` did not show their progress (since nikque.3). → They now show the progress (number of positions) every second. While `book merge` reads files, `Checking book ...` and `Merging book ...` also show the number of positions read.

**Compatibility**

- **The built-in default level changes from 21 to 18.** `-solve` and game searches without `-l` give different results from before. With `-l`, they are the same.
- The search results with the same options, `eval.dat` and the book file format do not change.
- The change of the legal-move limit does not change the memory or speed of the book.
- The fix of the progress display does not change the content of the book.

**Notes**

- `level` in `config.ini` and `edax.ini` only sets the level at startup. It does not limit the search in timed games (`-t`, `-move-time`). To set a limit, use `-l` on the command line or `level` at the prompt (`level` in a settings file read with `-o` is also a limit).

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque6)

---

## v4.5.5-nikque.5

This release makes timed games stronger by using up the available time, and cuts the memory of the book by about 14%.

**Changes**

- In timed games (`-t`, `-move-time`), the search limit is now 60 when no level is given. Before, the search stopped at the default level 21, using only about half of the time in 16-second games and less than 20% in 64-second games.
- When a level is given with `-l` or `level`, that level is the limit, as before. The same goes for `sd` in xboard and `depth` in NBoard.
- The time allocation is now estimated from the measured search speed (times 2). When `-speed n` is given, that value is used as before (`-speed auto` returns to automatic).
- Together, these two changes make 16-second single-thread games +90 Elo stronger.
- When `-nps` is given, the remaining time of the game is also reduced by the number of nodes (for comparisons in which games can be reproduced).
- One position of the book in memory goes from 56 bytes to 48 bytes. With a book of 657 million positions, the memory after loading goes from 35.55 GiB to 30.64 GiB.
- `book deviate`, `deviate2` and `deviate3` use a table of 1 byte per position only while they choose positions (about 0.9 GB for 657 million positions).
- New settings:

| Setting | Default | Meaning |
|---|---|---|
| `book-expand-tasks` | `auto` in the bundled `config.ini`; `1` when there is no `config.ini` | `auto` sets the number of positions expanded at the same time from the level of the book. Each position's search uses 2 threads at level 18 or below, 4 at level 24 or below, and 8 above that (16 positions with 32 threads at level 18) |
| `probcut-model` | `standard` | `refit` is experimental: a ProbCut error model refitted on measurements |

**Compatibility**

- The results of fixed-level searches (book learning, `-solve`, games with a given level) do not change.
- The book file format and the saved content do not change. Books can be read and written both ways with earlier versions.
- With a `book enhance` error larger than 63, the results may differ from before. In positions whose upper or lower score bound goes beyond ±127, the bound is clamped to ±127. With an error of 63 or less, nothing changes.
- With `probcut-model = refit`, the search results change, and the book made also differs from `standard`. Check it well before mixing it with a `standard` book.
- The book made with `book-expand-tasks = auto` is not the same as when positions are expanded one at a time (read the notes on `book-expand-tasks` in nikque.4).
- `eval.dat` does not change.

**Notes**

- When a book with out-of-range upper or lower bounds is loaded (one made by `book enhance` with a large error in an earlier version), those values are clamped to ±127.
- The limit of the cumulative loss (the second argument) of `book deviate2` and `deviate3` is 254. Larger values are treated as 254, with a warning.
- Choosing positions in `book deviate3` and others is slightly slower (first round of `book deviate3 2 5` on a real book: 12.6 s → 15.5 s). The positions chosen are the same.
- Using `refit` for book learning makes the learning of the opening and midgame slower.

**Not checked**: The speed of `book-expand-tasks = auto` was measured only at level 18.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque5)

---

## v4.5.5-nikque.4

This release makes search and book learning faster, and raises the limit on the number of positions in a book to about 4.3 billion. It adds a setting for learning that expands several positions at the same time.

**Changes**

- Book learning no longer clears the hash table each time it searches a position. The larger the hash table, the larger the gain (level 18, 1 thread, `-h 26`: 180 s → 108 s).
- The evaluation function of the x86-64-v4 build (AVX-512) is faster. Single-thread midgame search is 7 to 13% faster.
- The Windows executables (except ARM64) are built with PGO (compiler optimization that uses how the program runs).
- The limit on the number of positions in a book goes from 2,147,483,647 to 4,294,967,295.
- New settings:

| Setting | Default | Meaning |
|---|---|---|
| `book-expand-tasks` | `1` | `book deviate`, `deviate2`, `deviate3`, `enhance` and `play` expand n positions at the same time. Each uses `n-tasks / n` threads and its own hash table. Command line: `-book-expand-tasks n` |
| `hash-table-size` | `auto` in the bundled `config.ini`; 21 when there is no `config.ini` | `auto` chooses from the number of threads (21 for 1 to 3, 22 for 4 to 15, 23 for 16 to 63; at most 25 and at most 1/32 of the memory). Command line: `-h auto` |

- With `book-expand-tasks = 16`, `book deviate3 2 5` on a book of 657 million positions (10,000 expansions, 32 threads) went from 832 s to 174 s.

**Fixes**

- When search threads were freed right after being created, Edax could miss the stop signal and wait forever, or free a task before its thread started running. → They are now stopped reliably.

**Compatibility**

- With the same options, the search results do not change. The book file format does not change either.
- When `hash-table-size = auto` changes the size of the hash table, the search results also change slightly, as when `-h` is changed.
- With `book-expand-tasks` at 2 or more, the book made is not the same as when positions are expanded one at a time. The positions expanded and the Link moves were the same, but some Leaves and scores differed by 1 or 2.
- Books of 2,147,483,647 positions or fewer are saved byte for byte the same as before. Larger books cannot be read by earlier versions or by upstream Edax.

**Notes**

- When `book-expand-tasks` is 2 or more, positions expanded at the same time do not see each other. For example, when positions of the same round lead to the same new position, no Link is made at that time (it may be made in a later round).
- In rounds with fewer than n positions to expand, fewer searches run at the same time.
- The higher the level, the smaller `book-expand-tasks` should be (with 32 threads, start with 4 or 8 and compare the speed).
- Each of the n searches of `book-expand-tasks` has its own hash table. When `hash-table-size` is given as a number, n times that memory is used.
- `book-expand-tasks` cannot exceed `n-tasks`. If you play or analyze games while learning, lower `n-tasks` (and `book-expand-tasks`).
- A book of 4.3 billion positions needs about 240 GB of memory.

**Not checked**: `book-expand-tasks` was measured only at level 18. The Windows ARM64 and Android builds were only built, not run.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque4)

---

## v4.5.5-nikque.3

This release makes loading, saving, the `book deviate` commands and `book merge` much faster on large books of hundreds of millions of positions, and uses less memory. It fixes 11 bugs and adds a setting that saves automatically after `book merge`.

**Changes**

- On a book of 657 million positions (28.95 GB), loading went from 445.6 s to 48.9 s, the memory after loading from 58.4 GiB to 35.5 GiB, and saving from 657.2 s to 28.3 s.
- One round of `book deviate 2 4` (0 positions to expand) went from 2,909.6 s to 142.0 s.
- Negamax, the choice of positions to expand in `book deviate`, `deviate2` and `deviate3`, and the Link rebuilding, checks and sorting of `book merge` run in parallel on the `n-tasks` threads. The results are the same.
- `book merge` reads the source file twice instead of loading the source book. If the source is broken, the target is not changed.
- `book merge` uses the Leaves that the source has already searched at the same level, and skips searching them again. A merge of two books of 660 million positions went from 4 h 33 min and 117.2 GiB to 263 s and 37.0 GiB.
- New setting `book-merge-auto-save` (default `on`; command line `-book-merge-auto-save on/off`): when `book merge` succeeds, the book is saved to `<book file name>.mrg` (`data/book.dat.mrg` by default) and `Merged book saved to data/book.dat.mrg` is shown.
- When you play or analyze games while learning the book, you can set the number of threads with `n-tasks` (`-n`).

**Fixes**

- Links to positions not in the book made negamax and others crash. → They are skipped, and `book fix` removes those Links.
- With input from a pipe, `quit` arriving while the book was loading caused a crash. → Fixed.
- `book fill` could write to freed memory while adding positions, and crash. → Fixed.
- When memory ran out, positions were lost silently. → This is now reported and the learning stops.
- When saving the book, the old file was replaced before the content of the file was committed to disk. → It is now replaced after the content is committed.
- Among the positions added by `book merge`, those that cannot be reached from the initial position kept a score of ±127. → Fixed.
- Reading an empty, truncated or `.edx` file with illegal moves erased the current game. → Loading is aborted and the game is kept.
- On Windows, the clock value wrapped around about 49.7 days after the OS started. → Fixed.
- One uninitialized byte was written in the date and time of the book header. → 0 is written.
- Above about 2.1 billion positions, the number of positions overflowed. → Adding more is refused.
- The number of buckets was limited to 2^26, so lookups got slow above 1 billion positions. → It is now set from the number of positions.

**Compatibility**

- The book file format does not change. Earlier books can be read and written as they are.
- In `book merge`, the scores of positions that used Leaves from the source, and of their parent positions, may change slightly (most differences are 1 or 2). The set of positions, the Link moves and the search levels are the same. In nikque.2 too, the scores of positions searched again vary by about the same amount from run to run.
- The scores of positions added by `book merge` that cannot be reached from the initial position change.
- In a `book merge` where the number of positions after the merge exceeds 32 times the number of buckets of the target, the order of the saved positions changes. The content of the positions is the same.
- With `book-merge-auto-save = off`, `book merge` behaves exactly as before.
- `book merge` of upstream v4.5.5 does not relink Links and asks you to run `book fix`. This version goes as far as Link rebuilding, repair, negamax and sorting within the merge.

**Notes**

- Auto-save does not overwrite the original book (`data/book.dat`). To use the `.mrg` file, load it with `book load data/book.dat.mrg` or rename it. Nothing is saved if the merge fails. Saving on exit is the same as before.
- While `book load` loads another book, the memory of two books is used for a while, so that the current book is kept if loading fails.
- A round of `deviate2` or `deviate3` with many positions to expand is not much faster, as most of it is one search at a time.
- The 32-bit Linux build needs an i486 or later CPU. A 32-bit macOS build is still not distributed.

[Tests and measurements](docs/validation/nikque.en.md#v455-nikque3)

---

## Up to v4.5.5-nikque.2 (the first corrected release)

This is the first release, based on upstream Edax v4.5.5. It adds `book deviate2` and `book deviate3` and settings for automatic book saving, and fixes 18 bugs.

**Changes**

- This fork is based on the upstream tag `v4.5.5` (`4cde6ff588f0eade07fcba0c7f02d5cd0cacd4ee`).
- Two book learning commands are added. All of them follow Links from the current position and expand the Leaves that meet the conditions.

| Command | Conditions |
|---|---|
| `book deviate 2 4` | The usual relative error and the absolute error from the starting position (as before) |
| `book deviate2 5 5` | Loss of at most 5 for one move, and at most 5 for Black and White combined. Leaves solved exactly at the learning level of the book are excluded |
| `book deviate3 5 5` | Same conditions as `deviate2`, but Leaves solved exactly are included |

- New settings (`config.ini`):

| Setting | Meaning |
|---|---|
| `book-save-interval` | Saves at a time interval during expansion (in minutes). `0` turns it off |
| `book-deviate-save-rounds` | Saves every how many rounds with additions (common to the three deviate commands). `1` (default) saves every round, `0` only when the learning ends, `N` every N rounds and when the learning ends |

- The distribution (the Release ZIP) contains rebuilt executables for Windows, Linux, macOS x64 and Android, `bin/data/eval.dat`, an initial `bin/data/book.dat` and problem sets. `eval.dat` is the one from the upstream v4.5.5 distribution, unchanged.

**Fixes**

| Area | Fix |
|---|---|
| Book learning | The usual `book deviate` gets the starting position again after the first expansion. Before, it read freed memory, and the learning could stop or positions could be missed |
| Book learning | `book enhance` continues until the expansion settles, and saves after every round with additions |
| Book merge and saving | A book is written to a temporary file and checked, and replaces the target only once it is fully written |
| Book merge and saving | Books that are truncated or contain extra records are refused. When `book load` or `book merge` fails, the current book is kept, and a broken startup book is not overwritten |
| Files | On Windows, `.edx` files are read and written in binary mode |
| Files | Unsupported extensions for saving a game are refused before the file is opened |
| Files | File names shorter than 4 characters are handled correctly |
| Time control | XBoard `level 0` no longer divides by zero. The Fischer increment is added after the move |
| Time control | Game analysis limits how far it goes back in the history, and initializes the pass history |
| PGN and GGF | PGN output reserves enough space for a dense FEN |
| PGN and GGF | The FEN tag of PGN is read, and time tags can be read and written |
| PGN and GGF | GGF fields of maximum length get space for the terminator |
| PGN and GGF | Reading PGN moves stops at 60 moves |
| Commands and communication | Parsing of string fields and commands respects the size of the destination |
| Commands and communication | GTP `reg_genmove` returns the actual result |
| Commands and communication | GTP `time_left` refuses invalid colours |
| Commands and communication | GTP response IDs are initialized for each command and returned in the right order, also on errors |
| Commands and communication | NBoard node counts and elapsed seconds are shown in the correct format |

**Compatibility**

- The selection conditions of the usual `book deviate` do not change.
- Unlike upstream v4.5.5, `book merge` goes as far as rebuilding Links, repairing inconsistent positions (including old `nomove` Leaves), recomputing scores and sorting moves. Positions already in the target are not overwritten.
- Unlike upstream v4.5.5, when the source of `book merge` is missing or broken, the merge is aborted and the current book is kept.
- Unlike upstream v4.5.5, if nothing has changed since the book was saved with `book save`, it is not saved on exit.

**Notes**

- The fix of the usual `book deviate` does not automatically fill in positions missing from existing books.
- While the book is being saved, free disk space of about the size of the book is needed for the temporary file. If saving fails, the previous book remains.
- On failure or interruption, the learning done since the last save may be lost. Take care when you combine `book-deviate-save-rounds = 0` with saving at a time interval turned off.
- The usual `book deviate` counts expansion for both Black and White as one round. `deviate2` and `deviate3` count one expansion as one round.
- `todo` of `deviate2` and `deviate3` is the number of Leaf positions chosen for that round (not the number of games). Intermediate books get `.dev`, `.dev2` or `.dev3`.
- `mEdax-x86` for old 32-bit macOS is not distributed. The SDK of current Xcode cannot link the corrected executable for i386.
- Start the executables from `bin/`, or give `-eval-file`. The AVX-512 build needs a CPU that supports x86-64-v4.
- When you redistribute the executables, include the source, a notice of these changes and `LICENSE`.

[Tests and measurements](docs/validation/nikque.en.md)

---

# eval2 line

The nikque line plus a new evaluation function (mobility weights), published from the `eval2` branch. Each version contains all changes of the nikque versions listed in the "Versions" table above.

## v4.5.5-eval2.19

This release includes the changes from [v4.5.5-nikque.31](#v455-nikque31).

**Compatibility**

- The previous release's `eval.dat`, books, and search and learning settings remain compatible.

**Not checked.** Production books, playing strength, and communication with a live GGS server.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval219)

---

## v4.5.5-eval2.18

This release includes the changes from [v4.5.5-nikque.30](#v455-nikque30).

**Compatibility**

- The fix can change search results or node counts in affected positions. The evaluation function, `eval.dat`, and the book file format are the same as in the previous release.

**Not checked**. Playing strength, a production book, and SVE behavior and speed on hardware.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval218)

---

## v4.5.5-eval2.17

The changes of v4.5.5-nikque.29 are included ([nikque.29](#v455-nikque29)). `book correct`, `book leaf recalculate` and `book learn` can now go on from a book saved before they ended. `book deepen` now works.

**Changes**

- `book correct` and `book leaf recalculate` (2, 3, 4) can now go on from a book saved before they ended. Each position or leaf that is done gets a mark in the book, and a new run skips the marked ones. The marks are removed when the command ends (the same as nikque.29).
- `book learn <file>` can now go on from the lines already learned (the same as nikque.29).
- `book deepen` now works. Positions whose level is shallower than the level of the book get the level of the book and are searched again with it (the same as nikque.29).
- There is no new change of the eval2 line alone.

**Compatibility**

- The marks are the two upper bits of the level byte of each position in the book file (the level is 0 to 60). The file size and format do not change. A book without marks is the same as before.
- **A book saved before `book correct` or `book leaf recalculate` ended holds marks, and the versions up to v4.5.5-eval2.16 read it as a damaged book and do not load it.** A finished book has no marks.
- Other tools that read book files directly need to ignore these two bits (nikque GUI 0.4.6 does).
- `eval.dat` and the search do not change. Scores, moves and node counts are the same as v4.5.5-eval2.16 (fforum-20-39 with 1 thread: 78,733,311 nodes).
- The books made by the other commands are the same as with v4.5.5-eval2.16.

**Not checked**: a real large book; the strength (the search is not changed).

[Tests and measurements](docs/validation/eval2.en.md#v455-eval217)

---

## v4.5.5-eval2.16

The changes of v4.5.5-nikque.28 are included ([nikque.28](#v455-nikque28)). An exact search at level 60 from 17 empties on could, rarely, give a wrong score (and move); this is fixed. Nothing else is changed.

**Fixes**

- Exact searches rarely gave a wrong score and move at the root (wrong by a few discs). The fix is the same as nikque.28.
- The same error is in Edax 4.5.5 and 4.5.6 of Toshihiko Okuhara, from which this version comes.

**Compatibility**

- `eval.dat` and the book file format do not change.
- Exact scores that were right do not change. Node counts change a little (fforum-20-39 with 1 thread: 78,733,311 nodes, 78,733,223 with v4.5.5-eval2.15; the same scores and moves).
- Midgame searches that do not reach the endgame depth do not change.
- A book with positions searched exactly by an earlier version (for example the leaves of 20 empties and less at level 18) can, rarely, hold such wrong scores.

**Not checked**: the strength; a real book.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval216)

---

## v4.5.5-eval2.15

The changes of v4.5.5-nikque.27 are included ([nikque.27](#v455-nikque27)).

**Changes**

- The fix that keeps a book file held by another program at start-up from being replaced is included.
- `edax_book_file_status` of libedax is included.
- The fixes of `book import`, `book merge`, the `export` of the `book deviate` family, `book moveinfo` when memory runs out, and others are included.
- The rearranged code is included.
- All of these are the same as nikque.27.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are the same as eval2.14.
- The answers differ from eval2.14 only in the cases of nikque.27.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval215)

---

## v4.5.5-eval2.14

The changes of v4.5.5-nikque.26 are included ([nikque.26](#v455-nikque26)).

**Changes**

- The `export` of `book deviate`, `book deviate2` and `book deviate3` is included. It writes the lines the command walks to a file, without expanding the book.
- `edax_book_deviate_export` of libedax is included.
- The mark `@cancel id=N state=none export=1` is included.
- All of these are the same as nikque.26.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are the same as eval2.13.
- The commands without `export` are the same as in eval2.13.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval214)

---

## v4.5.5-eval2.13

The changes of v4.5.5-nikque.25 are included ([nikque.25](#v455-nikque25)).

**Changes**

- The fix that makes umigame numbers marked `exact=0` the same whatever the build environment is included (the same as nikque.25).

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are the same as eval2.12.
- Except the numbers marked `exact=0`, the answers are the same as in eval2.12.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval213)

---

## v4.5.5-eval2.12

The changes of v4.5.5-nikque.24 are included ([nikque.24](#v455-nikque24)).

**Changes**

- The umigame numbers, the numbers of positions, `book levels` and `book missing` are taken over from earlier answers (the same as nikque.24).
- Manual: 12.4 has the same text as the nikque manual.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are the same as eval2.11.
- The answers of `book moveinfo`, `book levels` and `book missing` are the same as in eval2.11.
- The umigame numbers marked `exact=0` have the same exception as in nikque.24.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval212)

---

## v4.5.5-eval2.11

The changes of v4.5.5-nikque.23 are included ([nikque.23](#v455-nikque23)).

**Changes**

- The precision stages of `book moveinfo` are taken over from earlier answers (the same as nikque.23).
- A book command entered without `book` now gets a hint (the same as nikque.23).
- Manual: every book command is written in the form `book …`.
- Manual: the review of the wording of the nikque manual is applied to the eval2 manual too. The version described, the names of the versions, the version of `eval.dat` and the places of the records follow eval2. Figures measured with nikque versions say so.
- The rewriting of the documents in nikque.23 (README, release notes, places of the records, `README-NIKQUE.*` removed) is not in eval2.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are the same as eval2.10.
- The answers of `book moveinfo` are the same as in eval2.10.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval211)

---

## v4.5.5-eval2.10

The changes of v4.5.5-nikque.22 are included ([nikque.22](#v455-nikque22)).

**Changes**

- `book reduce` and `book reducecount` now take less time and memory, and the new settings `book-reduce-index`, `book-reduce-reuse`, `book-reduce-shrink` and `book-reduce-walk` can be used (the same as nikque.22).
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.9.
- The book file format is not changed. The results of `book reduce` are those of nikque.22.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval210)

---

## v4.5.5-eval2.9

The changes of v4.5.5-nikque.21 are included ([nikque.21](#v455-nikque21)).

**Changes**

- The parallel `book reducecount` and the new way `book reduce` does its work are included (the same as nikque.21).
- There is no new change of the eval2 line alone.

**Fixes**

- `book reducecount` did not end with a damaged book, and `book reduce` removed positions even with wrong input. Both were fixed (they came in eval2.8; fixed in the same way as nikque.21).

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.8.
- With a sound book, the positions kept are those of eval2.8. The book file format is not changed.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval29)

---

## v4.5.5-eval2.8

The changes of v4.5.5-nikque.20 are included ([nikque.20](#v455-nikque20)).

**Changes**

- The command `book reduce`, which makes the book smaller, and `book reducecount`, which counts first, can be used (the same as nikque.20).
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.7.
- The book file format is not changed.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval28)

---

## v4.5.5-eval2.7

The changes of v4.5.5-nikque.19 are included ([nikque.19](#v455-nikque19)).

**Changes**

- The book analysis commands for GUIs `book moveinfo`, `book missing`, `book levels`, `book bestlines` and `book findline`, and `stop id=N`, can be used (the same as nikque.19).
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.6.
- The book file format is not changed.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval27)

---

## v4.5.5-eval2.6

The changes of v4.5.5-nikque.18 are included ([nikque.18](#v455-nikque18)).

**Changes**

- The executables no longer hold the folder in which they were built (the same as nikque.18).
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.5.
- The book file format, and the books made with `book-reproducible`, are those of eval2.5.

**Notes**

- The 32-bit `wEdax-x86-sse.exe` tends to be a little slower than eval2.5 (`book learn` with 32 threads 1.007 times, `book leaf expand` 1.013 times; `book leaf expand` with 1 thread 1.058 times). We think this comes from a difference of the PGO build. The speed of the other executables and the peak memory are not changed.

**Not checked**: why `wEdax-x86-sse.exe` tends to be slower.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval26)

---

## v4.5.5-eval2.5

The changes of v4.5.5-nikque.16 and 17 are included ([nikque.17](#v455-nikque17), [nikque.16](#v455-nikque16)).

**Changes**

- nikque.16: fixes of the errors found by the second cross-check, faster `base unique` and others, the libedax function `edax_book_keep_unread_file`, the EOF of Cassio, and inlining in the MSVC 32-bit AVX2 build.
- nikque.17: the settings `book-seed` and `book-reproducible`, to make the same book every time (the default behaviour is not changed).
- There is no new change of the eval2 line alone. The sources other than the 4 files of the evaluation function are those of nikque.17.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.4.
- The changes about the book are those of nikque.16 and 17.

**Not checked**: the speed and strength of this version (the evaluation function is that of eval2.1), the full set of book tests for eval2 alone, the real book of 29 GB, runs on real Android, Windows ARM64 and macOS devices.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval25)

---

## v4.5.5-eval2.4

The changes of v4.5.5-nikque.15 are included ([nikque.15](#v455-nikque15)).

**Changes**

- The fixes of the errors found by the cross-check, and the improvements of speed and memory, are included (the same as nikque.15).

**Fixes**

- The same 2 fixes as nikque.15 were made in the files of the evaluation function of the eval2 line too.
- The check of the mark of the evaluation file now passes only when both of its 8 bytes match.
- When libedax is initialized again with another `eval-file`, it now reads that file.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.3.
- The book code is that of nikque.15.

**Notes**

- This version was released after checking only that it builds and starts. The tests were done after the release.

**Not checked**: the speed and strength of this version, its behaviour with a real book, the full set of book tests for eval2 alone.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval24)

---

## v4.5.5-eval2.3

The changes of v4.5.5-nikque.14 are included ([nikque.14](#v455-nikque14)).

**Changes**

- The new commands `book fill2` and `book leaf expand`, the range and the speed-up of `book fill`, and the new libedax functions are included (the same as nikque.14).
- `book leaf-recalculate` is renamed `book leaf recalculate` (a change of nikque.14). From this version, the `book leaf-recalculate` of the eval2.2 notes is written `book leaf recalculate`.
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.2.
- With the upstream `eval.dat`, the results and node counts are those of nikque.9 and earlier.
- The book code is that of nikque.14.

**Not checked**: the speed and strength of this version, the time of the new commands with the eval2 `eval.dat`, the full set of book tests for eval2 alone.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval23)

---

## v4.5.5-eval2.2

The changes of v4.5.5-nikque.10 to 13 are included ([nikque.13](#v455-nikque13), [nikque.12](#v455-nikque12), [nikque.11](#v455-nikque11), [nikque.10](#v455-nikque10)).

**Changes**

- The book fixes, `book leaf-recalculate`, the faster `book subtree` and `book prune`, the change of `book-expand-tasks = auto`, the new libedax functions and others are included (the same as nikque.10 to 13).
- After `eval.dat` is changed, the scores of the book can be computed again with the `book leaf-recalculate` commands (added in nikque.12). When a book made with the upstream `eval.dat` is computed again with the eval2 `eval.dat`, some Leaves get another score or move. Computed again with the upstream `eval.dat`, the book becomes the original book again. Exactly solved positions are not included.
- With `book deviate2 5 5` on 32 threads (a book of 661.62 million positions), the number expanded in 5 minutes is about 13% more than eval2.1 (11,673 → 13,240). About 80% of the difference comes from the change in how `book-expand-tasks = auto` is decided.
- There is no new change of the eval2 line alone.

**Compatibility**

- The evaluation function and `eval.dat` are those of eval2.1 (the same file).
- The results of the search (scores and node counts) are those of eval2.1.
- With the upstream `eval.dat`, the results and node counts are those of nikque.13.

**Not checked**: the strength of this version and the speed of games and searches (the numbers of eval2.1 are with the sources of nikque.8 and 9), the speed of the book commands measured with the eval2 `eval.dat` (the numbers of nikque.10 to 12 are with the upstream `eval.dat`), the time of `book leaf-recalculate` on a large book.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval22)

---

## v4.5.5-eval2.1

The first version of eval2, a separate line with another evaluation function (`eval.dat`). Only the change of the evaluation function is added to the sources of v4.5.5-nikque.9. It is stronger, and faster from level 18 up, but the scores and the results of the search change.

**Changes**

- The evaluation function now uses the mobility (the number of squares where a move can be played) of both sides. The 46 patterns are not changed.
- All the weights were learned again, starting from the upstream (v4.5.5) weights.
- The learning data are self-play games of Edax itself (level 8, about 840,000 games), the books learned with this fork, and the learning data published on the [Egaroucid](https://www.egaroucid.nyanyan.dev/) site. The patterns and learned weights of Egaroucid are not used. The learning data are not in the release.
- It is released from the branch `eval2`. The default branch and the latest Release stay at v4.5.5-nikque.9. When you need the same scores as before, use nikque.9.
- Strength (games against the upstream `eval.dat`; the same executable, 1 thread, no book, about 2,200 games each; 95% confidence intervals in brackets):

| level | Elo |
|---|---|
| 6 | +22.1 [+8.5, +35.7] |
| 10 | +18.8 [+6.1, +31.5] |
| 18 | +17.0 [+6.6, +27.5] (+20.0 [+9.5, +30.5] from other starting positions) |
| 21 | +17.4 [+7.9, +26.8] |

- The mean error of the level 18 scores against the exact results went from 1.089 discs to 1.067 discs.
- Speed (time ratio against the nikque.9 release with the upstream `eval.dat`; below 1 is faster):

| Executable (1 thread) | level 10 | level 18 | level 21 | Exact solving |
|---|---|---|---|---|
| `wEdax-x86-64-v4.exe` (AVX-512) | **1.055** | 0.945 | 0.877 | **1.026** |
| `wEdax-x86-64-v3.exe` (AVX2) | **1.045** | 0.943 | 0.878 | **1.036** |
| `wEdax-x86-64.exe` (standard) | **1.135** | 0.994 | 0.916 | **1.029** |
| `wEdax-x86-sse.exe` (32-bit SSE2) | **1.204** | **1.037** | 0.949 | 0.97 (±0.05) |
| `wEdax-x86.exe` (32-bit) | **1.239** | **1.070** | 0.973 | **1.034** |

- Where it is faster: level 18 to 24 with the AVX2 and AVX-512 versions (2 to 15% shorter with 1 to 32 threads; level 24 of the AVX-512 version 0.847 times with 1 thread, 0.902 times with 32 threads). This is because fewer positions are searched.
- Where it is slower: shallow searches such as level 10, level 18 with the 32-bit versions, and exact solving (3 to 4% with 1 thread, 279.6 s → 290.1 s on fforum 40-59; about 6% with 32 threads, 22.5 s → 23.8 s). This is because the mobility is counted at each call of the evaluation function.
- When you mostly use exact solving, play or learn many games at shallow levels, or use a 32-bit version, nikque.9 is faster.

**Compatibility**

- `eval.dat` is in a new format, version 3.3.0 (13,960,248 bytes, SHA-256 `1870f8fa5eecb6df972a4d83224832f9a620745cdc67c2d9b17bcd0ba09ea172`). The tables of the mobility are added after the former content.
- Use `eval.dat` and the executable (library) as a pair:

| Executable (library) | `eval.dat` | Behaviour |
|---|---|---|
| This version | This version (3.3.0) | The new evaluation function |
| This version | The upstream one (the one in the releases up to nikque.9) | The same results and node counts as nikque.9. Time 0.98 to 1.013 times |
| nikque.9 and earlier, upstream | This version (3.3.0) | **Do not use.** It loads, but the tables of the mobility are not read and it is weaker |

- The book file format is the same, and earlier books are read as they are. But the scores change, so learning on with this version mixes old and new scores in one book. When you do not want them mixed, keep separate books.
- The settings, commands, libedax API and peak memory are those of nikque.9.

**Notes**

- In programs that use libedax, replace both the library (`libedax-*` of this version) and `data/eval.dat`.
- edax_runner v5.3.0-nikque.3 holds the library of nikque.9 and the upstream `eval.dat`.
- The coefficients of `probcut-model = refit` (an experimental setting of nikque.5) are still fitted to the upstream weights and were not adjusted for the new weights. The default `standard` was measured again with the new weights and is not changed.

**Not checked**: edax_runner with the library of this version, the time of book learning, the speed at levels below 10, games at level 24 and higher, with several threads and against other programs, book tests with the new `eval.dat`, the behaviour of the macOS, Android and Windows ARM64 versions.

[Tests and measurements](docs/validation/eval2.en.md#v455-eval21)
