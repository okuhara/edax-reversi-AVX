# Regression harness

The harness covers the 17 audit cases, legal-move and exact endgame checks, Cassio, and event handling. Build it with `tests\build-regression.cmd` from a Visual Studio 2022 x64 Developer Command Prompt. Set `EDAX_EVAL_FILE` to the path of your `eval.dat` before running cases that search positions. Run commands from the repository root.

Examples:

```cmd
set EDAX_EVAL_FILE=C:\path\to\eval.dat
tests\regression.exe edx
tests\regression.exe truncated
tests\regression.exe enhance
tests\regression.exe deviatesave
tests\regression.exe formats
tests\regression.exe gtp
tests\regression.exe core 1
tests\regression.exe core 4
```

v4.5.5-nikque.3 adds `missinglink` (a link to a missing position is skipped by negamax and removed by `book fix`), `mergefile` (a truncated merge source leaves the destination unchanged; a complete one is merged), and `linkstore` (links move between in-place and heap storage without loss).

The final audit of v4.5.5-nikque.8 adds `mergetwice` (a second merge does not take the positions of the first one for duplicated positions; a merge that fails only removes what it added), `negamaxloop` (a link of a damaged book that leads back to its own position does not make the negamax with threads endless) and `learnline` (the first moves of a game to learn are copied without their spaces: a long line is not cut).

`reproducible` checks `book-seed` (the random numbers of a book start from the seed; `auto` and a wrong value leave them to the clock), the check of the links of a new position before its expansion is applied, and that `book-reproducible on` gives the same book (except the save date) with 4 threads, twice, and with 1 thread (`book new 2 52`, `book deviate 2 4`). It uses 4 threads.

`expandredo` searches the second of two expansions before the first, whose new position is a move of the new position of the second: when the first is applied, the second has old links, is searched again, then applied, and the book is the one of `position_expand` (the first, then the second).

Additional case names are listed in `tests/regression.c`. Some cases write small `audit-*` files in the current directory. Run them from a disposable checkout if you want to keep the source directory clean. The cases that read `tests/merge-complete-fixture.dat` (`gtp`, `bookio`, `truncated`, `mergefile`, ...) fail with the exit code 2 when it is not in the current directory, as a case does when a file of its own cannot be opened (up to v4.5.5-nikque.26 they crashed).

To check that `book correct`, `book leaf recalculate`, `book deepen` and `book learn` go on from a book saved before they ended (the progress marks, removed when the command ends; the level given by `book deepen`; the progress file `<book-file>.learn`), run `python tests/book-resume.py bin/wEdax-x86-64-v4.exe bin/data/eval.dat` (one thread, a few seconds; `--old <exe>`: a version without marks reads a book with marks as a damaged book).

To check Cassio's input thread, including EOF immediately after a command with
no final newline, run `python tests/cassio-eof.py bin/wEdax-x86-64-v4.exe`.
Use `--cwd bin` if the executable is elsewhere and `bin/data/eval.dat` is used.
The same test accepts a Linux executable and needs only Python's standard library.

From a native Visual Studio 2022 x86 Developer Command Prompt, run
`tests\build-regression-x86-avx2.cmd`, then set `EDAX_EVAL_FILE` and run
`tests\regression-x86-avx2.exe core 1`. Keep this build's `/O2 /GL` options:
MSVC's x86 optimization of the inlined CRC hash could corrupt a later AVX2
legal-move calculation, causing wrong scores or repeated passes until stack
overflow. The `core` case compares 96 exact searches with an independent solver.
