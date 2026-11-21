# 9. Large books

[Contents](README.md) | Previous: [8. Maintaining a book](08-book-maintenance.md) | Next: [10. Game and problem files](10-files.md)

This fixed version is designed so that a book of tens to hundreds of millions of positions can be grown and maintained on a single PC. This chapter covers rough memory, disk and time requirements for such books, the recommended settings, and the precautions to take.

The figures are measurements from the test records (`docs/validation/`). **Each figure comes from one PC and one book; it is not a prediction for other environments.** Unless stated otherwise, the measurements were taken under Windows on a Ryzen 9 9950X (32 logical CPUs), with a book of level 18 and depth 40 (about 660 million positions, a file of about 29 GB).

## 9.1 Memory

| What | Size |
|---|---|
| The book (in memory) | About 50 bytes per position, links included. 30.6 GiB for 657 million positions |
| The book file | About 44 bytes per position (29.16 GB for 661.62 million positions) |
| The hash table of the search | The table of [5.6](05-search.md). 226 MB with `auto` and 32 threads |
| The hash tables of the simultaneous searches (`book-expand-tasks`, `book-store-tasks`) | One set per search. 57 MB each for the 2-thread searches of `auto` (about 0.9 GB for 16 of them). 14 MB each for 1-thread searches up to level 18 |
| The `book deviate` family, only while it selects positions | 1 byte per position (about 0.9 GB for 657 million positions) |

- With the book of about 29 GB, peak memory was 33.4 GB for `book deviate2 5 5` and 31.8 GB for `book subtree`. From these two values, a rule of thumb: **the size of the book file + 4 to 5 GB**.
- **`book load` uses the memory of two books while it reads**, so that the current book can be kept if the load fails.
  - For a large book, do not use `book load`. Instead, **set it in `book-file` and let Edax load it at startup**.
  - Loading at startup and `book merge` do not need this extra memory.
- When memory runs short:
  - If the simultaneous searches cannot be set up, a warning is printed and the work continues with fewer of them (`not enough memory to search the positions at the same time` and similar messages).
  - When no more positions can be added to the book, the growing command stops.
- The 32-bit version cannot load a book of about 89.47 million positions or more. It prints an error and keeps the current book. Use a 64-bit version for a large book.
- A book can hold at most 4,294,967,295 positions. A book with more than 2,147,483,647 positions cannot be loaded by versions before v4.5.5-nikque.4 or by the original Edax.

## 9.2 Disk

- **Each save needs free space equal to the size of the book.** Edax first writes the whole book under another name and then swaps it with the original file ([6.6](06-book-basics.md)). For a 29 GB book, 29 GB must be free during the save.
- **Every intermediate file is a full copy of the book.** Files such as `book.dat.dev2`, `book.dat.store` and `book.dat.mrg` appear next to `book.dat`, and each is as large as the book. Edax does not delete them: delete the ones you no longer need yourself.
- Rule of thumb: free space of at least 3 times the book file (the book, one intermediate file, and the temporary file of a save).
- On Windows, a temporary file left by a forced stop (`book.dat.tmp.number`) is deleted by Edax at the next save.
- **On Windows, keep the full path of `book-file` (folder included) within 240 characters** ([chapter 13](13-troubleshooting.md)).
- Save time: 18 to 28 seconds for the book of about 29 GB (it depends on the disk speed).

## 9.3 Example times

| Operation | Time (book of about 660 million positions) | Version measured |
|---|---|---|
| Loading at startup | 28 to 49 seconds | v4.5.5-nikque.3 to 12 |
| Saving | 18 to 28 seconds | v4.5.5-nikque.3 |
| One negamax | 18 to 34 seconds | v4.5.5-nikque.10, 12 |
| Selecting what to expand, for `book deviate2` and `book deviate3` | 21 to 23 seconds | v4.5.5-nikque.3 |
| A whole `book fix` (from loading to saving) | 222 to 242 seconds | v4.5.5-nikque.10 |
| `book merge` (taking in a book of 6.49 million positions; the whole) | 218 to 224 seconds | v4.5.5-nikque.10 |
| `book merge` (two books of 657 million positions; 1.736 million positions added) | 263 seconds, peak memory 37.0 GiB | v4.5.5-nikque.3 |
| `book depth 39` + `book subtree` (depth 40 → 39; from loading to the end) | 177 seconds | v4.5.5-nikque.12 |

- The same operation varied by 10 to 20% depending on the day of measurement and on other running programs.
- With v4.5.5-nikque.2 (the original Edax with bug fixes only), the same book took:
  - 446 seconds to load,
  - 657 seconds to save,
  - 937 seconds for a negamax,
  - four and a half hours to merge two books of 600 million positions

  (from the section on v4.5.5-nikque.3 in the test records).

**Growing time is almost entirely search time.**

- `book deviate2 5 5` on the book of about 660 million positions had 2.75 million leaves to expand in one round (`todo`).
- With 32 threads and `book-expand-tasks = auto`, 32 to 40 leaves were expanded per second (it varied with the time of day).
- At that speed, one round would take about 19 to 24 hours. This is a calculation, not a value measured over a whole round.

## 9.4 Settings

The `config.ini` in the package has values chosen for growing a large book. An example for growing a level-18 book on a PC with 32 logical CPUs:

```
level = 18
n-tasks = 32
hash-table-size = auto
book-file = data/book.dat
book-expand-tasks = auto
book-store-tasks = auto
book-save-interval = 360
book-deviate-save-rounds = 1
book-merge-auto-save = on
```

| Setting | How to choose it |
|---|---|
| `n-tasks` | `auto` to use all the logical CPUs. **Lower it if you also do other work** (half of them, for example). Negamax, the selection of what to expand and the linking also run in parallel on this number of threads |
| `book-expand-tasks` | `auto`. Up to level 18 this gives "`n-tasks ÷ 2` searches of 2 threads", and in rounds with many leaves (32 times `n-tasks` or more) "`n-tasks` searches of 1 thread". At levels 19 to 24 each search has 4 threads, above that 8. At high levels, set a number and compare the speeds (the test records suggest starting from 4 or 8 with 32 threads) |
| `book-store-tasks` | `auto` (`n-tasks` games learned at the same time) |
| `hash-table-size` | `auto`. A larger table does not make short searches faster |
| `book-save-interval` | Keep in mind that one save takes 18 to 28 seconds and writes a file as large as the book. The package uses 360 minutes (6 hours) |
| `book-deviate-save-rounds` | `1` if one round is long (hours). If a round is short (minutes), writing 29 GB every round is wasteful, so make it larger |

**Settings that reproduce the behaviour of earlier versions and of the original Edax**: `book-expand-tasks = 1`, `book-store-tasks = 1`. A book made with `auto` differs from one made with `1` in the move and score of some leaves ([7.2](07-book-learning.md), [7.6](07-book-learning.md); the test records give the measured number of positions that differed).

## 9.5 Precautions for working on a large book

1. **Copy the file before maintenance.** The following commands change the book substantially: `book subtree`, `book prune`, `book reduce`, `book merge`, `book correct`, `book leaf recalculate`, `book fill`, `book fill2` and `book leaf expand`. The save at the end overwrites `book-file` ([8.4](08-book-maintenance.md)).
2. **Do not enter `book new` while a large book is in use.** The book in memory is discarded, and getting it back means loading the file again, which takes time. If Edax is then quit without further action, the `book-file` file becomes the new empty book. The previous file is kept under a name with `.old` appended (up to v4.5.5-nikque.12 it is overwritten, not kept; [6.7](06-book-basics.md)).
3. **`book export` (text file) produces a very large file for a large book.** For everyday saving and for sharing a book, use the files written by `book save`.
4. **Before stopping, consider the save interval.** Stopping a growing command loses everything since the last save ([7.5](07-book-learning.md)). `quit` waits until the command ends.
5. **Before quitting, check which file is the latest.**
   - If you `quit` after a growing command has ended, `book.dat` is the latest.
   - If you stopped the command partway, `book.dat.dev2` or a similar file is probably newer.

   Check the dates and sizes before renaming anything.
6. **Do not open and save the same book file with two Edax instances at the same time.** The one that saves last overwrites the other. To grow in parallel, use separate files and combine them with `book merge` afterwards.
7. **Try the steps on a small book before running them on a large one.** On a book that takes seconds, you can check the order of the commands, the file names and where things are saved, and so avoid mistakes.

## 9.6 Book size, depth and level

- A higher level makes adding each position take longer. In the recorded measurement (level 18, `book deviate2 5 5` on the book of about 660 million positions), expanding one leaf took a search of about 33 million nodes.
- Each additional move of depth adds a very large number of positions. In the recorded example, reducing the depth of the same book from 40 to 39 reduced it from 661.62 million positions to 524.08 million (the deepest layer alone held 137.54 million positions).
- Increasing either number of `book deviate2 X Y` by one multiplies the work several times. Always try small numbers first.

Next: [10. Game and problem files](10-files.md)
