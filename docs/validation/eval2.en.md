# Edax 4.5.5 eval2: tests and measurements

Tests and measurements done for each version, moved without changes from the former README-NIKQUE.en.md.

For what changed, see the [release notes](../../RELEASE-NOTES.md). "Above" and "below" in the text refer to positions in the former document.


## v4.5.5-eval2.17

Checks:

- `tests/book-resume.py` (new, 40 checks, 1 thread on one logical CPU): `book correct` with 1 and 2 threads (24 solved positions of 14 empties; half of them marked with a wrong score: only the 12 others are searched, the wrong scores stay, the marks go; a third run finds the 12 wrong scores), `book deepen` (11 positions of level 1 get the level 60 of the book; a second run searches none), `book leaf recalculate3` (every position marked: no leaf searched; half marked with a wrong score: they keep it; the marks go), `book learn` (a file of 6 games; a progress file of its first 3 lines: the 3 others only; the progress of another book or another file is not used; a run stopped as soon as its progress file was written, then a new run on the book saved to `.store`: the other lines only), and v4.5.5-eval2.16 reads a book with marks as a damaged book.
- The 14 book suites of the reproducibility work (A, B, F, G, MB, UA, UB, UF, S1, S2, S3, S3off, M1, L18; 1 thread, without `book-reproducible`), with a build of this source and with the release program of v4.5.5-eval2.16: the same files except the save date in 13 suites; S3 (`book learn` with the random seed of the clock) differs from run to run with either program, and with `-book-seed 1` both give the same books twice.
- `tests/regression.c`: the 50 cases pass (a build of this source, one logical CPU).
- The 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same depths, scores, moves and node counts as v4.5.5-eval2.16 (78,733,311 nodes); the 1,205 positions of the exact-search fix: 0 wrong scores; the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible`: the same books as v4.5.5-eval2.16, except the save date. The programs are 4 to 7 KB larger.
- Built by the release workflow (run 44156978523): the library test (251 checks, 0 failures) and fforum-1-19 on macOS, with the same node counts as v4.5.5-eval2.16.
- Speed: not measured (the search is not changed).

## v4.5.5-eval2.16

Checks:

- The 5 Windows release programs (PGO) solve the 1,205 positions whose exact scores were wrong (`-l 60`, 1 thread, hash 2^20; the scores of an independent solver) with 0 wrong scores (366 with the programs of v4.5.5-eval2.15). A test build of the same source gives the same scores as before on 1,640 positions of 17 to 22 empties (almost the same node counts); with the same fix, the C++23 port (newedax) gets all 1,205 right with 1 and 4 threads and hash tables of 2^20 and 2^24.
- Search: the 5 Windows release programs solve fforum-20-39 with 1 thread with the same depths, scores and moves as v4.5.5-eval2.15 (78,733,311 nodes, 78,733,223 with v4.5.5-eval2.15); the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible`: the same books as v4.5.5-eval2.15, except the save date.
- `tests/regression.c` `core` (96 exact searches against an independent solver) passes with 1 and 4 threads.
- Built by the release workflow (run 43743485109): the library test (251 checks, 0 failures) and fforum-1-19 on macOS.
- Speed: not measured.

## v4.5.5-eval2.15

Checks:

- Before and after the changes (builds of the same source), on test books with 1 and 4 threads: the book commands (76 cases) write the same files and give the same answers, the analysis commands (20,046) give the same answers, and 25 cases of the other commands give the same output (but the time and the line numbers of `book.c` in its error messages). The differences of `tests/regression.c` are the line numbers of `book.c`, the new cases and checks, and `enhance`, which varied from run to run before (a random seed from the clock). The new checks of the bugs (`tests/libedax_test.c`, `tests/book-analyze.c`, `tests/regression.c`) fail with the code before the fixes, but a few that cannot hit the bug on purpose (checked with test copies of the code).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`). The unit tests of the analyses (`tests/book-analyze.c`) pass, the earlier ones with the numbers of the previous version, and the 3 new ones (`book moveinfo` without memory, the limit of the numbers of the items, the memory of a count given away) too.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and lines as v4.5.5-eval2.14; the 7 Cassio cases pass. The 3 DLLs: the API checks (`tests/libedax_test.c`, 279 checks), 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-eval2.14. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 251 checks (all but the 28 for Windows only), 0 failures. Android (NDK r27d): no warning. Release CI (run 42432244412): the Linux and macOS libraries pass 251 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the handling of the book files that could not be loaded or opened, the fixes of the commands above, the functions of libedax, and the code rearranged without changing what it does). The node counts above check that the search is the same.
- Size (against v4.5.5-eval2.14): the 5 Windows programs −1,024 to +1,536 bytes, the 3 DLLs +1,536 to +3,584 bytes. Built by the release workflow: the Linux programs −4,096 to +4,128 and libraries −4,096 to +8,192, the macOS programs +64 and −8,136 and library +480, Windows ARM64 +3,584, the Android programs −696 and +344 and libraries +520 and +32 bytes.
- Speed: not measured.

## v4.5.5-eval2.14

Checks:

- The test of the export: the lines written are compared with sets computed apart in Python (the positions with a leaf to expand, the ends, all the positions of the lines): on 41 positions of 3 test books, the 4,545 cases (3 commands, 5 ranges, 3 kinds) are all the same; every line can be played from the start, its moves follow the walk, and with `book deviate2` and `book deviate3` its total loss is the smallest. The mark `export=1` in the first answer is there.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`). The unit tests of the analyses (`tests/book-analyze.c`) pass with the numbers of the previous version.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and lines as v4.5.5-eval2.13; the 7 Cassio cases pass. The 3 DLLs: 204 API checks (verify18), 0 failures; the test with the export (`tests/libedax_test.c`, 218 checks) has no failure either. Suite F of `book-reproducible` gives the same books as v4.5.5-eval2.13. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 218 checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 41843776946): the Linux and macOS libraries pass 218 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the export and its mark in `book_analyze.c`, the reading of the command and `help book` in `edax.c`, and the function of libedax are new). The node counts above check that the search is the same.
- Size (against v4.5.5-eval2.13): the 5 Windows programs +3,584 to +5,120 bytes, the 3 DLLs +17,920 to +19,456 bytes. Built by the release workflow: the Linux programs +8,192 to +12,288 and libraries +8,192 to +16,384, the macOS programs +16,512 and +12,288 and library +128, Windows ARM64 +6,656, the Android programs +7,496 and +3,656 and libraries +8,520 and +4,368 bytes (the functions of the export).
- Speed: not measured.

## v4.5.5-eval2.13

Checks:

- The test of the order of the terms (the tests of nikque.25, `tests/book-analyze.c`): every test, `umigame-order` included, passes with the same numbers as nikque.25 (gcc and MSVC give the same number, 39, in the test of dropped terms).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and lines as v4.5.5-eval2.12; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-eval2.12. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40650324945): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the order in which `book_analyze.c` drops the terms of an umigame number is changed). The node counts above check that the search is the same.
- Size (against v4.5.5-eval2.12): the 5 Windows programs +0 to +1,536 bytes, the 3 DLLs the same size. Built by the release workflow: the Linux and macOS executables and libraries and Windows ARM64 the same size, Android executables +32 and +32 bytes and libraries +32 and +16 bytes.

## v4.5.5-eval2.12

Checks:

- The tests of what is taken over (the tests of nikque.24, `tests/book-analyze.c`): the umigame numbers, the numbers of positions, `book levels` and `book missing` taken over are those found from each position alone (the same numbers as nikque.24); the tests of the precision stages (502,470 moves) and of the hint (24 cases) pass.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and lines as v4.5.5-eval2.11; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-eval2.11. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40629265761): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (only the code of `book_analyze.c` that finds the umigame numbers and the numbers of positions of `book moveinfo`, `book levels` and `book missing` is changed). The node counts above check that the search is the same.
- Size (against v4.5.5-eval2.11): the 5 Windows programs +0 to +2,048 bytes, the 3 DLLs the same size. Built by the release workflow: Linux executables +4,096 bytes and libraries +0 to +4,096 bytes, macOS executables +0 bytes and library +32,896 bytes, Android executables +1,488 and +832 bytes and libraries +1,520 and +864 bytes, Windows ARM64 +1,024 bytes.

## v4.5.5-eval2.11

Checks:

- The tests of the stages taken over and of the hint (the tests of nikque.23): for 502,470 moves of 200 random books, the stages taken over are those walked from each position alone; the same on one test book (1,547 positions); the 24 cases of the hint pass.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the 14 tests of damaged books and wrong words pass; so do the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs of positions, the background thread, the lines of `book bestlines` and `book findline`).
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and lines as v4.5.5-eval2.10; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. Suite F of `book-reproducible` gives the same books as v4.5.5-eval2.10. Linux (WSL, gcc 11.4, built as the release workflow does): the 4 programs give the same rows, the 3 libraries 204 API checks, 0 failures. Android (NDK r27d): no warning. Release CI (run 40423804909): the Linux and macOS libraries pass 204 API checks with 0 failures, and fforum-1-19 on macOS gives the same node counts as before (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs and DLLs are the local PGO builds. No program or library holds a folder of the build machine.
- The warning of `book_analyze.c` in the gcc 11.4 `x86-64-v4` build (a write that might go beyond an array, a false alarm: the number of ranges is at most 8 when the command is read) is gone with one line that checks the number of the range (it works the same).
- The search, the evaluation, the book learning and the file format are not changed (the code changed finds the precision stages of `book moveinfo`, and writes the warning of an unknown command). The node counts above check that the search is the same.
- Size (against v4.5.5-eval2.10): the 5 Windows programs +1,536 to +3,584 bytes, the 3 DLLs +512 bytes. Built by the release workflow: Linux programs +8,192 to +16,384 bytes and libraries +8,192 to +12,288 bytes, macOS programs +8,200 and +16,528 bytes and library +0 bytes, Android programs +2,520 and +4,368 bytes and libraries +2,352 and +4,368 bytes, Windows ARM64 +4,096 bytes.

## v4.5.5-eval2.10

Checks:

- The tests of `book reduce` and `book reducecount` (the tests of nikque.22) all agree, 546 cases (with 4 threads and 1 thread, with the count first).
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the tests of damaged books and wrong words (14 cases) all pass; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and principal variations as v4.5.5-eval2.9; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. The books of suite F of `book-reproducible` are those of v4.5.5-eval2.9. The 4 Linux builds (WSL, gcc 11.4) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Android (NDK r27d): no warning. Release workflow (run 40014781527): the API checks of the Linux and macOS libraries, 204, 0 failures; fforum-1-19 on macOS with the same node counts as v4.5.5-eval2.9 (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs are the local PGO builds. No program or library holds a folder of the build machine.
- The search, the evaluation, the book learning and the file format are not changed (the code changed runs only when `book reduce` or `book reducecount` is typed, or reads their settings); the node counts above show that the search is the same.
- Size (against v4.5.5-eval2.9): the 5 Windows programs +4,096 to +5,120 bytes, the 3 DLLs +1,024 to +1,536 bytes. Built by the release workflow: Linux programs +4,096 to +8,192 bytes and libraries +8,224 to +12,288 bytes, macOS programs +0 and +8,200 bytes and library +80 bytes, Android programs +3,576 and +6,104 bytes and libraries +3,672 and +6,248 bytes, Windows ARM64 +6,144 bytes.

## v4.5.5-eval2.9

Checks:

- The tests of `book reduce` and `book reducecount` (the tests of nikque.21) all agree, 546 cases, with 4 threads and with 1 thread. The tests of a damaged book and of wrong words (the 14 cases of nikque.21) all pass.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) and the tests of damaged books and wrong words (14 cases) all pass; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and principal variations as v4.5.5-eval2.8; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. The books of suite F of `book-reproducible` are those of v4.5.5-eval2.8. The 4 Linux builds (WSL, gcc 11.4) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Release workflow (run 39950151867): the API checks of the Linux and macOS libraries, 204, 0 failures; fforum-1-19 on macOS with the same node counts as v4.5.5-eval2.8 (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs are the local PGO builds. No program or library holds a folder of the build machine.
- Size (against v4.5.5-eval2.8): the 5 Windows programs +4,096 to +6,144 bytes; the 3 DLLs the same size. Built by the release workflow: Linux programs +8,192 bytes and libraries +8,192 to +12,288 bytes, macOS programs +8,200 and +16,512 bytes and library +32,896 bytes, Android programs +4,112 and +7,104 bytes and libraries +4,128 and +7,104 bytes, Windows ARM64 +6,144 bytes.

## v4.5.5-eval2.8

Checks:

- The tests of `book reduce` and `book reducecount` (the tests of nikque.20: 3 small books made for the tests, compared with a computation written apart) all agree, 529 cases.
- With the released `wEdax-x86-64-v3.exe`: the `book reduce` tests (500 cases on 2 small books) all agree; the earlier tests of the analysis commands (3 small books, the real `book deviate` family, 40,000 random graphs, the background thread, the lines of `book bestlines` and `book findline`) find no error.
- Search: the 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and principal variations as v4.5.5-eval2.7; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. The books of suite F of `book-reproducible` are those of v4.5.5-eval2.7. The 4 Linux builds (WSL, gcc 11.4) give the same rows, and the 3 libraries pass 204 API checks, 0 failures. Release workflow (run 39887059716): the API checks of the Linux and macOS libraries, 204, 0 failures; fforum-1-19 on macOS with the same node counts as v4.5.5-eval2.7 (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs are the local PGO builds. No program or library holds a folder of the build machine.
- Size (against v4.5.5-eval2.7): the 5 Windows programs +3,072 to +5,120 bytes; the 3 DLLs the same size. Built by the release workflow: Linux programs +8,192 to +12,288 bytes and libraries +4,096 to +8,192 bytes, macOS programs +16 and +12,288 bytes and library +32,960 bytes, Android programs +3,120 and +7,136 bytes and libraries +3,216 and +7,216 bytes, Windows ARM64 +5,632 bytes.

## v4.5.5-eval2.7

Checks:

- The tests of the analysis commands (the same as for nikque.19: the comparisons on 3 small books, 164 runs of the real `book deviate`, 40,000 random graphs of positions, the background thread) all pass.
- Search: 17 positions with 1 thread, level 14 and no book give the same depths, scores, node counts (7,676,900 in all) and principal variations as v4.5.5-eval2.6. The 5 Windows release programs (PGO) solve fforum-20-39 with 1 thread with the same results, node counts (78,733,223) and principal variations as v4.5.5-eval2.6; the 7 Cassio cases pass. The 3 DLLs: 204 API checks, 0 failures. The books of suite F of `book-reproducible` are those of v4.5.5-eval2.6. Release workflow (run 39501777572): the API checks of the Linux and macOS libraries, 204, 0 failures; fforum-1-19 on macOS with the same node counts as v4.5.5-eval2.6 (x64 2,192,396, arm64 2,196,096). The Windows x86 and x64 programs are the local PGO builds. No program or library holds a folder of the build machine.
- Size (against v4.5.5-eval2.6): the 5 Windows programs +40,448 to +41,472 bytes; the 3 DLLs the same size. Built by the release workflow: Linux programs +73,728 to +90,112 bytes and libraries +73,760 to +86,048 bytes, macOS programs +66,176 and +73,856 bytes and library +99,184 bytes, Android programs +31,528 and +52,152 bytes and libraries +31,936 and +52,968 bytes, Windows ARM64 +48,640 bytes.

## v4.5.5-eval2.6

Checks (one logical CPU):

- fforum-20-39 with 1 thread: the 5 Windows programs give the same depths, moves, scores, node counts (78,733,223) and principal variations as those of v4.5.5-eval2.5. Cassio: 7 cases. The 3 DLLs: 204 API checks, 0 failures. `book-reproducible`: the books of suite F are those of v4.5.5-eval2.5.
- Linux (WSL, gcc 11.4, built as the release binaries workflow does): the 4 programs give the same rows as the Windows programs of v4.5.5-eval2.5 (78,733,223 nodes); the 3 libraries: 204 API checks, 0 failures. No build folder in them.
- Speed (CPU cycles, 12 pairs in alternating order, against v4.5.5-eval2.5): 0.995 to 1.002. The first build of `wEdax-x86-sse.exe` gave 1.0025 (median 1.0009), so it was built again (0.9998); the source of eval2.5 built again gives 1.0006: the speed of a PGO build changes by a few tenths of a percent from a build to the next.
- Speed with 32 threads (2026-10-19, the released programs, ratio of the times to v4.5.5-eval2.5, mean of 6 alternating rounds): `wEdax-x86-64-v4.exe` suite F 1.004, `book learn` 1.003, `book fill 2` 0.990, `book fill2 2` 0.992, `book leaf expand` 1.001, `book fill2 2 5 5` 0.986, `book import` 0.989 (each within the standard deviation or faster). **`wEdax-x86-sse.exe` tends to be slower**: `book learn` 1.007 and `book leaf expand` 1.013 (5 of the 6 rounds slower each), the other commands 0.997 to 1.007; one-thread `book leaf expand` (one logical CPU, CPU cycles, 12 pairs) 1.058 (all 12 slower; the same number of positions). Presumably the difference of the PGO builds (not checked). The same peak memory (±1 MiB; also in two one-thread test suites of book commands).
- Size: -1,024 to +512 bytes.

## v4.5.5-eval2.5

Checks (2026-10-16):

- The 5 Windows release executables (PGO): with the `eval.dat` of eval2, one-thread `-solve` of fforum-20-39 gives the same depths, scores, node counts (78,733,223 in all) and principal variations as the same executables of v4.5.5-eval2.4. The 7 Cassio cases pass.
- One-thread `-solve` of fforum-1-19: 2,192,396 nodes in all, as eval2.4.
- The 3 DLLs: 204 API checks, 0 failures, with the `eval.dat` of eval2.
- With `book-reproducible on`, the `book deviate` family (suite F) gives the same books (except the save date) with 32 threads and with 1 thread.
- The seven jobs of the release CI pass (the Linux job tests the library).

Not checked:

- The speed and strength of this version (the evaluation function is that of eval2.1); the full book regression set for eval2; the real 29 GB book; Android, Windows ARM64 and macOS on devices.

## v4.5.5-eval2.4

(No test or measurement record for this version.)

## v4.5.5-eval2.3

(No test or measurement record for this version.)

## v4.5.5-eval2.2

(No test or measurement record for this version.)

## v4.5.5-eval2.1

### Strength (measured)

The same executable played against itself with only `eval.dat` changed, the new file against the upstream one (1 thread, same level, no book; the method is the one of "How strength was measured" under v4.5.5-nikque.5 below: two games with colors swapped from each balanced start position). Elo is given for the new `eval.dat`, with its 95% confidence interval.

| Level | Start positions | Games | Wins − draws − losses | Elo | Nodes per move (ratio) |
|---|---|---|---|---|---|
| 6 | 8 moves, 1102 positions | 2204 | 1093 − 158 − 953 | **+22.1** [+8.5, +35.7] | 0.99 |
| 10 | same | 2204 | 1032 − 259 − 913 | **+18.8** [+6.1, +31.5] | 0.99 |
| 18 | same | 2204 | 970 − 372 − 862 | **+17.0** [+6.6, +27.5] | 0.91 |
| 18 | 10 moves, 1087 positions | 2174 | 969 − 361 − 844 | **+20.0** [+9.5, +30.5] | 0.92 |
| 21 | 8 moves, 1102 positions | 2204 | 941 − 432 − 831 | **+17.4** [+7.9, +26.8] | 0.88 |

- The matches were played with a build of the v4.5.5-nikque.8 code plus this change (no PGO, AVX-512). Single-thread searches give the same results in v4.5.5-nikque.8 and v4.5.5-nikque.9, so this version plays the same games.
- The accuracy of the level 18 scores was measured too (see "Accuracy").
- **Not measured**: matches at level 24 or more, matches with several threads, matches with a time per game, matches against other programs.

### Speed (measured)

Ryzen 9 9950X. **Release executables (PGO) against release executables**: the **time** of this version with the new `eval.dat`, divided by the time of the v4.5.5-nikque.9 release executable with the upstream `eval.dat` (less than 1 is faster). Each is the time `-solve` takes for the same set of positions; the runs alternated 3 to 7 times and the shortest times are compared. Levels 10 and 18 use 89 positions, levels 21 and 24 use 30 midgame positions, exact solving uses `problem/fforum-20-39.obf`.

1 thread:

| Executable | Level 10 | Level 18 | Level 21 | Exact (fforum 20-39) |
|---|---|---|---|---|
| `wEdax-x86-64-v4.exe` (AVX-512) | **1.055** | 0.945 | 0.877 | **1.026** |
| `wEdax-x86-64-v3.exe` (AVX2) | **1.045** | 0.943 | 0.878 | **1.036** |
| `wEdax-x86-64.exe` (baseline) | **1.135** | 0.994 | 0.916 | **1.029** |
| `wEdax-x86-sse.exe` (32-bit, SSE2) | **1.204** | **1.037** | 0.949 | 0.97 (±0.05) |
| `wEdax-x86.exe` (32-bit) | **1.239** | **1.070** | 0.973 | **1.034** |
| Node count ratio (the same for every executable) | 0.984 | 0.889 | 0.835 | 1.030 |

- Each call of the evaluation function now counts the legal moves twice, which lowers the nodes per second (by 4 to 7% with the AVX-512 build, 9 to 13% with the baseline 64-bit build, 12 to 20% with the 32-bit builds). On the other hand, at level 18 or more the search visits 11 to 20% fewer nodes. The table shows the balance of the two. A shallow search (level 10) visits almost as many nodes, so it is slower.
- Exact solving with the 32-bit builds is a short measurement (2 to 3 seconds) that varies much (the x86-sse run came out shorter although it visits 3% more nodes).

Other conditions with `wEdax-x86-64-v4.exe`:

| Condition | Time ratio | Node count ratio |
|---|---|---|
| Level 18, 8 threads | 0.977 | 0.877 |
| Level 18, 32 threads | 0.958 | 0.842 |
| Level 21, 8 threads | 0.907 | 0.813 |
| Level 21, 32 threads | 0.931 | 0.916 |
| Level 24, 1 thread (1 run) | 0.847 | 0.795 |
| Level 24, 32 threads | 0.902 | 0.818 |
| Exact, fforum 40-59, 1 thread (1 run, 279.6 s → 290.1 s) | **1.038** | 1.055 |
| Exact, fforum 40-59, 32 threads (22.5 s → 23.8 s) | **1.056** | 1.094 |

- The node counts of multi-thread searches vary from a run to the next; these are ratios of the shortest of 3 to 5 runs.
- Position by position, some of the 30 positions at level 24 take more nodes (13 of the 30 in a check with a build without PGO; 0.80 times in total).
- Exact solving uses the evaluation function only to order the moves, but the order changes and the node counts grow by 3 to 9%. The scores (disc differences) are the same for every problem.

**The upstream `eval.dat` with the executables of this version** (same results and node counts as v4.5.5-nikque.9; time against the v4.5.5-nikque.9 release executable, level 10 / 18 / 21 / exact): AVX-512 1.013 / 1.008 / 0.997 / 1.000, AVX2 0.998 / 1.004 / 1.004 / 1.011, baseline 64-bit 1.005 / 1.000 / 1.006 / 1.000, x86-sse 1.003 / 1.000 / 0.994 / (varies too much), x86 0.981 / 0.988 / 0.986 / 0.993. The differences stay within ±2%; without the tables the legal moves are not counted. Between two builds made the same way without PGO (AVX-512) the ratios were 1.011, 1.017 and 1.009 at levels 10, 18 and 21, and 1.000 for exact solving.

**Linux executables** (gcc 11.4, 1 thread, 1 run each; time with the new `eval.dat` divided by the time with the upstream `eval.dat`, same executable; level 18 / level 21 / exact fforum 20-39): x86-64 0.995 / 0.915 / **1.026**, x86-64-v3 0.952 / 0.871 / **1.030**, x86-64-v4 0.943 / 0.881 / **1.033**, 32-bit (`lEdax-x86`) **1.028** / 0.938 / **1.050**: the same pattern as on Windows. With the upstream `eval.dat`, the results and node counts are those of the Linux executables of v4.5.5-nikque.9, in 0.99 to 1.01 times the time (runs of 1 to 2 seconds). Level 10 was not measured on Linux.

**Peak memory** is unchanged (release executables, 1 run each: 462.7 MB → 462.9 MB at level 21 with 32 threads, 135.2 MB → 135.2 MB at level 18 with 1 thread; the added tables take about 8 KB).

### Slower conditions (summary)

Conditions where this version (new `eval.dat`) is slower than the release executables of v4.5.5-nikque.9 (upstream `eval.dat`):

| Condition | Time |
|---|---|
| Level 10 (shallow search), AVX-512 and AVX2 builds | 4.5 to 5.5% longer |
| Level 10, baseline 64-bit build (`wEdax-x86-64.exe`) | 13.5% longer |
| Level 10, 32-bit builds | 20 to 24% longer |
| Level 18, 32-bit builds | 4 to 7% longer |
| Exact solving, 1 thread | 3 to 4% longer (3 to 5.5% more nodes) |
| Exact solving, 32 threads | about 6% longer (9% more nodes) |

- The speed of levels below 10 was not measured (level 6 was only played; its node counts are almost the same, so it should be as slow as level 10 or slower).
- Level 21 with the 32-bit builds is a little faster (0.95 to 0.97), and every 64-bit build is faster at level 18 or more.
- v4.5.5-nikque.9 is the faster one if you mostly solve endgames, play or learn large numbers of games at shallow levels, or use a 32-bit build.
- The time of book learning (`book learn`, `book deviate`, `book fix`...) was not measured. It is made of searches at the level of the book, so it should follow the ratios above; but even in a level 18 book, the searches of the endgame positions (exact solving) get slower.

### Accuracy

- The level 18 scores were compared with the exact scores (12 sets with different numbers of empty squares, 2850 positions, 1 thread). Mean absolute error: 1.089 discs with the upstream `eval.dat`, 1.067 with the new one; root mean square error: 2.02 and 1.94. The search visits fewer nodes and the scores are a little more accurate.
- The settings of the search pruning (ProbCut) are unchanged. The difference between shallow and deep searches was measured again with the new weights (1200 positions, depths 0 to 16): 0.93 to 1.00 times the one of the upstream weights with 32 empty squares or more, 1.18 to 1.24 times with 24 to 31 empties at depths 12 to 16. Scaling the error estimate by 1.1 and 1.2 was tried: the gain in accuracy (mean error 1.01 and 0.97 discs) was smaller than the loss of speed (1.16 and 1.47 times the time at level 18), so it was not taken. (The two tests of this paragraph were run before the final weights were chosen, with weights pulled less strongly toward the upstream ones.)

### Checks

- **Same behaviour as v4.5.5-nikque.9 with the upstream `eval.dat`**: with the five Windows release executables, the single-thread node counts at levels 10, 18 and 21 and of exact solving (fforum 20-39) are those of the v4.5.5-nikque.9 release executables.
- **New `eval.dat`**: the five Windows executables give the same node counts in these four conditions. The exact scores of fforum 20-39 and 40-59 are the same as with the upstream `eval.dat` for every problem.
- **The four Linux executables**: with the upstream `eval.dat`, the results and node counts of exact solving and of level 18 are those of the v4.5.5-nikque.9 release executables. With the new `eval.dat`, the four executables give the same results and node counts at levels 18 and 21 and for exact solving, and the node counts are those of the Windows executables.
- **libedax**: the 191 api checks pass with the new `eval.dat` (3 Windows libraries, 3 Linux libraries).
- **Book regression tests** (upstream `eval.dat`, 1 thread, level 21, `book-store-tasks = 1`, the AVX-512 release executable): in the 8 suites that cover all the book commands, the 84 files produced are identical to those of the v4.5.5-nikque.9 release executable (only the file that holds a date differs). The 8 suites took 172.1 s → 174.1 s in total (1.012 times; single runs, not distinguishable from noise). No book test was run with the new `eval.dat`, nor with several threads.
- Peak memory (see "Speed").
- **Not measured or not checked**: the macOS, Android and Windows ARM64 builds (running and speed; the Android and Windows ARM64 builds were only built), the time of book learning, matches at level 24 or more, searches at level 30 or more, book tests with the new `eval.dat` (the book code is the one of v4.5.5-nikque.9), edax_runner with the library of this version.
