# Edax 4.5.5 nikque: tests and measurements

Tests and measurements done for each version, moved without changes from the former README-NIKQUE.en.md.

For what changed, see the [release notes](../../RELEASE-NOTES.md). "Above" and "below" in the text refer to positions in the former document.

## v4.5.5-nikque.34

The final Windows x64 AVX2 build accepted a synthetic OBF and returned exit code 1 for a missing input. Interactive `solve` accepted another command after the error. A local GGS fixture covered LF-terminated and CR-terminated lines followed by an incomplete continuation. The stability regression passed 1,024 windows on 240 boards. An independent search verified the stored 240 scores. MSVC built the standalone `eval_builder`.

Production books, live GGS, playing strength and PGO performance differences were outside this verification.

---

## v4.5.5-nikque.33

- This version and the version before (v4.5.5-nikque.33-pre1 with lazy auto and `pc-slope`) were compared with one thread.
  - The default of this version and the version before with `-sort-eval-shift 1,0,0 -sort-min-depth 7` had the same scores, nodes and principal variations, position by position: 12,358,367 nodes for 20 positions of the midgame (level 16), 2,665,719,776 for 25 positions of 26 empties (level 21).
  - `-pc-slope on -pc-dev on -pc-mob on` and the version before with the options of the order of the moves and the three numbers were the same, position by position (11,847,494 and 1,958,837,980 nodes).
  - The three set to `off` were the same as the default.
  - From the default of the version before to the default of this version, the nodes went from 13,125,795 to 12,358,367 (midgame) and from 2,658,607,320 to 2,665,719,776 (26 empties). The score differed in 1 position (midgame), the best move in none.
- `pc-slope`, `pc-dev` and `pc-mob` set to `on` in `config.ini` gave the same search as `on` on the command line (20 positions of the midgame).

- The Windows programs of the distribution solved fforum-20-39 with no wrong move or score. They searched 93,534,529 positions, the same for the three x64 programs (local PGO builds) and the two x86 programs of the CI.
- The libedax test (290 checks) of the three DLLs of the CI passed with no failure.
- The 7 jobs of the GitHub CI (release binaries) passed.

Not checked: the effect of the three settings and the strength in this version, the strength in games, 32 threads, a large real book, an Android device, the Linux and macOS versions, the use from a GUI.

---

## v4.5.5-nikque.33-pre1

- The tests of regressions (51/51), libedax (290 checks), book analysis (11/11), resuming, and book and base (557 checks) passed with no failure.
- The test that stops `book learn` at each write and resumes it passed with no failure.
- With the default settings, the number of positions searched on 20 midgame positions (level 14) was that of the previous version (7,668,847).
- The exact solves of fforum-20-39 had no wrong move or score (93,711,935 positions searched).
- `-pc-r`, `-pc-dev`, `-pc-mob`, `-pc-gate`, `-corr`, `-lmr`, `-lazy`, `-sort-eval-shift` and `-sort-min-depth` were each run alone, with no abnormal end.
- fforum-20-39 was solved exactly with the following two combinations, with no wrong move or score and no abnormal end.
  - `-pc-r`, `-pc-end`, `-pc-dev`, `-pc-mob`, `-corr`, `-lmr`, `-sort-eval-shift` and `-sort-min-depth` (one thread)
  - `-lazy`, `-split-probe`, `-lmr` and `-corr` (four threads)
- `stop`, and `?` of XBoard, sent just before a search began were never lost in 90 runs each. A `ping` just after `go` in NBoard was never lost in 60 runs.
- `book deviate`, `deviate2` and `book enhance` made the same books with one and four threads and `book-reproducible on`.
- The midgame speedup was 0.896 of the CPU time (AMD Ryzen 9 9950X, AVX-512 build, one thread, level 21, median of 8 runs). In 27 runs of 1,050 positions, depths, scores, best moves, positions searched and lines were unchanged.
- Loading a book of 80,068 positions 80 times took 0.936 of the CPU time for a book saved in order and 0.629 for one that is not (median of 10 runs). The books saved again were byte for byte the same.

Not checked: the time settings, `-pc-end` and `-split-probe` alone, playing strength, 32 threads, large production books, Android devices, running the Linux and macOS builds, and use from a GUI.

---

## v4.5.5-nikque.32

- Exact solves with one thread (`-n 1 -l 60 -solve`) were timed alternately with nikque.31 (AMD Ryzen 9 9950X, one logical CPU while other work used every CPU). fforum-40-59: nikque.31 took 432.7, 432.8, 428.7 and 431.4 s (431.4 s on average, 24.52 billion positions searched), nikque.32 took 422.7, 425.6, 416.8 and 421.9 s (421.7 s, 22.11 billion), 0.978 times. fforum-20-39 searched 93,713,436 positions instead of 105,199,471. Both had zero move and score errors.
- nikque.31 built from source searched as many positions as the released `wEdax-x86-64-v4.exe` (105,199,471 on fforum-20-39).
- On 20 midgame positions (14 to 18 random moves from the start, level 14) the number of positions searched was the same as with nikque.31 (7,668,847).
- The [release build CI](https://github.com/Nikque/edax-reversi-AVX/actions/runs/44865879275) built Linux, Intel Mac, Apple Silicon, Android, and Windows. The three Windows x64 executables were replaced by local PGO builds. The five Windows executables (the three x64 PGO builds and the two x86 CI builds) solved fforum-20-39 with zero move and score errors, each searching 93,713,436 positions. All 279 API checks passed for each of the three DLLs. In the CI, both macOS executables solved fforum-1-19 with zero errors, and the Linux and macOS library tests (251 checks) passed.

The speed with many threads, production books, playing strength, and operation on Android devices remain untested.

---

## v4.5.5-nikque.31

- The five executables in the Windows distribution solved 24 synthetic endgame positions scored by an independent implementation with zero move or score errors. All seven Cassio EOF cases passed.
- All 279 API checks passed for each of the three DLLs.
- The x86-64-v4 executable was retested for all seven issues. The checks covered database summaries, an estimate with known error, GTP at game end and a legal pass, `q` in XBoard, GTP, and NBoard, a local GGS mock, and both help entries.
- For trials with totals of 2 and 4, `estimate` returned a mean of 3 and an error of 0.7071068.
- The [distribution CI](https://github.com/Nikque/edax-reversi-AVX/actions/runs/44574161500) built Linux, Intel Mac, Android, and Windows ARM64 binaries. The Linux library tests passed.
- The [fallback build](https://github.com/Nikque/edax-reversi-AVX/actions/runs/44577301101) cross-compiled the Apple Silicon executable and the universal library on Intel Mac from the same fix commit. Both architectures were verified and the Intel library tests passed. Native execution on Apple Silicon was not tested.

Production books, playing strength, communication with a live GGS server, and operation on Android devices remain untested.

---

## v4.5.5-nikque.30

- The previous build returned an incorrect upper bound of +8 for a position whose exact score is +10. The fixed build returned +10.
- The five executables in the Windows distribution gave the expected moves and scores on 20 FForum positions and the expected scores on 1,205 exact-endgame positions. All seven Cassio EOF cases passed for each executable.
- All 279 API checks passed for each of the three DLLs in the Windows distribution.
- The BMI2 final-move kernel matched an independent implementation on 71,168 positions. The evaluation setup, incremental evaluation and paired stability regression cases passed.
- [The distribution CI](https://github.com/Nikque/edax-reversi-AVX/actions/runs/44466219863) built the Linux, macOS, Android and Windows ARM64 binaries.

Playing strength, a production book, and SVE behavior and speed on hardware remain untested.

---

## v4.5.5-nikque.29

Checks:

- `tests/book-resume.py` (new, 40 checks, 1 thread on one logical CPU): `book correct` with 1 and 2 threads (24 solved positions of 14 empties; half of them marked with a wrong score: only the 12 others are searched, the wrong scores stay, the marks go; a third run finds the 12 wrong scores), `book deepen` (11 positions of level 1 get the level 60 of the book; a second run searches none), `book leaf recalculate3` (every position marked: no leaf searched; half marked with a wrong score: they keep it; the marks go), `book learn` (a file of 6 games; a progress file of its first 3 lines: the 3 others only; the progress of another book or another file is not used; a run stopped as soon as its progress file was written, then a new run on the book saved to `.store`: the other lines only), and v4.5.5-nikque.28 reads a book with marks as a damaged book.
- The 14 book suites of the reproducibility work (A, B, F, G, MB, UA, UB, UF, S1, S2, S3, S3off, M1, L18; 1 thread, without `book-reproducible`), with a build of this source and with the release program of v4.5.5-nikque.28: the same files except the save date in 13 suites; S3 (`book learn` with the random seed of the clock) differs from run to run with either program, and with `-book-seed 1` both give the same books twice.
- `tests/regression.c`: the 50 cases pass (a build of this source, one logical CPU).
- The 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same depths, scores, moves and node counts as v4.5.5-nikque.28 (77,639,600 nodes); the 1,205 positions of the exact-search fix: 0 wrong scores; the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible`: the same books as v4.5.5-nikque.28, except the save date. The programs are 4 to 7 KB larger.
- Built by the release workflow (run 44136939697).
- Speed: not measured (the search is not changed).

### Moved from the release notes

- `eval.dat` and the search do not change: the same scores, moves and node counts (fforum-20-39 with 1 thread: 77,639,600 nodes, as v4.5.5-nikque.28). The books made by the other commands are the same as with v4.5.5-nikque.28.

**Checked**: a new test (`tests/book-resume.py`, 40 checks): `book correct` with 1 and 2 threads, `book deepen`, `book leaf recalculate3`, `book learn` (also a run stopped while it learns, then a new run on the book it saved to `.store`), and that v4.5.5-nikque.28 reads a book with marks as a damaged book. The 14 book suites of the earlier releases (1 thread) give the same books as v4.5.5-nikque.28 (except the save date; suite S3 learns with a random seed from the clock: with `-book-seed 1` the same books). `tests/regression.c` (50 cases) passes. The 5 Windows release programs solve fforum-20-39 with the same depths, scores, moves and node counts (77,639,600) as v4.5.5-nikque.28, and the 1,205 positions of the exact-search fix with 0 wrong scores; the 3 DLLs pass the 279 API checks.

## v4.5.5-nikque.28

Checks:

- The 5 Windows release programs (PGO) solve the 1,205 positions whose exact scores were wrong (`-l 60`, 1 thread, hash 2^20; the scores of an independent solver) with 0 wrong scores (267 with the programs of v4.5.5-nikque.27). A test build of the same source gives the same scores as before on 1,640 positions of 17 to 22 empties (almost the same node counts); with the same fix, the C++23 port (newedax) gets all 1,205 right with 1 and 4 threads and hash tables of 2^20 and 2^24.
- Search: the 5 Windows release programs solve fforum-20-39 with 1 thread with the same depths, scores and moves as v4.5.5-nikque.27 (77,639,600 nodes, 77,639,567 with v4.5.5-nikque.27); the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible`: the same books as v4.5.5-nikque.27, except the save date.
- `tests/regression.c` `core` (96 exact searches against an independent solver) passes with 1 and 4 threads.
- Built by the release workflow (run 43743338167): the library test (251 checks, 0 failures) and fforum-1-19 on macOS.
- Speed: not measured.

### Moved from the release notes

- The same error is in Edax 4.5.5 and 4.5.6 of Toshihiko Okuhara (edax-reversi-AVX, also its master of 2026-11-03), from which this version comes (267 of the 1,205 positions below are wrong with the 4.5.5 program and with 4.5.6 built from its source; 0 with the same fix). Edax 4.6 and 4.7 of Richard Delorme do not have it (0 of 1,205).

**Checked**: 1,205 positions whose exact scores were wrong (most with 17 empties; the scores of an independent solver): all right now with the 5 Windows release programs (1 thread, hash 2^20), and with 1 and 4 threads and hash 2^20 and 2^24 with the same fix in the C++23 port (newedax); 1,640 positions of 17 to 22 empties give the same scores as before. The release programs solve fforum-20-39 with the same scores and moves as v4.5.5-nikque.27.

## v4.5.5-nikque.27

Checks:

- Before and after the changes (builds of the same source), on test books with 1 and 4 threads: the book commands (76 cases) write the same files and give the same answers, the analysis commands (20,046) give the same answers, and 25 cases of the other commands give the same output (but the time and the line numbers of `book.c` in its error messages); `book reduce` with 4 threads saves the same books (12 cases, 6 settings). The differences of `tests/regression.c` are the line numbers of `book.c`, the new cases and checks, and `enhance`, which varied from run to run before (a random seed from the clock). The new checks of the bugs (`tests/libedax_test.c`, `tests/book-analyze.c`, `tests/regression.c`) fail with the code before the fixes, but a few that cannot hit the bug on purpose (checked with test copies of the code).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`). The unit tests of the analyses (`tests/book-analyze.c`) pass, the earlier ones with the numbers of the previous version, and the 3 new ones (`book moveinfo` without memory, the limit of the numbers of the items, the memory of a count given away) too.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and lines as v4.5.5-nikque.26; the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-nikque.26. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 251 checks (all but the 28 for Windows only), 0 failures. Android (NDK r27d): no warning. Release CI (run 42432224298): the Linux and macOS libraries pass 251 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the handling of the book files that could not be loaded or opened, the fixes of the commands above, the functions of libedax, and the code rearranged without changing what it does). The node counts above check that the search is the same.
- Size (against v4.5.5-nikque.26): the 5 Windows programs −1,024 to +1,536 bytes, the 3 DLLs +1,536 to +3,584 bytes. Built by the release workflow: the Linux programs −8,192 to +4,128 and libraries +0 to +8,192, the macOS programs +64 and −4,040 and library +480, Windows ARM64 +3,072, the Android programs −712 and +344 and libraries +504 and +16 bytes.
- Speed: not measured.

### Moved from the release notes

- Tests: `tests/libedax_test.c` has 279 checks (61 more, 28 of them on Windows only), `tests/book-analyze.c` 3 more tests, `tests/regression.c` new cases; a case of `tests/regression.c` whose file cannot be opened fails with the exit code 2 instead of crashing.

**Checked**: before and after the changes, on test books with 1 and 4 threads, the book commands (76 cases) write the same files and give the same answers, the analysis commands (20,046) give the same answers, and 25 cases of the other commands give the same output (but the time and the line numbers of `book.c`); the new checks of the bugs fail with the code before the fixes, but a few that cannot hit the bug on purpose (checked with test copies of the code). The 279 checks of the test of libedax and the unit tests of the analyses pass; the gcc build (WSL) gives the same warnings as before, and its libraries pass 251 checks (all but the checks for Windows only).

## v4.5.5-nikque.26

Checks:

- The test of the export: the lines written are compared with sets computed apart in Python (the positions with a leaf to expand, the ends, all the positions of the lines): on 41 positions of 3 test books, the 4,545 cases (3 commands, 5 ranges, 3 kinds) are all the same; every line can be played from the start, its moves follow the walk, and with `book deviate2` and `book deviate3` its total loss is the smallest. The mark `export=1` in the first answer is there.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`). The unit tests of the analyses (`tests/book-analyze.c`) pass with the numbers of the previous version.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and lines as v4.5.5-nikque.25; the 7 Cassio cases pass. The 3 DLLs: 204 API checks (verify18), 0 failures; the test with the export (`tests/libedax_test.c`, 218 checks) has no failure either. Suite F of `book-reproducible` gives the same books as v4.5.5-nikque.25. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 218 checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 41843758711): the Linux and macOS libraries pass 218 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the export and its mark in `book_analyze.c`, the reading of the command and `help book` in `edax.c`, and the function of libedax are new). The node counts above check that the search is the same.
- Size (against v4.5.5-nikque.25): the 5 Windows programs +4,096 to +5,120 bytes, the 3 DLLs +17,920 to +19,456 bytes. Built by the release workflow: the Linux programs +8,192 to +12,288 and libraries +8,192 to +12,288, the macOS programs +0 and +8,200 and library +33,024, Windows ARM64 +6,656, the Android programs +7,496 and +3,656 and libraries +8,520 and +4,384 bytes (the functions of the export).
- Speed: not measured.

### Moved from the release notes

**Checked**: the lines written are compared with sets computed apart in Python (the positions with a leaf to expand, the ends, all the positions of the lines): on 41 positions of 3 test books, the 4,545 cases (3 commands, 5 ranges, 3 kinds) are all the same; every line can be played from the start, its moves follow the walk, and its total loss is the smallest. The 218 checks of the test of libedax pass.

## v4.5.5-nikque.25

Checks:

- The test of the order of the terms (`umigame-order` of `tests/book-analyze.c`): with a hook of the test build that puts the terms in random orders before they are sorted, 200 graphs of about 1,300 positions and the 1,744 graphs after their nodes up to 2 moves are counted again in 3 orders, and the numbers and the `exact` marks are all the same (with the source of v4.5.5-nikque.24 some numbers changed, e.g. 14 → 13, 8 → 10).
- gcc (WSL, 11.4) and MSVC give the same number, 39, of values "below the bound found when the count from the node drops terms" in `umigame-large` (40 with gcc and 37 with MSVC in v4.5.5-nikque.24). The earlier unit tests of the analyses (`umigame`, `umigame-large`, `precision`, `bookhint`, `reach`, `missing`, `umi-inherit` and the tests on the test books) pass too, with the numbers of v4.5.5-nikque.24 but this one.
- Programs (a test AVX2 build, 1 thread): on each position of 10 games of 2 test books (610 positions), the commands of the GUI (the stages and umigame numbers of `book moveinfo`, the numbers of positions, `book levels`, `book missing`) in 5 conditions give 46,092 `@` lines; one differs from those of a program built from the source of v4.5.5-nikque.24, a number marked `exact=0` (`umi=3:15 exact=0` → `umi=2:15 exact=0`, both upper bounds).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and lines as v4.5.5-nikque.24; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-nikque.24. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40650319801): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the order in which `book_analyze.c` drops the terms of an umigame number is changed). The node counts above check that the search is the same.
- Size (against v4.5.5-nikque.24): the 5 Windows programs +0 to +1,536 bytes, the 3 DLLs the same size. Built by the release workflow: the Linux and macOS executables and libraries the same size, Android executables +32 and +32 bytes and libraries +32 and +16 bytes, Windows ARM64 +512 bytes.
- Speed: not measured.

### Moved from the release notes

- A number marked `exact=0` may differ from nikque.24 (both are upper bounds; of the 46,092 `@` lines of 2 test books in 5 conditions, one changed: `umi=3:15 exact=0` → `umi=2:15 exact=0`).

**Checked**: the numbers and the `exact` marks are the same when the terms are put in random orders before they are sorted (test `umigame-order`: 200 graphs of about 1,300 positions and the 1,744 graphs after their nodes up to 2 moves, in 3 orders; before the fix some numbers changed). gcc (WSL) and MSVC give the same numbers in the test of dropped terms (39 in `umigame-large`; 40 and 37 before). The earlier tests all pass.

## v4.5.5-nikque.24

Checks:

- The tests of what is taken over (`tests/book-analyze.c`): for the umigame numbers, the values of every node are taken out when the root of 20,000 random graphs is counted for both colours, and the 157,258 values are those of the graphs from those nodes. On 200 graphs of about 1,300 positions (131 numbers with dropped terms among them), the values of the roots and the `exact` marks are the same with and without the values taken out, and the 1,214 values taken out up to depth 2 are those counted again from their nodes (37 of them below the bound found when the count from the node drops terms). On 100 random books × 6 conditions × 3 orders of the positions (55,188 moves each) and 3 test books (12,576 moves), the umigame numbers taken over are those counted again from the position after each move alone.
- Numbers of positions and `book levels` (`reach`): the 56,244 answers on 200 random books and the 6,174 answers on 3 test books (the number of positions and the number per level) are those counted from each position alone. `book levels` at a position already counted (13,404 times on the 200 books, 1,462 times on the test books) never walked the book.
- `book missing` (`missing`): of the 2,673,783 walks asked on 100 random books, the 1,013,269 known from the table to find nothing find 0 lines when walked; the same for 5,658 walks on a test book.
- The tests of the precision stages (`precision`, 502,470 moves; 14,805 moves on a test book) and of the hint of the warning (`bookhint`, 24 cases) give the numbers of v4.5.5-nikque.23, with no error.
- Programs (a test AVX2 build, 1 thread): on each position of 10 games of 2 test books (610 positions), the commands of the GUI (the stages and umigame numbers of `book moveinfo`, the numbers of positions, `book levels`, `book missing`) in 5 conditions give 46,092 `@` lines, all the same as those of a program built from the source of v4.5.5-nikque.23.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and lines as v4.5.5-nikque.23; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-nikque.23. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40629262755): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the code of `book_analyze.c` that finds the umigame numbers and the numbers of positions of `book moveinfo`, `book levels` and `book missing` is changed). The node counts above check that the search is the same.
- Size (against v4.5.5-nikque.23): the 5 Windows programs +0 to +2,048 bytes, the 3 DLLs the same size. Built by the release workflow: Linux executables +0 to +4,096 bytes and libraries +4,096 bytes, macOS executables +0 and +4,096 bytes and library +0 bytes, Android executables +1,488 and +832 bytes and libraries +1,504 and +848 bytes, Windows ARM64 +512 bytes.
- Speed: not measured (the time of the umigame numbers, the numbers of positions, `book levels` and `book missing` was measured neither on a real book nor in the GUI; what went down was counted by the nodes of the graphs built and the walks).

### Moved from the release notes

- On the games of 3 test books, with their positions looked at from the first move on, the nodes of the graphs built went down by about 38% to 46% with `umi=best`, and by about 4% to 53% with `umi=loss x=2 y=4`.

**Checked**: the answers taken over were compared with the answers found from each position alone, for every move (umigame numbers: 40,000 random graphs, 100 random books and 3 test books, 6 conditions, 3 orders of the positions; numbers of positions: 200 random books and 3 test books; `book missing`: 100 random books and 1 test book; no difference). The `@` lines of the same commands as nikque.23 (2 test books, 5 conditions) are the same.

## v4.5.5-nikque.23

Checks:

- The test of the stages taken over (`precision` of `tests/book-analyze.c`): on 200 random books (transpositions, passes, solved positions, positions beyond the book depth, values not negamaxed again and links that differ mixed in), for 502,470 moves (3 bases × 5 ranges × 3 orders of the positions), the stages and missing leaves taken over are those walked from each position alone. The 6,268 moves of loss 0 from a complete position were not walked at all.
- On 2 test books (1,547 and 409,494 positions), every move of every position gives the same answer (about 4.78 million moves on the book of 409,494 positions). The answers of `book moveinfo` (3 bases, each position of 2 games) are the same as those of a program built from the source of v4.5.5-nikque.22.
- The test of the hint of the warning (`bookhint`): 24 cases. With a file of commands given to the program: `leaf expand 30 30`, `fill2 2` and the like get the hint, `new x`, `check`, `correct` and `echo` do not.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and lines as v4.5.5-nikque.22; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-nikque.22. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40423800206): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The warning of `book_analyze.c` in the gcc 11.4 `x86-64-v4` build (a write that might go beyond an array, a false alarm: the number of ranges is at most 8 when the command is read) is gone with one line that checks the number of the range (it works the same).
- The search, the evaluation, the book learning and the file format are not changed (the code changed finds the precision stages of `book moveinfo`, and writes the warning of an unknown command). The node counts above check that the search is the same.
- Size (against v4.5.5-nikque.22): the 5 Windows programs +2,560 to +3,072 bytes, the 3 DLLs +512 bytes. Built by the release workflow: Linux executables +12,288 to +16,384 bytes and libraries +8,192 bytes, macOS executables +4,096 and +16,528 bytes and library +0 bytes, Android executables +2,536 and +4,368 bytes and libraries +2,368 and +4,352 bytes, Windows ARM64 +4,608 bytes.
- Speed: not measured (the time of `book moveinfo` was measured neither on a real book nor in the GUI).

### Moved from the release notes

- On a test book of about 410,000 positions, with every position looked at in the order of the book, the walks of the ranges went down by about 14% with `deviate` and about 29% with `deviate2` and `deviate3`, and the walks of the best lines by about 99% (how much depends on the order in which the positions are looked at).

**Checked**: the stages taken over were compared with the stages walked from each position alone, for every move (200 random books and 2 test books, 3 bases, 3 orders of the positions; no difference). The answers of `book moveinfo` to the same commands are the same as those of nikque.22.

## v4.5.5-nikque.22

Checks:

- The tests of `book reduce` and `book reducecount` (4 small books made for the tests, compared with a computation written apart; the tests of nikque.21): all the same in 546 cases, with 4 threads and 1 thread, with and without `book reducecount` before `book reduce`, with both values of `book-reduce-index` and of `book-reduce-walk`, and with a test build that takes the parallel way for every group. With the count first, that its result was used was checked too.
- The books saved after the same `book reduce` commands as for v4.5.5-nikque.20, with the executables of v4.5.5-nikque.20 and of this version, with and without the count first: the same byte for byte in 54 cases (but for the save date). With a test build with fewer buckets at least, so that `book-reduce-shrink` works on small books: the same positions, sorted, in 54 cases.
- The tests of damaged books and wrong words (14 cases) all pass.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the tests of damaged books and wrong words (14 cases) all pass; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release executables (PGO) solve fforum-20-39 with one thread with the same results, node counts (77,639,567) and principal variations as v4.5.5-nikque.21. The 7 Cassio cases pass. The 3 DLLs pass 204 API checks, 0 failures. The books of suite F of `book-reproducible` are the same as with v4.5.5-nikque.21. The 4 Linux builds (WSL, gcc 11.4, built as the release workflow does) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Android (NDK r27d): no warning. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (the code changed runs only when `book reduce` or `book reducecount` is typed, or reads their settings); the node counts above show that the search is the same.
- Size (against v4.5.5-nikque.21): the 5 Windows executables +4,096 to +4,608 bytes, the 3 DLLs +1,024 to +1,536 bytes. Built by the release workflow: Linux executables +4,096 to +12,288 bytes and libraries +8,224 to +12,320 bytes, macOS executables +0 and +8,200 bytes and library +32,960 bytes, Android executables +3,560 and +6,104 bytes and libraries +3,640 and +6,264 bytes, Windows ARM64 +5,632 bytes.
- Release workflow (run 40014088094): the API checks of the Linux and macOS libraries, 204 checks, 0 failures; fforum-1-19 on macOS with the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 executables and DLLs are the local PGO builds.
- Speed (the real book of 29 GB, read only; `book reducecount` from the initial position with 2 4, a test build without PGO, 8 logical CPUs): the numbers above. Measured twice; the first time, 12 to 20% slower, probably with other work at the same time, gave the same order and ratios. The memory added while counting: about 8.6 to 8.9 GB with 4 threads, about 10.4 GB with 8 (about the same for every setting). The speed and memory of `book reduce` itself were not measured (the real book is not changed).

## v4.5.5-nikque.21

Checks:

- The tests of `book reduce` and `book reducecount` (4 small books made for the tests, compared with a computation written apart; the tests of v4.5.5-nikque.20): all the same in 546 cases, with 4 threads and with 1 thread (and with 4 threads in a test build that takes the parallel way for every group). The 546 cases are the counts, the books saved after the cuts, the tests of the words, the stops and others in all. The leaves are compared as the positions that `book deviate3` selects first from the books before and after the cut, computed apart: `book deviate3` itself was not run to its end for the comparison.
- The books saved after the same `book reduce` commands as for v4.5.5-nikque.20, with the executables of v4.5.5-nikque.20 and of this version: the same byte for byte in 54 cases (but for the save date).
- Tests of two damaged books (of 2 positions and of 1 position) and of `book reduce` with wrong words were added (14 cases: 8 fail with the executable before the fixes, all pass with this version, with 1 thread and with 4 threads).
- The speed and the memory were not measured (other programs used the CPU during the tests): it cannot be said that the commands as a whole are faster or take less memory than before. What follows from the way the work is done is only that the moves to fill the gaps when positions are removed, and the list of the walk of `book reduce`, are gone.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the tests of damaged books and wrong words (14 cases) all pass; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release executables (PGO) solve fforum-20-39 with one thread with the same results, node counts (77,639,567) and principal variations as v4.5.5-nikque.20. The 7 Cassio cases pass. The 3 DLLs pass 204 API checks, 0 failures. The books of suite F of `book-reproducible` are the same as with v4.5.5-nikque.20. The 4 Linux builds (WSL, gcc 11.4, built as the release workflow does) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Android (NDK r27d): no warning. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (the code changed runs only when `book reduce` or `book reducecount` is typed); the node counts above show that the search is the same.
- Size (against v4.5.5-nikque.20): the 5 Windows executables +4,096 to +5,632 bytes, the 3 DLLs the same size. Built by the release workflow: Linux executables +4,096 to +8,192 bytes and libraries +8,192 bytes, macOS executables +12,296 and +16,512 bytes and library +16 bytes, Android executables +4,112 and +7,104 bytes and libraries +4,144 and +7,120 bytes, Windows ARM64 +6,144 bytes.
- Release workflow (run 39949381811): the API checks of the Linux and macOS libraries, 204 checks, 0 failures; fforum-1-19 on macOS with the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 executables and DLLs are the local PGO builds.
- Not checked with the 29 GB real book.

## v4.5.5-nikque.20

Checks:

- On 4 small books made for the tests (about 1,500 to 410,000 positions, with many transpositions and passes), compared with a computation written apart (Python; it reads the book file itself and walks as `book deviate3` does, after computing the values again): the numbers kept and removed by `book reducecount`; the positions of the book saved after `book reduce`, the value, level, links and leaf of each of them; the positions whose leaf `book deviate3 X Y` expands afterwards. All the same in 546 cases. Missing numbers, numbers out of range, a position out of the book, a position beyond the depth and `stop id=N` were checked too.
- With v4.5.5-nikque.19, from before the change, the same tests all fail (unknown commands).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests above (500 cases on 2 small books) all agree; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release executables (PGO) solve fforum-20-39 with one thread with the same results, node counts (77,639,567) and principal variations as v4.5.5-nikque.19. The 7 Cassio cases pass. The 3 DLLs pass 204 API checks, 0 failures. The books of suite F of `book-reproducible` are the same as with v4.5.5-nikque.19. The 4 Linux builds (WSL, gcc 11.4, built as the release workflow does) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Android (NDK r27d): no warning. No program or library holds a folder of the build machine.
- Speed: the search, the evaluation, the book learning and the file format are not changed (the code added runs only when a new command is typed); the node counts above show that the search is the same. The speed of the new commands was not measured.
- Size (against v4.5.5-nikque.19): the 5 Windows executables +3,584 to +5,632 bytes, the 3 DLLs the same size. Built by the release workflow: Linux executables +8,192 to +12,288 bytes and libraries +4,096 to +8,192 bytes, macOS executables +16 and +12,288 bytes and library +32,944 bytes, Android executables +3,120 and +7,152 bytes and libraries +3,216 and +7,200 bytes, Windows ARM64 +6,144 bytes.
- Release workflow (run 39887011040): the API checks of the Linux and macOS libraries, 204 checks, 0 failures; fforum-1-19 on macOS with the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 executables and DLLs are the local PGO builds.
- Not checked with the 29 GB real book.

## v4.5.5-nikque.19

Checks:

- On 3 small books made for the tests (about 1,400 to 410,000 positions, with many transpositions and passes), the answers were compared with computations written apart (in Python, reading the book file itself): the precision stages, the missing leaves, the values, the numbers of positions and the umigame numbers (against the smallest over every choice of moves) agree for 4,924 moves × 3 conditions; the lines to the missing leaves for 1,528 cases; the best lines for 608 cases (the 5,546 lines written were played again from the start).
- The precision stages were also checked by running the real `book deviate`, `deviate2` and `deviate3` 164 times (nothing is added within the range of stage k; something is added within the range of stage k+1).
- Lines to a position (`book findline`): 6,576 cases on 3 test books agree with a computation written apart (the n smallest costs and the number of lines); each line answered was played again (legal, every position in the book, the target reached, the same cost).
- Umigame numbers: 40,000 random graphs of positions (31,338 with shared positions) agree with the smallest over every choice of moves.
- The background thread: stopping (running, waiting, unknown number), the stop before a command that changes the book, no line lost while `hint` runs at the same time, `quit` during an analysis.
- Search: 17 positions with 1 thread, level 14 and no book give the same depths, scores, node counts (7,805,061 in all) and principal variations as `wEdax-x86-64-v3.exe` of v4.5.5-nikque.18. The 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (77,639,567) and principal variations as v4.5.5-nikque.18; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. The books of suite F of `book-reproducible` are those of v4.5.5-nikque.18. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. No program or library holds a folder of the build machine.
- Speed: the search, the evaluation, the book learning and the file format are unchanged. What the usual paths run in addition is a lock when a line is written, a check at the start of a book command (once per command) and the reading of the `stop` lines. The node counts above show that the search is the same.
- Speed of the new commands (a test book of 19.1 million positions, 4 threads): the precision stages and umigame numbers (best moves) of every move take 0.02 s or less even from the start; the number of positions from the start 2.3 s; `book bestlines loss=4` 15.5 s (9.81 million positions). The analyses take at most about 86 MB more memory (about 7 % of the book).
- Size (against v4.5.5-nikque.18): the 5 Windows programs +40,448 to +41,472 bytes (the analysis code); the 3 DLLs the same size (the unused code is dropped by the linker). Built by the release workflow: Linux programs +73,728 to +90,112 bytes and libraries +73,760 to +86,048 bytes, macOS programs +66,176 and +73,856 bytes and library +99,200 bytes, Android programs +31,544 and +52,152 bytes and libraries +31,968 and +53,000 bytes, Windows ARM64 +48,640 bytes.
- Release workflow (run 39501599439): the API checks of the Linux and macOS libraries, 204, 0 failures; fforum-1-19 on macOS with the same node counts as before (x64 2,175,751, arm64 2,181,259). The Windows x86 and x64 programs and DLLs are the local PGO builds.
- Not checked with the 29 GB real book.

## v4.5.5-nikque.18

Checks (one logical CPU):

- fforum-20-39 with 1 thread: the 5 Windows programs give the same depths, moves, scores, node counts (77,639,567) and principal variations as those of v4.5.5-nikque.17. Cassio: 7 cases. The 3 DLLs: 204 API checks, 0 failures. `book-reproducible`: the books of suite F are those of v4.5.5-nikque.17.
- Linux (WSL, gcc 11.4, built as the release binaries workflow does): the 4 programs give the same rows as the Windows programs of v4.5.5-nikque.17 (77,639,567 nodes); the 3 libraries: 204 API checks, 0 failures. No build folder in them, nor in the Android programs (NDK r27d).
- Speed (CPU cycles, 12 pairs in alternating order, against v4.5.5-nikque.17): 0.976 to 1.000.
- Speed with 32 threads (2026-10-19, the released `wEdax-x86-64-v4.exe`, ratio of the times to v4.5.5-nikque.17, mean of 6 alternating rounds): suite F (the `book deviate` family, level 21) 0.997, `book learn` (30 games, level 18) 1.003, `book fill 2` 1.005, `book fill2 2` 0.987, `book leaf expand` 0.998, `book fill2 2 5 5` 0.993, `book import` (a text book of 4 million positions) 0.988: each within the standard deviation (0.7 to 3.4 %) or faster. The same peak memory (±1 MiB; also in three one-thread test suites of book commands).
- Size: -3,072 to +512 bytes. The strings of the error messages are about 800 bytes shorter; the code of a PGO build changes by a few hundred bytes from a build to the next (the same source built twice: -512 to 0 bytes).

## v4.5.5-nikque.17

### Speed and memory (`book-reproducible = on`, measured)

32 threads on a quiet computer, against v4.5.5-nikque.16, four runs each in alternating order.

| Case | Time (nikque.16 = 1) | Peak memory |
|---|---|---|
| `book leaf expand` (level 12) | 0.84 | 1,558 → 2,423 MiB |
| `book fill2 2` (level 12) | 0.94 | 694 → 745 MiB |
| `book deviate` family (level 21) | 1.20 | 1,124 → 2,043 MiB |
| Learning 30 games at level 18 (8 threads) | 1.19 | 278 → 331 MiB |

Each one-thread search has the hash table of a one-thread run (21 bits, 57 MB, with `hash-table-size = auto`), so more memory is used. With the default (`off`), the time and peak memory of the four cases are the same as with v4.5.5-nikque.16 (the `book deviate` family, measured again 16 times each: ratio 1.0000).

### Checks

- The 14 book suites of the previous comparison (`book deviate` family, `enhance`, `fill`, `fix`, `correct`, `merge`, `add`, `store`, `learn`, level 18 learning, ...) were run twice each with `book-reproducible = on` and 32 threads, on one logical CPU and on 32 cores: every file is the same except the save date (S1, with `go`, included). Ten suites (the expansion commands among them) also give the books of v4.5.5-nikque.16 with `n-tasks 1`.
- With the default settings and `n-tasks 1`, 13 of the suites give the books of v4.5.5-nikque.16 (S3 uses random choices); S3 with `book-seed 1` gives the books that v4.5.5-nikque.15 and 16 gave with a fixed seed during the investigation.
- `tests/regression.c` has two new cases: `reproducible` (`book-seed`, the check of the links, the same book with 4 threads twice and 1 thread) and `expandredo` (an expansion whose links changed is searched again, and the book is the one of the one-by-one expansion). They pass on Windows (MSVC) and Linux (gcc 11.4). ThreadSanitizer reports nothing from the changed code (the older report about `need_saving` in the parallel negamax remains).
- See the [Japanese validation report](validation-nikque17.md). The real 29 GB book was not used. Android and Windows ARM64 were not run on devices.

## v4.5.5-nikque.16

### Windows PGO comparison with v15 (2026-10-16)

Ryzen 9 9950X, 32 threads, 192 GiB, Windows 11 Pro build 26300, VS2022 17.14.42. Each case has warm-ups, then ABBAABBA (four per version). A is v15; B source is `19e99cb1c16ade6f4e1a4c6b9c10f82cd219c09e`. Ratios are means of four paired B/A times; ± is sample SD. Times cover the whole process, including startup and I/O.

| 条件 / Case | A平均 / mean s | B平均 / mean s | B/A ± SD |
|---|---|---|---|
| Endgame, 1 thread | 1.005078 | 1.011058 | 1.0060 ± 0.0039 |
| Endgame, 32 threads | 0.427658 | 0.429263 | 1.0042 ± 0.0258 |
| Midgame, level 18 | 0.973384 | 0.976649 | 1.0034 ± 0.0166 |
| Midgame, level 21 | 4.046103 | 4.089137 | 1.0106 ± 0.0326 |
| Learn games, n=1 | 119.052166 | 119.752446 | 1.0059 ± 0.0064 |
| Learn games, n=8 | 20.200064 | 20.333607 | 1.0067 ± 0.0137 |
| book fill 2 | 6.831650 | 6.748804 | 0.9881 ± 0.0165 |
| book fill2 2 | 3.109296 | 3.137015 | 1.0090 ± 0.0117 |
| book leaf expand | 1.683624 | 1.719111 | 1.0210 ± 0.0158 |
| base unique | 1.439397 | 0.146957 | 0.1021 ± 0.0031 |

Leaf expand was initially 2.10% slower; the additional ABBA gave 1.0094±0.0009 (0.94% slower). No other initial case exceeded 2%. Other CPU load recorded during runs was 0.34–7.42%; these are not guaranteed fully idle measurements. Peak working sets and all individual times are in the report.

The 51,106-game base has no final newline: A outputs 51,105 games, B 51,106; B's extra game is the last input line. The approximately 9.8-fold improvement is for the CLI process, not a new measurement of the previous 0.012-second function-only figure. The 29 GB book was not used. See the report for byte mismatches, multithreaded variation and procedure incidents.

### Speed and memory (same results)

- `base unique` sorts the games before comparing them (47,000 games: 4.9 s → 0.012 s).
- `base save` writes the games without copying them.
- The recount of saturated positions remembers its partial results (test book: 891 → 525 ms, same values).
- The size of an allocation of the evaluation data (eval.c) is fixed.

### Checks

- With the version before the fixes (v4.5.5-nikque.15), most of the defects were reproduced by small tests (the lost last game, the lock choice, the stats out of range, unusable boards, the reading of FEN, times and coordinates, truncated Wthor files, the Cassio lines, `flip_neon_rbit.c`, ...), with AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer.
- `tests/regression.c` has new cases (hashlock, lastgame, basereplace, keepunread, statsrange, gamescore, badinput, wthortrunc, cassiolist, parsers, tools, unique). The 44 cases pass on Linux (gcc, `NDEBUG`).
- Search: fforum-20-39 solved with 1 thread gives the same depths, scores, node counts (77,639,567) and lines as v4.5.5-nikque.15; with 4 threads, the same scores.
- aarch64 (qemu): the default build, the build with `HAS_CPU_64`, and the build with NEON and LASTFLIP_LOWCUT give the same depths and scores as x64 on fforum-20-39 (the ARM builds have the same node counts, 77,662,358; the difference with x64 was already there). After the fix, `flip_neon_rbit.c` flips the same discs as the generic code on 200,000 random positions.

## v4.5.5-nikque.15

### Speed and memory (the books are the same)

- On **Linux and macOS** too, the unused part of the block that holds the positions of a loaded book (the pool) is released after `book prune` and `book subtree`, and after `book load` (up to now on Windows only).
- When learning adds a position to a loaded book, its bucket is copied out of the pool, and its place in the pool stayed unused. After a book command or a libedax call, the pool is compacted and its end released when the unused places are more than a quarter of it.
- `book fill`, `fill2`, `leaf expand`: the list of positions is sorted after it is narrowed (it was sorted whole, then narrowed). The array of marks (1 byte per item) and the copy of the list (8 bytes per item) are gone; `leaf expand` no longer sorts the list twice; the list gives back its memory when most positions were left out.

### Checks

- Three of the defects were reproduced on small books with the release of v4.5.5-nikque.14 (prune never ends after the import, a damaged book crashes, an evaluation file passes with half of its header).
- Before publication: builds for Windows, Linux, Android and macOS, release startup, search and API checks. The post-release checks follow.
- **Post-release test set (2026-10-15, `5eaf9f3`)**: the book regression differs from nikque.13 in the same files as nikque.14 (fill and later steps). A2 without fill matches every nikque.13 file with 1 and 8 threads. The first three AddressSanitizer sweeps covered 79 cases each with no detection (3–4 existing time limits). Lost inputs were recreated and the 21 omitted cases were run with normal allocation, 0xCD-filled allocation and assertions: **all 63 runs completed normally, with no detection or timeout**. Some game inputs reproduce the conditions rather than the original bytes. Thread creation failure, stop/resume and repeated searches behaved as before. The build failure in `tests/regression.c` was fixed in `55c35cb`; release binaries are unchanged.
- **Focused checks**: failing the second position allocation during import preserves the current book's export byte for byte. Reducing 8 links step by step to 0 performs no allocation and preserves the remaining links. Reinitializing the same libedax with the original weights, a zero-weight copy, then the original weights changes the f5f4 depth-1 score −3→0→−3. A binary book with only its boards reflected was normalized: all 3,191 positions were found with the corresponding 3,671 links and 1,514 leaves (the normalization phase was observed with a test hook). Load→fix→prune also completed with the AddressSanitizer build of the released source. Hooks were added only to an isolated source copy, not to release files.
- **libedax stop checks**: calling `edax_stop` every 5 ms during `edax_base_complete` or `edax_base_correct` produces byte-identical game files to the runs without stop calls (4 games each, level 14, correction from 22 empties).
- **Linux memory release**: a saved and loaded book containing 500,000 test-text positions plus the initial position and its child shrank its pool from 500,002 to 2 positions during prune; RSS fell 58,880→35,456 KiB. After learning 2,048 games, reclaiming 181,473 unused positions shrank the pool 500,002→318,529 and RSS 131,136→122,628 KiB. RSS was sampled immediately before and after shrinking. No macOS RSS measurement or real-book run was made.
- **ThreadSanitizer comparison**: B6 progress-counter reports disappeared. Of the 19 book/play-related signatures compared individually with the old 27, 15 match exactly, 3 differ in access order or callers, and 1 reads the same search-stop field from PVS. The 2,432-byte block was already reported by the old version. Comparing all 262 signatures also found access combinations absent from the old log, in existing search-stop and hash-table code. This does not establish that all races are fixed.
- **Speed (released builds, 32 threads, order A B B A)**: on a book of 83,000 positions (level 12), `book fill 2`, `fill2 2`, `leaf expand` and `fill2 2 5 5` take the same time as v4.5.5-nikque.14 (within 2%). `book import` of a text book of 4 million positions: 9.7 s → 7.9 s.

## v4.5.5-nikque.14

### Checks

**The complete test set** (the set of v4.5.5-nikque.13, on the source before the last fixes, `9666634`, against builds of the source of v4.5.5-nikque.13 with the same options)

- **One-thread search**: the 69 positions give the same depth, score, node count and principal variation as before (Windows test builds: 64 bits and two 32-bit builds; Linux, 4 builds). Distribution executables: the same node counts for the 5 Windows and 4 Linux executables. Repeated searches with 8 and 32 threads (Windows 6,000 + 2,000 positions, Linux): 0 wrong results.
- **Book regression** (every book command, 1 and 8 threads, with a book of 6.49 million positions): the only differences with v4.5.5-nikque.13 are the step of `book fill` and the steps after it (in suite A the negamax, fix, correct, export, import, subtree, deviate and store after `book fill 2` start from the book that `fill` made; in suite G, `fill` and the `fix` after it). **Suite A without `book fill` (A2) gives the same files as v4.5.5-nikque.13, with 1 and 8 threads.** Between the distribution executables, the same steps only differ.
- **libedax**: 204 API checks (the 198 of before and the new functions), 0 failures (3 Windows test builds, the 3 distribution DLLs, 3 Linux libraries). 104 functions are exported (5 more). The edge cases: 0 failures.
- Thread creation failing (Windows, Linux): the same books as the usual build, 0 errors. The stop-and-go test and the stress test of learning at the same time: 0 failures. File and setting tests: the only differences are the new name on the screen (`book leaf recalculate rounds`) and what changed from run to run before (node counts of searches with several threads, the merge of a damaged book).
- ThreadSanitizer (Linux, gcc 11): the 8 earlier cases show the same 27 kinds of reports in the book and game code as before. The 24 cases of the new commands (2 books × 6 commands × 2 builds; one is a test build whose walks and filters use threads from 2 positions on) all ended normally, and the reports in book.c are only of the harmless kinds known before (the "needs saving" flag of the negamax, the progress display).
- gcc warnings: none only in this version (the warning of `hash.c` in the library is in v4.5.5-nikque.13 too).
- Builds that detect memory errors (MSVC AddressSanitizer): the 79 cases whose inputs were there (the 12 cases of the new commands among them) found no memory error (the time limit stopped the two `deviate3` cases that it always stops). 21 cases were not run: some of their inputs had been lost. The runs with the allocated memory filled and with the assertions on were stopped before the fixes below.

**Fixes after the test set**: the test set above ran on the source `9666634`. Then four points found by a cross-check (another reading of the source) were fixed: a position whose leaf became a link could keep the score of a leaf without a move (when it was the last move; the negamax counted it as a line); a position that only got links did not mark the book as changed for the save at the end; a list that could not be allocated did not stop the command as a failure; range numbers starting with a sign (`+5`, `-1`) ran the command without a range. **On the fixed source, only the builds and runs of the commands on a small book were checked; the test set was not run again.**

**The new commands**

- On small books (level 4: 56,000 positions, and two books of 3,000 positions with passes and game ends), the 6 new commands made the same book with 1 and 8 threads (18 pairs). Counted afterwards with a tool written apart from Edax: after `book fill` and `fill2` without a range, 0 missing links (609 positions before), 0 gaps after the leaves, and after `book fill` 0 gaps after any move; after `book leaf expand`, 0 leaves better than every link; after a command with a range, running it again in the same range finds nothing to link or fill.
- On the book of the regression (suite A, 79,000 positions), `book fill 2` made a book with the same positions, the same number of links and the same scores as the `book fill 2` of v4.5.5-nikque.13 (only the order of the links, the leaf of 2 positions and the statistics of 11 positions differ).
- `book fill 1` made the same book as v4.5.5-nikque.13 (two books). A test build (`BOOK_TEST_FILL_OLD`) runs `book fill n` with the code of v4.5.5-nikque.13.
- Speed (level 12, 83,000 positions, 32 threads, almost nothing else running, two runs each; **measured on the code before the linking step was widened to every move, not on the final code**): `book fill 2` 6.74 and 6.64 s against 94.8 and 93.2 s with v4.5.5-nikque.13. `book fill2 2` 3.03 and 3.09 s at the same time (`auto`) against 39.6 and 40.1 s one after the other (`book-expand-tasks 1`); `book leaf expand` 1.67 and 1.64 s against 14.8 and 14.6 s.
- Counts on the real book (672 million positions, read only, never saved): see "Not changed, and good to know" above.

**Distribution**

- edax_runner 5.3.0+nikque.8: with the libraries of this version, the bundled learning list, games, lines with errors and an empty list were learned; the book, the list and the log are the same as with the libraries of v4.5.5-nikque.13 (the bundled list has a line played with randomness: its book changes from run to run, also with the same library).
- Distribution executables: built again from the final source (Windows with profile-guided optimization, Linux and Android in WSL, macOS by the GitHub workflow).

## v4.5.5-nikque.13

### Checks

This version is published, for the convenience of its user, **when the Windows part of the complete test set had ended.** What follows tells what was finished at that time and what was not; what was not finished ended on 2026-10-08, and its results are added below.

**Finished on the final source**

- **One-thread search**: for 69 positions (39 solved exactly, 30 at level 18), depth, score, node count and principal variation are those of v4.5.5-nikque.12 and before (test builds for Windows 64-bit and the two 32-bit targets). The distribution executables give the node counts of the earlier versions too: the 5 Windows ones and the 4 Linux ones (105,079,412 and 68,850,986; on Windows also 2,175,751).
- **Book regression** (every book command, against v4.5.5-nikque.12; 1 thread, 8 threads, a book of 6.49 million positions): 165 files the same and 21 different with the test builds, 167 the same and 22 different between the distribution executables. **The files that differ are the three kinds that this version changes**: the file of `book export` (one more line at the end, `% depth n`), the book saved after an import and the books cut from it (only the depth of the header differs; not one byte of the positions), and the game file `a17_game.edx` (the fix of the memory that was never set).
- **libedax**: 198 API checks (the 197 of before and one for `edax_base_complete`), 0 failures (3 Windows test builds, the 3 Windows distribution libraries, the 3 Linux distribution libraries); 81 edge cases, 0 failures.
- With a limited number of threads (9, 12, 20): learning, import and the repair of damaged books give the books of the usual build, 0 errors. Repeated exact solves (6,000 positions with 32 threads, 2,000 with 8): 0 wrong results. 240 repeated runs of learning several games: 0 failures.
- 23 settings tests and 94 file-name tests: the outputs only differ by what this version changes on purpose (`book info`, `book save` without a name, ...), the line of the new setting, the function names and line numbers of the error messages, and counts that depend on the time.
- The 33 checks of the fixes of this version and 3 with the library (each one with its control: the build without the fix must show the fault).
- edax_runner (5.3.0+nikque.7): its 27 tests, and 9 learning lists (same books as with the libraries of v4.5.5-nikque.12; the list with random lines differs from a run to the next).
- The Linux distribution build (gcc) and the build of the two Android libraries.

**Speed and memory** (idle PC, Ryzen 9 9950X, 32 threads; v4.5.5-nikque.12 and this version in turn)

| `book deviate2 5 5` on the real book of 661.62 million positions (level 18, 2 runs each) | v4.5.5-nikque.12 | this version |
|---|---|---|
| Positions selected | 2,753,399 | 2,753,399 |
| Expanded in the 5 minutes after the selection (distribution executables) | 11,900 / 11,565 | 11,688 / 11,885 |
| The same (builds without PGO) | 11,926 / 11,980 | 12,032 / 12,003 |
| Peak memory | 33,409 - 33,412 MB | 33,410 - 33,413 MB |

- On the real book, two runs of the same executable differ more than the two versions do: no slowdown and no more memory were measured (2 runs each). One round of `book deviate2 3 6` on the book of 6.49 million positions took the same time (read in steps of 0.5 s).
- A small book command file (120 rounds of `book deviate` from an empty book): 8 threads 17.82 s -> 17.77 s (distribution executables, medians of 4 runs). 1 thread 22.87 s -> 22.96 s (the same; +0.4%), and +0.3% (4 runs each) and +0.1% (6 more runs each; same processor time) with builds without PGO: too few runs to tell a difference. Same peak memory.

**Finished after the publication** (added on 2026-10-08)

- **The Linux test set and the build that detects memory errors** (finished after the publication, on 2026-10-08, on the published source): 0 gcc warnings. The one-thread search of the Linux executable gives the node counts of the earlier versions. 198 API checks, 0 failures, with the 3 Linux libraries. With a limited number of threads (9, 12 and 20), the books are those of the usual build. ThreadSanitizer (8 threads, 8 cases) gave 2,498 reports; those where the code of the book or of the games reads or writes are of the 4 kinds known from before (search statistics, the signal that stops a search, the "to be saved" mark, the display of the progress). No function changed since v4.5.5-nikque.12 reads or writes in a report. The 88 book and game cases with the build that detects memory errors (AddressSanitizer of MSVC), in 3 settings: 0 errors detected, 0 abnormal exits (3 to 4 long cases were stopped at their time limit, with 0 errors detected until then). No case stops in a build with the assertions on.
- **One-thread time of the small book** (120 rounds of `book deviate` from an empty book; more runs): with the distribution executables, 8 runs each, v4.5.5-nikque.12 took 23.01 s and this version 23.00 s (medians; the 8 runs range from 22.59 to 23.20 s and from 22.61 to 23.42 s). The +0.4% of the 4 runs above does not show with 8 runs. Builds without PGO, 8 runs each: 21.46 s → 21.53 s (+0.3%; the median of the processor time is the same). No difference larger than 0.3% was seen; these runs cannot tell whether there is a smaller one. The peak memory is the same (95.2 to 95.6 MB).
- **Not measured**: the speed on Linux, macOS, 32-bit and ARM64; concurrent searches with books of level 19 or more. The Android version was only built. The macOS files come from the automatic build of the release.

## v4.5.5-nikque.12

**Checked** (test builds without PGO)

- The number of leaves is the `todo` that `book deviate X Y` / `book deviate2 X Y` first print on the same book (6 and 43 on a small book, 23,013 on the book of 6.49 million positions).
- On a small book (level 10, 1,563 positions, 120 of them solved) whose leaf scores were all altered, `book leaf-recalculate4` brought the 1,437 unsolved leaves back to their original scores. The 120 solved ones stayed altered (they are skipped).
- For each of the four commands, five settings of threads and searches (1 thread; 8 one-thread searches, twice; 8 threads with `auto`; 32 one-thread searches) gave the same book. On the book of 6.49 million positions, 32 threads with `auto` and 8 one-thread searches gave the same book. With the whole book in the range, running the same command a second time changed no score and no move (with a part of the book in the range, a second run reaches other leaves: see `book-leaf-recalculate-rounds` of v4.5.5-nikque.13).
- Speed, as an indication (6.49 million positions, level 18, 32 threads, count after 60 s, one run each): 16,794 leaves for `book leaf-recalculate2 3 6`, 8,698 expansions for `book deviate2 3 6`. **These are figures of a small book, not a forecast for a large one.**
- The peak memory was about that of the expansions of `book deviate` (992 to 1,007 MB against 999 to 1,028 MB for `book deviate2`, 6.49 million positions).

**The book changes even with the same `eval.dat`.** Share of the leaves whose score changed, on the book of 6.49 million positions (itself cut from a larger book by depth):

| Where the leaf came from | Scores changed |
|---|---|
| Leaf of a position added by `book deviate2` of this version (one-thread searches) | 0.1% (11 of 12,810) |
| Leaf of a position expanded by `book deviate2` of this version (one-thread searches) | 1.6% (225 of 14,180) |
| Leaves made by `book deviate2` of v4.5.5-nikque.9 (two-thread searches) | 3.3% and 4.9% |
| Leaves already in the book, except its deepest layer | 23 to 31% |
| Leaves already in the book, deepest layer (values set by the cut) | 72% |

- `book deviate` searches the leaf of an expanded position right after the search of its child, without emptying the hash tables. With a test build that empties them, all 6,676 leaves came back to the same score. In a later test (the audit of v4.5.5-nikque.13, another run), the differences of the added positions went away when a new position was searched on the board of the book (see "Not changed, and good to know" of v4.5.5-nikque.13).
- Searches with several threads give slightly different scores from run to run (the same leaves searched twice with 32-thread searches differed for 1.7% of them, and for about 5% from the one-thread result).
- **When `book subtree` reduces the depth, the leaf of a position of the new deepest layer becomes the score of a link to a removed position (the negamaxed value of the searches below it). `book leaf-recalculate3` and `4` replace it by the score of a search of that position at its level** (77% of the values changed on a layer that had just been cut). When that layer is solved (21 empties or fewer at level 18), it is skipped and nothing changes.
- The cause of the 23 to 31% of the row "already in the book, except its deepest layer" was not found (the version and the settings that made that book are not known).

**Not checked**: another `eval.dat`, levels above 18, a whole run on a book of 600 million positions, not enough memory for the searches, going on from a timed save. (Another `eval.dat`, the lack of memory for the searches and the timed saves were run during the audit of v4.5.5-nikque.13: see its "Checks".)

libedax: `edax_book_leaf_recalculate(X, Y)`, `edax_book_leaf_recalculate2`, `3` and `4` were added.

### Checks

- Book regression (all book commands; 1 and 8 threads; including the book of 6.49 million positions): all files identical to the release build of v4.5.5-nikque.11 (except `a17_game.edx`, a saved game that held memory that was never set, not a date: fixed in v4.5.5-nikque.13). The books of `book deviate`, `book deviate2` and `book deviate3` are the same.
- The selection walk of `book deviate` / `book deviate2` was timed in turn with a build of the v4.5.5-nikque.11 sources, 6 runs each: no difference could be read (6.49 million positions, `deviate2 8 20` with 32 threads: 451 to 473 ms against 454 to 481 ms).
- libedax: the 197 checks of the API test (the former 193 and the 4 new functions; 3 Windows libraries).
- ThreadSanitizer (Linux, 8 threads): no report about the new marking or about the code of the leaf recalculation themselves.
- The search code is unchanged: results and node counts of single-thread `-solve` are those of v4.5.5-nikque.11 (release builds).
- **Not measured**: the release builds against each other on an idle machine (only book commands changed), `book prune` on the real book, the speed on Linux, macOS, 32-bit and ARM64 builds, the marking when memory runs out.

## v4.5.5-nikque.11

### Checks

- Book regression (all book commands; 1 and 8 threads, including the book of 6.49 million positions): all files identical to the release build of v4.5.5-nikque.10 (except `a17_game.edx`, a saved game that held memory that was never set, not a date: fixed in v4.5.5-nikque.13).
- For each change, the saved books were compared with a test build that keeps the former behaviour (see the sections above).
- libedax: the 193 checks of the API test (3 Windows libraries). `edax_book_subtree` has the same change (the audit of v4.5.5-nikque.13 checked that it gives the same book as the command).
- The search code is unchanged. The results and node counts of single-thread `-solve` are the same as v4.5.5-nikque.10 with the release builds.
- **Not measured**: times with the release builds (PGO; the figures above are from builds of the same source without PGO), a comparison of the release builds on an idle machine (only book commands changed, so it was not done), `book prune`, `book correct` and `book enhance` on the real book, the speed on Linux, macOS, 32-bit and ARM64, `book subtree` and `book prune` on a damaged book with links going back.

## v4.5.5-nikque.10

### Speed and memory (release builds)

`wEdax-x86-64-v4.exe` for Windows, the release of v4.5.5-nikque.9 against this version (PGO build made the same way). Ryzen 9 9950X, 32 threads (1 thread on the first line), measured only while no other program was running, 8 to 64 runs. The ratio is this version ÷ v4.5.5-nikque.9, ± is the standard error.

| Condition | v4.5.5-nikque.9 | This version | Ratio | Peak memory |
|---|---|---|---|---|
| `-solve` (fforum-20-39), 1 thread | 1.211 s | 1.209 s | 0.999 ± 0.003 | same |
| the same, 32 threads (64 runs) | 0.361 s | 0.365 s | 1.012 ± 0.005 (see the note below) | same |
| `-solve` (fforum-40-59), 32 threads | 23.270 s | 23.326 s | 1.002 ± 0.006 | same |
| 30 midgame positions, level 18 | 0.940 s | 0.938 s | 0.998 ± 0.005 | same |
| 30 midgame positions, level 21 | 4.149 s | 4.119 s | 0.993 ± 0.008 | same |
| 20 `book negamax` (6.49 million positions) | 4.034 s | 3.786 s | 0.939 ± 0.003 | same |
| `book fix` (6.49 million positions) | 2.244 s | 2.216 s | 0.988 ± 0.006 | same |
| `book merge` (6.49 million positions) | 5.084 s | 4.979 s | 0.980 ± 0.007 | same |
| `book fix`, 1000 leaves (level 18) | 9.045 s | 9.135 s | 1.010 ± 0.011 | same |
| `book deviate 0 2` (270 thousand positions, `auto`, rounds of 1 to 7 positions) | 8.541 s | 8.494 s | 0.994 ± 0.042 (0.993 ± 0.006 for the time for each position) | same |
| `book learn`, level 21, 8 games | 10.314 s | 10.363 s | 1.005 ± 0.008 | same |
| `book learn`, level 24, 4 games | 10.166 s | 10.249 s | 1.008 ± 0.009 | same |
| 30 games stored one by one with `book store` (level 18) | 20.003 s | 20.227 s | 1.011 ± 0.013 | same |
| `book learn`, level 18, 128 games | 30.732 s | 30.731 s | 1.000 ± 0.004 | same |

The peak memory is the same in every condition. For the time, only the short `-solve` with 32 threads (0.36 s; the clock ticks every 0.016 s) came out slower beyond the error, 1.012 ± 0.005. fforum-40-59, which takes 23 s with the same 32 threads, shows no difference (1.002 ± 0.006), and 120 runs of this condition in turn with v4.5.5-nikque.9, while other programs were running, gave 0.999 ± 0.006. The search code is unchanged since v4.5.5-nikque.9. No other condition is slower beyond the error.

`book deviate` in a round with many positions (the book of 6.49 million positions, `book deviate3 2 6`, `auto`, positions expanded in 60 s; release builds, 3 runs): 8,529, 8,484, 8,462 → 10,801, 10,883, 10,848 (about 1.28 times), peak memory 1,432 MB → 1,002 MB.

**Not measured**: the book of 661.62 million positions with the release builds (the numbers of the real book above are between builds of the same source without PGO), the speed of the 32-bit, Linux, macOS and ARM64 builds, `book deviate` above level 18 (its rule is unchanged).

### Checks

- `-solve`: single-thread results and node counts are the same as v4.5.5-nikque.9 (the 5 Windows release executables, the 4 Linux ones, a 32-bit test build).
- Book regression tests (every book command; 1 and 8 threads, including the book of 6.49 million positions): every file is the same as with the code of v4.5.5-nikque.8 (except `a17_game.edx`, a saved game that held memory that was never set, not a date: fixed in v4.5.5-nikque.13).
- The tests of v4.5.5-nikque.9 were all run again on the code of this version: identical books with test builds that fix the searches to one thread, 240 runs of `book fix` with the "stop and continue" searches, repeated multi-thread `-solve` (8,000 positions on Windows, 3,000 on Linux, no wrong result), test builds where threads cannot be created (Windows and Linux), running out of memory in the 32-bit build, 94 cases of damaged books and 23 cases of settings, ThreadSanitizer on 8 cases (no new kind of report for the code added here), the Android build.
- libedax: 193 API checks (2 were added: `edax_stop` from another thread during `edax_bench`; 3 Windows and 3 Linux libraries), 81 cases of edge values and wrong calls, the tests of libedax4dart 7.67.0 (28 of 29; the remaining one is the same since v4.5.5-nikque.7).
- `book fix` (2 leaves) at levels 31 to 36 and the learning of one game at levels 31 and 32 end normally (v4.5.5-nikque.9 was checked up to level 30; one game took 104 s at level 31 and 188 s at level 32, with a peak of 2.15 GB; build without PGO, one run while other programs were running).

## v4.5.5-nikque.9

Measured (Ryzen 9 9950X, builds made the same way without PGO, 12 to 24 rounds; "before" is the code without this change only):

| Work | Before | This version | Ratio |
|---|---|---|---|
| Level 18: 128 games (32 threads, book of 270,000 positions) | 33.6 s | 30.3 s | **0.901 ± 0.002** |
| Level 18: 30 games | 8.64 s | 8.15 s | 0.944 ± 0.004 |
| Level 18: 30 games, `n-tasks` 8 | 21.6 s | 20.2 s | 0.935 ± 0.003 |
| Level 18: 30 games, `book-store-tasks` 8 | 9.42 s | 9.29 s | 0.986 ± 0.015 |
| Level 18: 8 games | 2.61 s | 2.59 s | 0.993 ± 0.009 |
| Level 21: 8 games | 10.33 s | 10.26 s | 0.994 ± 0.006 |
| Level 21: 8 games, `n-tasks` 4 | 40.9 s | 40.2 s | 0.981 ± 0.003 |
| Level 24: 8 games | 21.5 s | 21.4 s | 0.997 ± 0.006 |
| Level 24: 4 games / 2 games (8 and 16 threads each: no thread is added) | 10.03 s / 6.85 s | 10.05 s / 6.86 s | 1.002 ± 0.006 / 1.001 ± 0.011 |

- The peak memory is the same in every row.
- Note: a first version also gave threads to the games played with 8 threads or more; with 2 games at level 24 (16 threads each) it took 1.008 ± 0.005 times the time (72 rounds), slightly on the slower side. So only the games played with 7 threads or fewer get more threads.
- **Resulting book**: the last games of a group are now played by searches with several threads, so their moves can change from a run to the next. Learning 128 games (272,576 positions) three times: 9 to 13 positions differ between two runs before this change, 6 to 11 after it (the searches that get more threads while they run, since v4.5.5-nikque.8); between before and after, one more game was played differently (7 positions). To make the same books as the older versions, set `book-store-tasks = 1`, as before.
- Checked: with this change, the tests of the audit were run again (single-thread `-solve`, book regression, identical books with the test build where every search keeps one thread, builds where threads cannot be created, ThreadSanitizer, API checks), and 48 learning runs with different numbers of games, threads and `book-store-tasks` all ended normally, with nothing to fix in the books.

### Speed and memory (measured)

Ryzen 9 9950X (32 logical CPUs), `n-tasks` 32, `hash-table-size = auto`. The code of v4.5.5-nikque.8 and the code with the fixes of this version (without the change of "Learning games a little faster" above, whose effect is in the table above) were built with the same compiler and options (AVX-512, without PGO), and run in turn, in a changing order; the ratio is taken between the runs of the same round (± is the standard error). The release builds (with PGO) are compared under the table.

| Work | v4.5.5-nikque.8 | Rounds | This version / v4.5.5-nikque.8 (time) | Peak memory |
|---|---|---|---|---|
| Level 18: `book learn` of 128 games (book of 270,000 positions) | 32.5 s | 12 | 1.000 ± 0.004 | same (718 MB) |
| Level 21: `book learn` of 8 games | 10.0 s | 12 | 0.996 ± 0.008 | same (2.0 GB) |
| Level 24: `book learn` of 4 games | 9.6 s | 12 | 1.005 ± 0.008 | same (2.4 GB) |
| Level 24: `book learn` of 1 game (37 searches) | 5.1 to 11.7 s (mean 8.5) | 40 | 1.07 ± 0.06 | same (2.2 GB) |
| Level 21: the same | 2.0 to 4.7 s (mean 2.9) | 40 | 0.95 ± 0.04 | same (1.35 GB) |
| Level 18: 30 games, each one played then stored with `book store` | 18.3 s | 12 | 0.997 ± 0.006 | same (720 MB) |
| Level 18: `book add` of 30 games | 2.8 s | 24 | 1.00 ± 0.01 | same (720 MB) |
| Level 18: `book fix` with 1000 leaves to search again | 9.0 s | 12 | 1.003 ± 0.007 | same (701 MB) |
| Level 18: `book deviate 1 2` (book of 270,000 positions, `book-expand-tasks = auto`, about 3400 positions expanded) | 28.6 to 49.7 s (mean 35.6) | 9 | 0.95 ± 0.09 | 1,297 MB → 1,149 MB |
| Level 18: `book deviate 0 2` (the same book, about 370 positions expanded, 102 rounds) | 9.2 s | 16 | 0.918 ± 0.015 | 1,297 MB → 1,149 MB |
| `book merge` of a book of 6.49 million positions into an empty book | 4.86 s | 24 | **1.0075 ± 0.0028** | same (746-761 MB) |
| `book merge` of two real books of 270,000 positions | 0.13 s | 48 | 0.98 ± 0.01 | same (272 MB) |

- **`book merge` of the 6.49 million position book is 0.75% (about 0.04 s) slower** (2.7 times the standard error). Measured again with builds holding the fixes step by step (24 rounds): 1.008 ± 0.004 with the fixes of the parallel code and of the book, 1.005 ± 0.004 with all the fixes of this version; the difference comes from the former (the marks of all the positions checked when a merge starts, the links checked by negamax, ...). The peak memory is the same (746 to 761 MB). Between the release builds this difference does not appear (1.001 ± 0.003). For the other rows the difference is within the error, or on the faster side.
- `book deviate`: the searches of the concurrent expansion are now created with their final size at once (a part of the fix under "When memory or threads are missing"): the peak memory is about 150 MB lower, and a learning made of many rounds is a little faster. The time of a concurrent expansion varies much from a run to the next (the order of the expansions changes).
- Learning a single game (levels 21 and 24) takes a time that changes by a factor of 2 or more with the same executable. The time is the one of the last search still running (at level 24, the solving of a position with 30 empties): continued with 32 threads, it visited from 1.5 to 5.9 billion nodes depending on the run.
- Values of `book-store-tasks` (128 games at level 18, seconds / peak memory): `auto` 34.3 / 718 MB, `1` 109.0 / 281 MB, `2` 59.8 / 1155 MB, `4` 46.3 / 1155 MB, `8` 37.9 / 1587 MB, `16` 34.1 / 1587 MB. 8 games at level 21: `auto` 10.0, `1` 29.2, `2` 12.9, `4` 11.0, `8` and `16` 10.0 to 10.2. No value is slower than `1`, but with many games 2 to 8 are slower than `auto` and use more memory.
- `n-tasks` 4, 8 and 16 (ratio to the code of v4.5.5-nikque.8): 30 games learned at level 18: 0.996 and 0.996 (not enough rounds with 16); 8 games at level 21: 0.990, 0.997 and 0.985 (± 0.003 to 0.008): no difference. `auto` is still faster than `1` with 4 threads (8 games at level 21: 40.1 s against 43.2 s).
- Size of the hash tables of the searches done at the same time (19 bits up to level 18, 20 up to level 21, 21 above), changed in test builds: 128 games at level 18 take 1.017 ± 0.002 times the time with 1 bit less and 1.055 ± 0.004 with 2 bits less (peak memory 719 → 503 → 393 MB); 4 games at level 24: 1.021 ± 0.007 and 1.053 ± 0.009 (2433 → 1568 → 1136 MB); 8 games at level 21: 0.999 ± 0.011 and 1.024 ± 0.011. Larger tables only gain 1 to 2%. The sizes were kept.
- 32-bit build (without PGO, ratio to the code of v4.5.5-nikque.8): `-solve` 0.93 ± 0.02 with 1 thread, 1.005 ± 0.005 with 8 and 32 threads; 30 games learned at level 18: 0.997 ± 0.003; `book fix` with 1000 leaves: 1.015 ± 0.014; 8 games at level 21 (8 threads): 1.000 ± 0.002. **The peak memory of the commands that use searches done at the same time is about 110 to 120 MB higher** (718 → 830 MB, for example: 128 MB are allocated for a moment to check that they are free before a search is created; see "When memory or threads are missing").
- Level 30 (one run each): `book fix` with 2 leaves to search: 3.4 s with `auto`, 3.6 s with `1`; with 4 leaves: 11.9 s and 18.4 s (peak memory 684 MB and 262 MB). Learning one game (6 runs each): 97.4 s with the code of v4.5.5-nikque.8, 96.6 s with this version (ratio 0.99 ± 0.05), 136.1 s with `book-store-tasks = 1`.
- **The Linux library is about 8% faster** (x86-64-v3, the 30 problems of `edax_bench`, gcc 11.4; time against a library built as in v4.5.5-nikque.8): 0.987 ± 0.002 with `-Bsymbolic`, and 0.926 ± 0.001 with `-fno-semantic-interposition` as well (1 thread, same node counts); 0.924 ± 0.010 with 8 threads. With `-fPIC` alone, the calls between the functions of the library were kept replaceable by functions of another program, and were not inlined. The library grows from 622 KB to 765 KB. The API, the exported names and the search results are unchanged.
- **The release builds (with PGO, `wEdax-x86-64-v4.exe`) against each other** (time of this version against the released v4.5.5-nikque.8; 8 to 24 rounds): `-solve` 1.000 ± 0.002 with 1 thread and 1.000 ± 0.007 with 32; midgame with 32 threads 0.983 ± 0.004 at level 18 and 0.996 ± 0.009 at level 21; learning games: 0.899 ± 0.003 for 128 games at level 18 (34.9 → 31.4 s), 0.983 ± 0.007 for 8 games at level 21, 0.992 ± 0.008 for 4 games at level 24; 30 games stored one by one 1.006 ± 0.014; `book fix` with 1000 leaves 0.996 ± 0.009; `book merge` of the 6.49 million position book 1.001 ± 0.003; `book deviate 0 2` 0.954 ± 0.033. **No condition is slower.** Same peak memory (1,298 → 1,150 MB for `book deviate`).
- Linux executables (against the released v4.5.5-nikque.8, `-solve`, 8 rounds): 0.994 for x86-64, 0.988 for x86-64-v3 (midgame 1.001, 8 threads 0.990), 0.995 for x86-64-v4: the same speed.
- **Not measured**: the macOS, Android and Windows ARM64 builds (they cannot be run here), levels above 30.

- `book-store-auto-save = off`: 1.004 ± 0.005 times the time of `on` for 128 games learned on the book of 270,000 positions, 0.99 ± 0.04 on a book of 770,000 positions, 0.99 ± 0.01 for 30 games learned one by one: with books of this size the difference cannot be measured (a save takes about 0.013 s). With the book of 6.49 million positions (a save takes 0.2 to 0.3 s): 0.966 ± 0.002 for 128 games (23.8 → 23.0 s), 0.87 ± 0.02 for 8 games learned one by one (18.7 → 16.3 s). The setting halves what is written to the disk, and the time of a save grows with the book.
- `book negamax` and the number of threads (6.49 million positions, one negamax): 1.97 s with 1 thread, 1.00 s with 2, 0.53 s with 4, 0.30 s with 8, 0.21 s with 16, 0.19 s with 32: almost proportional up to 8 threads, flat above. The work lost by threads walking the same positions is about 20% with 32 threads, 3% with 8.
- edax_runner at level 18 with 32 threads, 3,200 games learned in 16 minutes: its memory stayed between 717 and 725 MB.

**The real book of 657 million positions** (28.95 GB, level 18; the first check on it since v4.5.5-nikque.5). 32 threads, one run each.

| Work | v4.5.5-nikque.8 | This version | Result |
|---|---|---|---|
| Load → `book info` → save | 35.9 s, peak 31.6 GB | 36.3 s, 31.6 GB | the saved file holds the same positions' bytes as the original file (both versions) |
| `book negamax` (with the load and the save) | 53.4 s (negamax: about 19 s) | 56.0 s (about 19.6 s) | same book with both versions |
| `book merge` of another real book of 657 million positions (1.74 million positions added; with the load and the save) | 254.5 s, peak 32.9 GB | 256.0 s, 32.9 GB | same book with both versions (658,615,773 positions) |
| `book fix` (with the load and the save) | 239.6 s, peak 32.0 GB | 239.2 s, 32.0 GB | 64 links added, 31 searches. The saved books differ by the leaf move of 1 or 2 positions between two runs of the same version, and by as much (1 position) between the two versions |

- The release executable of this version also loaded the book, ran `book negamax` and `book merge` in a row and ended normally, with the same numbers of positions, links and leaves as above (peak 32.9 GB).
- No crash, error or warning. These are single runs: a difference of a few percent is within the noise. The times are about 100 times the ones of the 6.49 million position book (proportional to the number of positions). `book merge`: checking the file 13 to 16 s, adding the positions 18 s, rebuilding the links 145 to 150 s, checking the positions 17 s, negamax 19 s. `book fix`: checking the positions about 20 s, rebuilding the links 164 s, negamax 19 s.
- The book made by `book fix` changes slightly from a run to the next: with each version run twice and the books compared position by position, the leaf move differs in 1 position between the two runs of v4.5.5-nikque.8, in 2 positions between the two runs of this version, and in 1 position between the two versions (same scores; all the other 656.87 million positions are equal). The leaf searches, done with several threads, choose one or the other of two moves with the same score.

### Checks

- `-solve`: single-thread results and node counts equal to v4.5.5-nikque.8 (Windows x64 and 32-bit, Linux).
- Book regression tests (all the book commands; 1 and 8 threads; including a book of 6.49 million positions): all files equal to v4.5.5-nikque.8 (except `a17_game.edx`, a saved game that held memory that was never set, not a date: fixed in v4.5.5-nikque.13; and except the messages described above).
- With the test build whose searches all use one thread, the books are identical to "one search after the other, with an empty hash table for each" (the ten damaged books, `book store`, `book add`, `book learn`, and merges).
- 240 runs of `book fix` with 32 threads for the "stop and go on" step, repeated multi-thread `-solve` (no wrong result), test builds where threads cannot be created or the search memory cannot be allocated, the 32-bit build with its address space exhausted, damaged books, unwritable targets and odd settings, ThreadSanitizer (Linux).
- The real book of 657 million positions: load, save, `book negamax` and `book merge` give the same result as v4.5.5-nikque.8 (see "Speed and memory" above).
- `book-store-auto-save`: with `off` no `.store` file is written, and the saved book is the same as with `on`.
- With the two last changes (this setting, and the threads handed over while learning), all the tests above were run again (including the damaged books, the odd settings and the tests of edax_runner).
- libedax: 191 API checks, 81 cases of edge values and wrong calls, the tests of libedax4dart 7.67.0 (28 of 29; the remaining one is the same as in v4.5.5-nikque.7).

The bugs of the upstream parallel search have not been reported upstream (okuhara/edax-reversi-AVX).

## v4.5.5-nikque.8

### Measurements

Ryzen 9 9950X (32 logical CPUs), `n-tasks` 32, `hash-table-size = auto`, the AVX-512 Windows build. Times in seconds.

| | `1` (the searches of v4.5.5-nikque.6) | `auto` of v4.5.5-nikque.7 | `auto` of v4.5.5-nikque.8 |
|---|---|---|---|
| level 24: play a game, then `book store` (34 searches) | 13.4 | **26.0** | 5.8, 10.6 |
| level 21: the same | 4.2 | **6.4** | 2.0 |
| level 18: 30 games, each one played then stored (a book of 270,000 positions) | 34.5 | 29.0 | 18.2, 18.3 |
| level 24: `book fix` with 1 leaf to search again | 1.1 to 1.5 | **6.2** | 1.2 (the same search as `1`) |
| level 24: the same, 4 leaves | 2.0 to 3.0 (average 2.5) | **6.1** | 1.4 to 2.5 (average 1.7) |
| level 24: the same, 16 leaves | 6.8 to 9.2 (average 7.8) | **8.9** | 2.9 to 3.7 (average 3.2) |
| level 18: the same, 1,000 leaves (a book of 270,000 positions) | 54.1 to 58.8 | 17.7 to 18.2 | 9.2 to 9.7 |
| level 24: `book learn` of 4 games (new book) | 36.0 | 13.2 | 10.6, 10.7 |
| level 21: `book learn` of 8 games (new book) | 31.2 | 17.9 | 11.0, 11.1 |
| level 18: `book learn` of 128 games (a book of 270,000 positions) | - | 36.1 | 33.5, 33.6 |
| level 18: 128 games learned by edax_runner | 112.9 (measured with v4.5.5-nikque.7) | 37.2, 38.8 | 35.1, 35.9 |
| level 18: `book add` of 30 games | 24.0 (the same) | 3.1 | 3.0 |
| `book merge` of a book of 6.49 million positions into an empty book (127 searches) | as v4.5.5-nikque.6 | about 0.4 s slower (4.7 to 5.1) | as v4.5.5-nikque.6 (4 alternate runs each: 5.7 to 6.3 and 5.8 to 6.3) |

- In bold: where `auto` of v4.5.5-nikque.7 was slower than `1`. `auto` of v4.5.5-nikque.8 was as fast as `1` or faster in everything that was measured.
- With few searches, the time changes from a run to the next (searches with several threads). The averages are those of 10 to 20 runs.
- `book merge` of two real books of 270,000 positions (1 search) took 0.12 s, against 0.21 s with v4.5.5-nikque.6.
- `book fix` of a sound book (6.49 million positions, no search) took 2.2 s, against 21.5 to 21.9 s with v4.5.5-nikque.6 (23.3 s and 23.2 s with one thread), with the same peak memory of 407 MB (this code is the one of v4.5.5-nikque.7).
- **Against several edax_runner at the same time** (128 games at level 18, with the time to merge their books): 42.3 s + 1.9 s of merges with 8 of them and 4 threads each, 34.9 s + 2.6 s with 16 and 2 threads each. One edax_runner with `auto` took 35.1 and 35.9 s.

**Peak memory** (each of the searches done at the same time has its hash tables):

| | `1` | `auto` of v4.5.5-nikque.7 | `auto` of v4.5.5-nikque.8 |
|---|---|---|---|
| level 24: `book store` of a game | 262 MB | 2.2 GB | 2.2 GB |
| level 24: `book fix` with 4 / 16 leaves | 261 MB | 462 MB / 1.1 GB | 684 MB / 1.1 GB |
| level 21: `book store` of a game | 261 MB | 1.3 GB | 1.4 GB |
| level 18: `book fix` with 1,000 leaves | 274 MB | 696 MB | 701 MB |
| `book merge` of 6.49 million positions | 0.55 GiB (v4.5.5-nikque.6) | 0.96 GiB | 0.57 GiB |

- With 32 threads, the hash tables of the searches done at the same time take up to about 450 MB up to level 18, 0.9 GB up to level 21 and 1.8 GB above (in proportion to the threads). With few searches, each one uses more threads and gets larger tables (never more in total than these values). Set `book-store-tasks = 1` if this memory is a problem.

**Resulting books**:

- A search that ends with its single thread gives the same result as with v4.5.5-nikque.7 (it only depends on the position). A search that got more threads, or that started with several, can give a slightly different result from a run to the next, as any search with several threads. Two runs of the learning of 128 games gave books (272,576 positions) with another leaf move in 7 positions and the same scores everywhere (`auto` of v4.5.5-nikque.7 gave the same book every time). The book differs from the one of `auto` of v4.5.5-nikque.7 in 19 positions.
- With a test build where every search keeps one thread, the books are still exactly those of the searches done one after the other with empty hash tables (the 10 damaged books, `book store`, `book add`, `book learn`, merges), as with v4.5.5-nikque.7.
- Test of the searches continued with more threads: 300 runs of `book fix` with a few leaves and 32 threads all ended normally (840 continued searches counted in 120 of them), and `book fix` of v4.5.5-nikque.6 has nothing to change in the books they make.

## v4.5.5-nikque.7

**Measured speed** (Ryzen 9 9950X with 32 logical CPUs; a real book of 270,000 positions at level 18, depth 40; the AVX-512 Windows build). 128 games (their first 32 moves on average are given; Edax plays the rest of each game) learned by edax_runner with the libedax of this fork; the times include playing and learning.

| `n-tasks` | `book-store-tasks` | 128 games | Per game | Peak memory |
|---|---|---|---|---|
| 8 | 1 | 133.5 s | 1.04 s | 179 MB |
| 32 | 1 | 112.9 s | 0.88 s | 290 MB |
| 32 | 2 (16 threads per game) | 105.3 s | 0.82 s | 1.2 GB |
| 32 | 4 (8 threads per game) | 76.4 s | 0.60 s | 1.2 GB |
| 32 | 8 (4 threads per game) | 59.7 s | 0.47 s | 1.6 GB |
| 32 | 16 (2 threads per game) | 48.7 s | 0.38 s | 1.6 GB |
| 32 | auto (32 games, 1 thread per game) | 39.4 to 41.6 s (6 runs) | 0.31 to 0.33 s | 722 MB |
| 16 | auto (16 games) | 63.4 s | 0.50 s | 503 MB |
| 8 | auto (8 games) | 102.9 s | 0.80 s | 285 MB |

- `auto` is 2.8 times (`n-tasks` 32) to 3.3 times (`n-tasks` 8) faster than `1`. The more games at the same time, the faster (a one-thread search does the most work per core).
- **Against several edax_runner at the same time** (the usual way so far): the same 128 games shared between several edax_runner (all with `book-store-tasks = 1`, each one with its own book) took 53.4 s with 4 of them and 8 threads each, 41.1 s with 8 x 4 threads, 35.2 s with 16 x 2 threads and 41.3 s with 32 x 1 thread (0.7 to 3.5 GB of memory in total; the time to merge the books afterwards is not included). One edax_runner with `auto` (40.2 to 40.8 s, 722 MB) is about as fast, with a single book and no merge.
- With `1`, this version is also faster than the libedax before this work (165.3 s to 133.5 s with `n-tasks` 8): the links of the book are now rebuilt with several threads after `book store` (see "A faster book fix" below).
- `book add` (30 games added to the book above; from the start of Edax to the saved book) took 3.4 s with `auto` and `n-tasks` 32, 24.0 s with `1` (24.4 s with `1` and `n-tasks` 8).
- **`book store` of a single game gains little.** Playing then storing 30 games one by one took 33.2 s with `auto` and 37.9 s with `1` (`n-tasks` 32; the time includes the games). The 20 to 30 positions of a game are searched at the same time, but the longest search still has to end. For the same reason, a small `book-store-tasks` such as 2 or 4 gains little (see the table above). The default, `auto`, was the fastest of the measured values.

**How much the book differs** (the same 128 games; position by position). The book learned with `auto` is not the same as the book learned game by game, but it differs no more than the books of the original learning differ when the number of threads changes. And `auto` gives the same book every time (its searches use one thread and start with empty hash tables). This is why `auto` is the default. Set `book-store-tasks = 1` when you need the same book as v4.5.5-nikque.6.

| Compared books (about 272,600 positions each) | Positions with different contents | Positions in one book only | Positions with different scores (by 1-2 / 3-4 / 5-8) |
|---|---|---|---|
| 1 thread, `1`: 2 runs | 0 | 0 | 0 |
| 32 threads, `auto`: 3 runs | 0 | 0 | 0 |
| 1 thread, `1` and 32 threads, `auto` | 372 | 5 and 8 | 95 (66 / 13 / 16) |
| 1 thread, `1` and 8 threads, `1` | 381 | 192 and 196 | 136 (123 / 9 / 4) |
| 1 thread, `1` and 32 threads, `1` | 439 | 185 and 186 | 141 (128 / 8 / 5) |
| 8 threads, `1` and 32 threads, `1` | 261 | 90 and 87 | 75 (67 / 8 / 0) |

- "Different contents": the position is in both books with another score, leaf or link. Of the 372 positions that differ between `auto` and one thread, 152 have another leaf move and 88 another leaf score only.
- Two things make the difference: (1) each search starts with empty hash tables (in the original learning, what the searches of the previous positions of the game left in the hash tables slightly changes the next searches); (2) the games of a group are played without what the other games of the group teach.
- With `book add` (30 games), the book of 32 threads with `auto` differs from the book of 1 thread with `1` in 60 positions (all the score differences are 1 or 2), with no position in one book only; the book of 8 threads with `1` differs in 97 positions. Two runs with `auto` gave the same book.
- **Check of the mechanism**: with a test build of the original learning that also empties the hash tables before every search, and one search thread, the books made by `book store` and `book add` are exactly the same as with the planned searches. So the difference from the original learning only comes from the empty hash tables. Emptying them makes the searches visit about 9% more nodes (30 games at level 18).

**Memory**: each of the one-thread searches has its hash tables. They take 14 MB each (19 bits) up to level 18 (the searches are short: they visit only 0.7% more nodes than with 21 bits), whatever `hash-table-size` is. The 722 MB of `auto` in the table above are these 32 searches (about 450 MB), the hash tables of the search of the prompt (226 MB), the book, etc.

Measured (load a book of 6.49 million positions, 286 MB, then `book fix`; seconds):

| | Upstream v4.5.5 | v4.5.5-nikque.6 | v4.5.5-nikque.7 |
|---|---|---|---|
| 32 threads: total | 25.4 | 22.9 | **2.3** |
| checking / linking / negamax | 0.1 / 22.4 / 2.9 | 2.2 / 20.5 / 0.2 | 0.2 / 1.9 / 0.2 |
| 1 thread: total | 25.4 | 24.7 | 24.7 |
| Peak memory | 660 MB | 407 MB | 407 MB |
| `book negamax` (32 threads / 1 thread) | 3.2 / 3.2 | 0.2 / 2.3 | 0.2 / 2.3 |

- About 10 times faster with 32 threads; with one thread, the same time and memory as v4.5.5-nikque.6.
- The checking is longer than upstream because the links to positions missing from the book are also looked for since v4.5.5-nikque.2.
- On a book of 770,000 positions (32 threads): upstream 2.3 s, v4.5.5-nikque.6 1.9 s, this version 0.1 s.

**With many leaf searches** (a real level 18 book of 270,000 positions where a link of 1,000 positions was moved back to the leaf, then `book fix`: 1,000 leaves have to be searched; 32 threads, `hash-table-size = auto`):

| `book-store-tasks` | Time | Peak memory |
|---|---|---|
| `1` (one after the other, 32 threads each) | 56.5 to 58.3 s | 288 MB |
| `auto` (default: 32 one-thread searches at the same time) | 18.2 to 18.5 s | 730 MB |

- About 3 times faster. The book is exactly the same as the one of a test build that does these searches one after the other and empties the hash tables before each one.
- **A usual `book merge` gains almost nothing from this**: it reuses the leaves that the merged book searched at the same level, so it has almost no search to do (merging two real books, each one learned from 64 other games on the same book, added 908 positions and needed 1 search; merging a book of 6.49 million positions into an empty book needed 127). Many leaves are searched when `book fix` repairs a book with missing links, when the merged book has another level, etc.

## v4.5.5-nikque.6

(No test or measurement record for this version.)

## v4.5.5-nikque.5

### How strength was measured, and changes not taken

Two versions played two games (colors swapped) from each of 1102 balanced start positions (random 8-move openings scored within ±3 at level 18; 1087 10-move openings were also used) over the NBoard protocol. Elo and its 95% confidence interval were computed on the pairs of games. Timed comparisons used `-nps`, which counts the search clock in nodes: results do not depend on the CPU load or on the speed of each core, and a match is exactly reproducible (with `-nps`, the game clock is now also charged in nodes).

Changes that improved neither strength nor speed (1 thread), and were not taken:

- Search constants that only change node counts: `LASTFLIP_HIGHCUT`/`LASTFLIP_LOWCUT`, `ETC_MIN_DEPTH` 4 and 6, `DEPTH_MIDGAME_TO_ENDGAME` 13 and 17, move sorting depths (`inc-*-sort-depth`): speed differences within the measurement noise (±3%) or slower.
- ProbCut strength (t) and shallow depth (`probcut-d`): the defaults were close to the best (a lower t was much weaker, a higher one no better).
- Move sorting in depth-2 searches: 5.5% fewer nodes, but 8-13% slower.

## v4.5.5-nikque.4

### Output checks

Compared with v4.5.5-nikque.3 under the same conditions (fixed `-h`, `book-expand-tasks = 1`):

- Single-thread `-solve`: fforum-20-39 and 30 midgame positions on the Windows x86-64, v3, v4, x86 and x86-sse builds and the Linux x86-64, v3, v4 and x86 builds; fforum-1-19 and fforum-40-59 on Windows v4. Same best moves, scores, principal variations and node counts.
- fforum-40-59 on 32 threads (two runs of each version): same scores; best moves differ only between moves of equal score, as between two runs of the previous version.
- The book test suites (all book commands on 1 and 8 threads, merges, the 6.49-million-position book): same saved books. The log differences are the known ones: random choice among equal book moves, `.edx` values, times, and source line numbers in error messages.
- The real book: loaded and saved identical except the save date.

The ARM64 Windows and Android executables were built but not run.

## v4.5.5-nikque.3

### Time and memory

Measured on a real 28.95 GB book of 656.9 million positions (Ryzen 9 9950X, 32 logical CPUs, with another Edax process running on the same machine).

| Operation (whole book) | v4.5.5-nikque.2 | v4.5.5-nikque.3 |
|---|---|---|
| Load the book | 445.6 s | 48.9 s |
| Memory after loading | 58.4 GiB | 35.5 GiB |
| Save the book | 657.2 s | 28.3 s |
| Negamax (run by the deviate commands at start and after every round) | 936.5 s | about 33 s |
| One round of `book deviate 2 4` (nothing to expand) | 2,909.6 s | 142.0 s |
| Selecting the positions to expand, deviate2 / deviate3 | 446.8 s / 479.2 s | 21.4 s / 22.9 s |
| `book merge` of two 660-million-position books (below) | 16,408 s (4 h 33 min), 117.2 GiB | 263 s, 37.0 GiB |

When many positions are selected, a `deviate2` or `deviate3` round spends most of its time searching them (two searches per position: the new child and the parent). That search time is unchanged.

### `book merge` measurement

Another real book (657 million positions, 29.03 GB) was merged into the 28.95 GB real book of 657 million positions (1.736 million positions added). Both released `wEdax-x86-64-v4.exe` builds were run one at a time on the same PC with the same books.

| Step | v4.5.5-nikque.2 | v4.5.5-nikque.3 | nikque.3 without leaf reuse (reference) |
|---|---|---|---|
| Load the book at startup | 222 s | 28 s | 30 s |
| Whole `book merge` command | 16,408 s (4 h 33 min) | 263 s | 10,934 s (3 h 2 min) |
| of which link rebuild | 15,332 s | 177 s | 10,845 s |
| `book save` after the merge | 251 s | 18 s | 62 s |
| Start to exit | 17,364 s (4 h 49 min) | 315 s | 11,027 s |
| Peak memory | 117.2 GiB | 37.0 GiB | 37.0 GiB |

Most of the merge time is spent searching again the positions whose best move without a link (the Leaf) became a link when the links were rebuilt (about 184,000 positions here). When the source book has already searched such a position at the same level, v4.5.5-nikque.3 reuses that result instead of searching again. This alone makes the merge about 42 times faster (10,934 s to 263 s). The other changes (streaming the source book, parallel processing and so on) make it about 1.5 times faster and reduce the memory from 117.2 GiB to 37.0 GiB.

**Because of this reuse, a few scores of the merged book may differ slightly.** The reused Leaf move is the move a new search should find, but its score can depend on the hash table contents and on the progress of the parallel search. In this merge, 27,897 of 659 million positions (0.004%) have scores that differ from nikque.2: 14,200 directly, the others through negamax to their parents. Most differences are 1 or 2. The set of positions, the link moves, the win/draw/loss counts and the levels are identical. nikque.2 itself gives different scores for the positions it searches again from one run to the next: in a merge of two reduced real books (34.84 million positions), two runs of nikque.2 differed in 1,655 positions, about as many as between nikque.2 and nikque.3 (about 2,500).

Main changes:

- Books are read and written through a 16 MB buffer (previously about 15 small reads or writes per position).
- A position takes 56 bytes in memory instead of 64. Up to 4 links are stored inside the position (99% of the positions of a large book) instead of a separate allocation.
- A book saved by Edax is loaded into one exactly sized block (previously about 20% of the arrays were spare capacity).
- Negamax, the selection of the positions to expand, and the link rebuild, check and sort of `book merge` run on `n-tasks` threads (the number of CPUs by default). The results are the same as with one thread.
- Each round no longer rewrites a flag in every position or scans the whole book for positions to expand. Positions are expanded in the same order as before.
- `book merge` reads the source file twice (first to check it, then to add positions) instead of loading it, and leaves the destination unchanged if the source is damaged. When the source book has already searched a position, its result (the best move without a link) is reused instead of searching again (scores may differ slightly; see "`book merge` measurement" above).

Use `n-tasks` (`-n`) to limit the threads if you play or analyze games while a book is learning.

### Output checks

The following commands were run with the previous and the new version under the same conditions, and their outputs were compared. The only differences are the ones that also change between two runs of the previous version: the random choice among equally scored book moves, values stored in `.edx` files, and displayed times and speeds. The `.mrg` file and message added by the new automatic merge save are outside this comparison (with `book-merge-auto-save = off` the output matches the previous version exactly).

- book: `new`, `load`, `save`, `import`, `export`, `merge`, `info`, `stats`, `show`, `analyze`, `fix`, `negamax`, `correct`, `prune`, `subtree`, `add`, `check`, `problem`, `extract`, `deviate`, `deviate2`, `deviate3`, `enhance`, `play`, `deepen`, `feed-hash`, `store`, `depth`, `randomness`, `on`, `off`
- game: `play`, `go`, `hint`, `save`, `load`, `vmirror`, `hmirror`, `rotate`, `undo`, `redo`, `setboard`

The checks used one and eight threads, small generated books and a 6.49-million-position book cut from the real book, and five Windows builds (x86-64, v3, v4, x86, x86-sse). The search hash handling is unchanged.

Three outputs change on purpose:

- After `book merge`, the scores of the positions whose source search result was reused when the links were rebuilt, and of their parents (they may differ slightly; see "`book merge` measurement").
- The scores of positions added by `book merge` that cannot be reached from the initial position (they used to keep values such as +/-127).
- The order of the saved positions after a `book merge` whose result exceeds 32 positions per bucket of the destination (for example a large merge into a book just created with `book new`), because the number of buckets is increased. The positions are the same.

### Comparison with upstream v4.5.5

The executables of the upstream `edax-4.5.5.zip` (SHA-256 `6f446149092b37cbaa57f68a5610c6e960d45c4f0cbb9a3c8e7a563b503fa9a4`) and v4.5.5-nikque.3 were run under the same conditions and their outputs compared (Windows x86-64-v4, x86-64 and x86 builds). `deviate2`, `deviate3` and the save settings, which upstream does not have, were left out.

The saved books, text outputs and screen output match upstream for:

- a 6.49-million-position book cut from the real book: `load`, `stats`, `negamax`, `export`, `show`, `subtree`, `prune`;
- generated books: `new`, `deviate`, `enhance`, `negamax`, `fix`, `correct`, `export`, `import`, `subtree`, `prune`, `problem`, `extract`, `check`, `add`, `depth`, `store`, `play`, `deepen`, and the game commands (`play`, `go`, `hint`, `save`, `undo`, `redo`, and others).

The differences from upstream all come from fixes or display changes:

- Upstream crashes (fixed): displaying a position without any move (pass only) crashes upstream (with the real book, `book info` does). A stale `nomove` Leaf in the destination makes `book fix` crash after a merge.
- `book merge`: upstream does not rebuild Links; it prints "Book needs to be fixed before usage" and expects `book fix`. Running `book fix` right after the merge gives the same saved book as this version, which rebuilds, repairs, negamaxes and sorts the book during the merge. A missing or truncated source file is rejected here; upstream merged an empty book or the positions read before the truncation.
- `.edx` save: upstream writes the file in text mode on Windows, so a saved `.edx` cannot be read back correctly.
- Save on exit: upstream saves to the configured book file (`data/book.dat`) on exit even after `book save`. This version saves on exit only if the book changed after the last explicit save.
- Display: upstream also printed intermediate `todo` counts of `book deviate` every 10 positions; this version prints the final count only. The "Memory occupation" of `book info` follows the new in-memory size of a position.

While `book load` reads another book, the current book is kept so that it survives a failed load (since v4.5.5-nikque.2), so memory for both books is needed temporarily. Loading at startup and `book merge` do not need this extra memory.
