# 6. Book basics

[Contents](README.md) | Previous: [5. Search basics](05-search.md) | Next: [7. Growing a book](07-book-learning.md)

## 6.1 What a book is

A **book** (opening book) is data that stores opening and midgame positions together with their scores. When the current position is in the book, Edax **plays the book move without searching**. A book has these advantages:

- Opening moves are based on a much deeper search (done when the book was built) than a search during a game.
- No time is spent on the opening moves.
- You can see the score of each position at once, which is useful for study.

A book is usually a single file (`bin/data/book.dat` in the package). The `book.dat` in the package is almost empty: **it contains only the initial position**. You can grow it yourself ([chapter 7](07-book-learning.md)), or replace it with an Edax book made by someone else (Edax 4.4 and 4.5 format).

All book commands start with `book` (or `b` for short). The word after `book` may be written in upper case (`Book Info`).

## 6.2 Inside a book: positions, links and leaves

A book is **a set of positions**. For each position, it records:

| Item | Meaning |
|---|---|
| Board | The discs. The 8 shapes obtained by rotating and mirroring are kept as one position |
| Level | The level at which the position was searched ([chapter 5](05-search.md)) |
| Score | The score of the position (the expected disc difference, seen from the side to move), with its lower and upper bounds |
| **Link** | A move that "leads to another position of the book", and its score |
| **Leaf** | **The one best move** among those that are not links, and its score (a value found by a search) |
| Game counts | The number of lines through this position (won, drawn, lost, not finished) |

The key concept is **the difference between a link and a leaf**.

- **A link move**: the resulting position is also in the book. Its score reflects the deeper analysis below it.
- **A leaf move**: the resulting position is not in the book yet. It marks the move that looks best among the remaining moves, although nothing beyond it has been examined.
- **Growing a book means turning promising leaves into links**, that is, adding the positions after them to the book.
- A move that is neither a link nor a leaf is not recorded in the book. The search judged it worse than the leaf move.

The score of a position is the best of the scores of its links and its leaf. When the score of a lower position changes, the scores of the positions above it change too.

Recomputing the scores from the bottom up is called **negamax**. The commands that change the book run negamax automatically when they finish.

## 6.3 Looking into the book

### book show: the current position

This shows how the current board position is recorded in the book.

```
>book show

Level: 8
Best score: +0 [-2, +2]
Moves: [d3:+0] <f4:-3>
       152 incomplete lines.
```

| Line | Meaning |
|---|---|
| `Level: 8` | The position was searched at level 8 |
| `Best score: +0 [-2, +2]` | The score is +0. `[lower, upper]` is a margin that allows for the error of the search |
| `Moves:` | The recorded moves. **`[move:score]` is a link, `<move:score>` a leaf.** Best first |
| `152 incomplete lines.` | 152 lines from this position do not reach the end of the game |

- The case of a move shows the side: Black's moves are upper case, White's are lower case.
- When some lines reach the end of the game, a line `Lines: N full games with …% win, …% draw, …% loss` is also printed.
- When the current position is not in the book, **nothing is printed**.

You can follow an opening move by move by typing `book show` as the game goes on.

### book info: the whole book

```
>book info
Edax Book 4.5; 2026-10-7 17:14:18;
Positions: 2246 (moves = 2301 links + 2243 leaves);
Level 8 : 2246 nodes
Depth: 12
Memory occupation: 1160986
Hash balance: 0 < 0 < 2
```

| Line | Meaning |
|---|---|
| First line | The version of the book format, and the date and time when the book was made (or last rebuilt) |
| `Positions` | The number of positions, of links and of leaves |
| `Level 8 : 2246 nodes` | The number of positions for each level. Usually one line only (the level of the book) |
| `Depth: 12` | The depth of the book (6.4) |
| `Memory occupation` | The size the book takes in memory (bytes) |
| `Hash balance` | How even an internal table is (of no concern) |

If the book contains positions whose level differs from the book's level, `book info` prints the first 10 of them, one per line (lines starting with `{board:…}`). If there are more, a line such as `(2245 positions have another level than the book: only the first 10 are shown)` gives their total. Up to v4.5.5-nikque.12, every such position was printed.

### book stats: detailed counts

`book stats` shows the number of positions, links and leaves for each number of empty squares, and the number of positions for each score.

```
Stage distribution:
stage    positions        links       leaves      terminal nodes
   48          880            0          880          880
   49          625          909          625            0
   50          352          639          352            0
 …
   60            1            4            0            0
```

`stage` is the number of empty squares. `terminal nodes` is the number of positions without any link, for example the positions of the deepest layer, beyond which the book holds nothing.

## 6.4 The level and the depth of a book

A book has two settings of its own.

**Level**: the level used to search positions.

- The commands that grow the book search each new position at this level.
- **Mixing books of different levels makes the reliability of the scores inconsistent**, so a book is normally grown at a single level.
- A higher level makes the scores more reliable, but adding each position takes longer.

**Depth**: how many moves into the game the book goes. A book of depth 40 covers the first 40 moves from the initial position. In detail:

- The positions up to the one where move 40 is next (positions with 21 or more empty squares) are handled by the growing commands and can have links.
- The positions after move 40 (20 empty squares) are stored as **the deepest layer**, with a leaf only. Nothing beyond them is stored.

Level and depth are usually chosen together. As the table in [chapter 5](05-search.md) shows, a level 18 search solves exactly from 21 empty squares. If the book's depth reaches the point where exact solving starts, the scores of the book's last positions are exact. When Edax creates a book itself (6.6 below), it chooses this depth from the level.

| Level of the book | Depth chosen automatically (`Depth` of `book info`) |
|---|---|
| 10 or less | 61 − level×2 (level 8 gives 45) |
| 11 to 18 | 40 |
| 19 to 24 | 37 |
| 25 to 29 | 34 |
| 30 to 35 | 31 |
| 36 to 41 | 67 − level (level 36 gives 31) |
| 42 or more | 25 |

- You can change the depth with `book depth n`. This only changes the range that is grown; no position is removed.
- To remove the positions outside the range, use `book subtree` ([chapter 8](08-book-maintenance.md)).
- A number in `book-depth` in `config.ini` sets that depth at every startup. `auto` keeps the depth saved in the book file.

## 6.5 Using the book in a game

| Input or setting | Meaning |
|---|---|
| `book on` (setting `book-usage = on`) | Play the moves of the book in a game (the setting of the package) |
| `book off` (`book-usage = off`) | Do not play the moves of the book (think every time) |
| `book randomness n` (setting `book-randomness = n`) | When choosing a move from the book, also accept moves up to n discs worse than the score of the position (0 by default) |

When Edax plays from the book, it picks **at random one of the recorded moves (links and leaf) whose score is at least "the position's score − randomness"**.

- With randomness 0, only the best moves are chosen. If several moves share the best score, one is chosen at random, so **the same position does not always produce the same move**.
- With randomness 2, moves up to 2 discs worse than the best are also played. The openings vary more, at the cost of some slightly losing moves.
- When a position is not in the book, Edax searches from that point on.

When Edax plays a book move, the `depth` column of the search table shows `book`.

```
 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
book    -2                                          F4 e3 F6 d3
------+-----+--------------+-------------+----------+----------------------

Edax plays F4
```

`hint` also shows **the book moves first** (the `book` lines) when the book is in use (`book on`) and contains the current position. The other moves are searched and shown after them. To see the search results alone, type `book off` and then `hint`.

```
>hint 4
 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
book    +0                                          D3 c4 F4 f6 F3 e6 E7 f7
book    -3                                          F4
    8   -03        0:00.000         18124            G5 c6 C5 c4 B5 e6
    8   -04        0:00.000           711            F3 d3 F4 f6 G5 e6
------+-----+--------------+-------------+----------+----------------------
```

## 6.6 The book files

### Which file is read

At startup, Edax reads the file given by the `book-file` setting (`data/book.dat` by default).

- **If the file does not exist**, Edax creates a new book in memory, containing only the initial position.
  - Its level is the `level` setting, and its depth follows the table in 6.4.
  - The screen shows something like `New book 18 21...`. The second number is the lowest number of empty squares the book covers: 21 for depth 40.
  - This new book is saved under that file name when Edax exits. So even if you do nothing, a small file containing just the initial position appears.
- **If the file exists but cannot be read** (it is damaged, cut short, and so on), Edax also starts with a new book.
  - In this case, nothing is saved on exit unless the book was changed.
  - **The unreadable file is not overwritten.** If something later has to be saved under that name, the original file is first renamed to `book.dat.damaged` (`.damaged.1`, … if that name is taken) and kept.
- **If the file exists but cannot be opened** (another program has it open without sharing, there is no permission to read it, and so on), Edax waits about 2 seconds and tries again. If it still cannot be opened, Edax starts with a new book, but the file is not taken for a damaged one.
  - The screen shows `Cannot open opening book data/book.dat; it is not loaded, and no save replaces it`.
  - **This book is never saved to that file** (neither on exit nor by `book save`), and no `.damaged` file is made. The original file is left as it is.
  - Once the other program has closed it, `book load data/book.dat` loads it as usual. Restarting Edax works too.
  - To keep what was learned meanwhile, save it under another name, such as `book save data/other.dat`.
  - Up to v4.5.5-nikque.26, a file that could not be opened was taken for a damaged one: a later save moved it to `.damaged` and replaced it with the new book.

To switch to another book, do one of these:

- Write `book-file = data/my.dat` in `config.ini` and restart.
- Give `-book-file data/my.dat` at startup.
- Type `book load data/my.dat` while running (see the notes below).

### How saving works

| When | Where |
|---|---|
| When you type `book save file` | That file |
| When you type `book save` alone (no name) | The file of `book-file` |
| When Edax ends (`quit`). **Only if the book was changed and not saved yet** | The file of `book-file` |
| During and at the end of the commands that grow or maintain the book | Files named `book-file` plus an extension (table below) |

**Note 1: `book save` without a file name saves to the `book-file` file.** The file name is shown on screen.

```
>book save
Book saved to data/book.dat
```

This is the behaviour since v4.5.5-nikque.13. Up to v4.5.5-nikque.12, omitting the name gave the error `Cannot save book to ; existing book was not replaced`, and nothing was saved. With an older version, always give the name, as in `book save data/book.dat`.

**Note 2: a book read with `book load` is automatically saved to `book-file`, not to the file it was read from.**

For example: start with `data/book.dat`, type `book load data/other.dat`, grow the book and quit. The grown book is saved to `data/book.dat`, and `other.dat` is left unchanged.

When this happens, **the previous `data/book.dat` is kept as `data/book.dat.old`** (since v4.5.5-nikque.13; the same mechanism as after `book new`: [6.7](06-book-basics.md)). The same applies when Edax exits after `book import` of a text file.

```
WARNING: data/book.dat holds the book in use before "book load": it is kept as data/book.dat.old
```

- To save to the file that was read, type `book save data/other.dat` yourself. If you always use the same book, it is safer to set it with `book-file`.
- **A file named `book-file` plus an extension** (`data/book.dat.dev2`, `.store`, `.mrg`, …: the files the book commands save automatically) is treated as a continuation of the same book.
  - After you load such a file, `book-file` is overwritten as before, without keeping a `.old` file.
  - This way, resuming from an intermediate file does not leave yet another copy of a large book each time.
  - After loading `book.dat.old` or `book.dat.damaged`, the previous file is kept.
- If Edax exits without changes to the loaded book, nothing is saved.
- Up to v4.5.5-nikque.12, the previous `book-file` file is not kept: it is overwritten.

**Note 3: saving needs about as much free disk space as the size of the book.** Edax first writes the whole book under another name (`file.tmp.number`), then replaces the original file with it once it is complete. If the save fails partway, the original file remains.

### Files that the commands save automatically

The commands that grow or maintain the book save their progress and their result under the `book-file` name plus an extension (`data/book.dat.dev2` and so on, when `book-file` is `data/book.dat`). **These saves do not overwrite the original `book.dat`.**

| Extension | Written by |
|---|---|
| `.store` | `book store`, `book learn` (not written with the setting `book-store-auto-save = off`) |
| `.learn` | `book learn` (the lines learned: a small text file, removed when the whole file is learned) |
| `.gam` | `book add` |
| `.dev`, `.dev2`, `.dev3` | `book deviate`, `book deviate2`, `book deviate3` |
| `.enh` | `book enhance` |
| `.fill`, `.fill2` | `book fill`, `book fill2` |
| `.play` | `book play` |
| `.leaf`, `.leaf2`, `.leaf3`, `.leaf4` | `book leaf recalculate`, `2`, `3`, `4` |
| `.lexp` | `book leaf expand` |
| `.mrg` | `book merge` (not written with the setting `book-merge-auto-save = off`) |
| `.err` | `book correct` |
| `.dep` | `book deepen` |

- These are ordinary book files. `book load data/book.dat.dev2` reads one, and if you rename it to `book.dat`, you can use it directly.
- When a growing command has finished normally and you quit Edax without `book save`, the grown book is also saved to `book-file` (`data/book.dat`). So in normal use, **`book.dat` is the latest version**. The files with extensions are a backup in case the command is interrupted.
- **On Windows, keep the full `book-file` path, including the folder, within 240 characters.** If it is longer, the files with extensions cannot be saved ([chapter 13](13-troubleshooting.md)).

## 6.7 Making a new book

```
>book new 18 40
New book 18 21......done>
```

This creates in memory a book of level 18 and depth 40 that contains only the initial position.

- The current book is discarded from memory. If you changed it and have not saved it yet, save it first with `book save`.
- Without the numbers, the level is 21 and the depth 36.

The new book is not a file yet. Save it with `book save data/my.dat`, or quit Edax, which saves it to `book-file`.

### Saving after book new, and the previous book

After `book new`, a **save without a file name** (quitting Edax, or typing `book save` alone) replaces the `book-file` file with the new book. In that case, **the previous file is kept under its name with `.old` added** (since v4.5.5-nikque.13).

```
>book new 8 12
New book 8 49......done>
>quit
WARNING: data/book.dat holds the book in use before "book new": it is kept as data/book.dat.old
```

In this example, after exit, `data/book.dat` is the new book (one initial position, 84 bytes). The previous book (98,976 bytes) remains as `data/book.dat.old`.

To go back to the previous book:

1. Quit Edax.
2. Delete or rename `book.dat`.
3. Rename `book.dat.old` to `book.dat`.

Details:

- If `book.dat.old` already exists, the name is `book.dat.old.1` (then `.old.2`, …). A previously kept file is never overwritten.
- Nothing is kept when the previous `book-file` file contained only the initial position (such as the package's `book.dat`), or when there was no file.
- After `book new`, typing `book save data/book.dat` **with the file name** replaces the file as instructed, and no `.old` file is kept.
- The same applies if you grow the new book after `book new` and then exit Edax: `book.dat` is the grown new book, and `book.dat.old` is the previous one.
- If you change your mind after typing `book new`, type `book load data/book.dat` to reload the previous book. No file is changed.
- Edax never deletes an `.old` file by itself. Delete it yourself when you no longer need it (for a large book, it is as large as the book).

The same mechanism applies to saves after `book load` of another file and after `book import` (note 2 of [6.6](06-book-basics.md)).

**Up to v4.5.5-nikque.12, the previous file is not kept.** Quitting Edax right after `book new` overwrites the `book-file` file with the new empty book. In a test, just `book new` followed by `quit` replaced a 98,976-byte `book.dat` with an 84-byte file containing one initial position. With an older version, do not type `book new` while a book you care about is in use.

**To avoid confusing the previous book with a new one**: to start a new book, it is safer not to use `book new`. Instead, set `book-file` in `config.ini` to a new name (`data/my.dat`, for example), then start Edax. If no file of that name exists, Edax creates a new book at the `level` setting and starts with it. Copy the files of books you care about to another location from time to time.

## 6.8 The commands of this chapter

| Command | Meaning |
|---|---|
| `book show` | Show how the current position is recorded in the book |
| `book info` | Information about the whole book |
| `book stats` | The counts of positions, links and leaves in detail |
| `book on`, `book off` | Use the book in a game or not |
| `book randomness n` | The margin for choosing a move from the book |
| `book depth n` | Change the depth of the book (the range that is grown) |
| `book new level depth` | Make a new empty book |
| `book load file` (`book open`) | Read a book |
| `book save file` | Save the book (without a file name: to the file of `book-file`) |
| `book analyze n` (`book a`) | Look back at the last n moves of the current game with the scores of the book ([3.4](03-playing.md)) |
| `book verbose n` | How much the book commands print (0: nothing, 1: normal, 2: the search table too) |

Next: [7. Growing a book](07-book-learning.md)
