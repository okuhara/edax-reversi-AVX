# 7. Growing a book

[Contents](README.md) | Previous: [6. Book basics](06-book-basics.md) | Next: [8. Maintaining a book](08-book-maintenance.md)

This manual calls adding positions to a book "growing" it (Edax itself calls it "learning"). There are two main methods.

| Way | Commands | When it fits |
|---|---|---|
| **From games**: the positions of games you played, or of game files, are put into the book | `book store`, `book add`, `book learn` | Taking in your own games, or games of strong players and programs. Entering the openings you want to study |
| **Automatic expansion**: Edax itself chooses "promising leaves" of the book and expands them | `book deviate`, `book deviate2`, `book deviate3`, `book enhance`, `book fill`, `book play` | Making the book wider and deeper along the lines with good scores |

In practice, both are combined: first build a skeleton from games, then expand it automatically.

**Before you start**, read [6.6](06-book-basics.md) (which file is read and where the book is saved) and [6.7](06-book-basics.md) (`book new` and the previous book file).

## 7.1 Try a small book first

Before growing a large book, it is a good idea to see what the commands do on a small, low-level book. The following example takes about a minute. The output is real; the numbers may differ slightly depending on the PC and from run to run.

Start with a new book file name. `data/my.dat` does not exist yet; `-n 2` means 2 threads and `-l 8` means level 8.

```
wEdax-x86-64.exe -book-file data/my.dat -n 2 -l 8
```

Create a book of level 8 and depth 12, and expand it automatically.

```
>book new 8 12
New book 8 49......done>

>book deviate 1 2
Book deviate 1 2:
Book deviate 1 todo
Book deviate...
Book deviate...1/1 done: 1 positions, 1 links
 … (many rounds) …
Book deviate 1 2...finished

>book info
Edax Book 4.5; 2026-10-7 17:10:44;
Positions: 110 (moves = 111 links + 108 leaves);
Level 8 : 110 nodes
Depth: 12
```

The book has grown from 1 position to 110. Another command with wider conditions expands it further.

```
>book deviate2 2 4
Book deviate2 2 4:
Book deviate2 32 todo
Book deviate2...
Book deviate2...1/32 done: 1 positions, 1 links
 …
Book deviate2 2 4...finished

>book info
Positions: 1952 (moves = 1989 links + 1949 leaves);
```

When you exit with `quit`, the book is saved to `data/my.dat`. The intermediate files `data/my.dat.dev` and `data/my.dat.dev2` are also present ([6.6](06-book-basics.md)).

## 7.2 Growing from games

### book store: adding the current game

This adds the game on the board to the book. A sequence entered with `play` also works.

```
>play f5d6c3d3c4
>book store
Searching positions...
Searching positions...2 done
Linking book...
Linking book...3 done
Negamaxing book...done
```

- The positions of the game within the book's depth are searched **from the end backwards** and added to the book. The one position just beyond the depth is added too, as a deepest-layer position.
- Positions further on are not added. An unfinished game is fine.
- After adding, Edax rebuilds the links, runs negamax and saves the book to `<book file name>.store`.
- With the setting `auto-store on`, `book store` runs automatically each time a game ends ([3.9](03-playing.md)).

### book add: adding a file of games

```
>book add games.txt
Adding 3 games to book...
Searching positions...
Searching positions...4 done
Adding games...3/3 done: 3 positions, 3 links
```

This adds all the games of a game file, in the same way as `book store`. The format is determined by the extension: `.txt`, `.ggf`, `.sgf`, `.pgn`, `.wtb`, `.edx` ([chapter 10](10-files.md)). The result is saved to `<book file name>.gam`.

In a `.txt` file, each line is one game, with the moves written one after another.

```
f5d6c3d3c4f4c5b3c2
f5f6e6f4e3c5c4e7c6e2
```

### book check: comparing games with the book (the book is not changed)

```
>book check games.txt
Checking 3 games to book...
Positions : 0 missing, 20 good, 4 bad (16.67% bad)
```

This checks each pair "position, move played there" of the games against the book. Only positions within the book's depth are checked, and only games that start from the usual initial position.

| Word | Meaning |
|---|---|
| `missing` | The position is not in the book |
| `good` | The move played is the best move of the book (or has the same score) |
| `bad` | The move played is recorded in the book but is worse than the best |

### book learn: Edax plays given lines to the end and adds them

```
>book learn learn.txt
Playing games...
Playing games...2 done
Searching positions...
Searching positions...5 done
Linking book...
Linking book...2253 done
Negamaxing book...done
 …
3/3 games learned
```

For each line of the file, Edax:

1. plays these moves;
2. plays against itself from there to the end of the game;
3. adds that game with `book store`.

So you can list the opening lines you want to examine, and Edax plays them out and adds them to the book.

File format:

```
f5d6c4
2,f5f6e6f4e3
# this line is a comment
f5f4e3
```

- One game per line, with the moves one after another (spaces are allowed).
- A number and a comma at the start, as in `2,moves`, sets the book randomness (`book-randomness`) to that number for this game only.
- Empty lines, lines starting with `#` and lines containing `//` are skipped.
- The games are played at the current `level`. In positions that are in the book, the book move is played (when `book on`).
- At the end, `N/M games learned` is printed (N games learned out of M lines). A line with an illegal move produces a warning and is not learned.
- The result is saved to `<book file name>.store`.

**Continuing a run that did not end** (since v4.5.5-nikque.29):

Each time it saves to `.store`, `book learn` also writes a small text file, `<book file name>.learn`. It records:

- how many lines of the file of games have been learned;
- a check value of these lines;
- the number of positions, the level and the depth of the book at that time.

When a run stops before its end (Edax closed, the PC stopped, …), continue it as follows:

1. Load `.store`, or the book saved at the exit, with `book load`.
2. Run `book learn` again **with the same file of games**. The lines already learned are skipped; only the other lines are played and learned.

```
Book learn: the first 120 lines of learn.txt (118 games) were learned by a run that did not end: they are skipped
```

- **When the whole file has been learned, `.learn` is removed.**
- In these cases, a warning is shown and the file is learned from its first line:
  - the book in use does not match `.learn` (a different number of positions, level or depth);
  - the first lines of the file of games are not the ones learned.
- Lines added at the end of the file of games do not prevent continuing.
- With the setting `book-store-auto-save = off` (no save to `.store`), no `.learn` is written.
- Up to v4.5.5-nikque.28 there was no `.learn`. A new run played every line again from the beginning (with `book-randomness`, these were other games).

### Learning several games in parallel: book-store-tasks

With the setting `book-store-tasks`, `book store`, `book add` and `book learn` process **several games (the searches of their positions) in parallel**.

| `book-store-tasks` | Behaviour |
|---|---|
| `auto` (default) | `n-tasks` games are learned at the same time (1 thread per game). The fastest |
| A number n (2 or more) | n games at the same time (`n-tasks ÷ n` threads per game) |
| `1` | The positions are searched one at a time with all the threads (the behaviour of the original Edax; the resulting book is the same too) |

- In the recorded measurement (`docs/validation/`) (32 logical CPUs, level 18, 128 games), `auto` was 2.8 to 3.3 times as fast as `1`.
- The book made with `auto` is not identical to the book made with `1`: the move and score of some leaves may differ by 1 or 2 discs. The difference is of the same size as when the number of search threads changes. Use `1` only when you need exactly the same book as the original Edax.
- `book learn` plays the games in groups of `book-store-tasks` games at a time. The games in a group do not see what the others learn. When a group finishes, its games are added to the book together.

## 7.3 Automatic expansion: book deviate and its family

### How it works

These commands start **from the current board position** and follow the links of the book. They **select the leaves** that meet a condition **and expand them**.

"Expanding" a leaf means:

- the position after the leaf move is searched and added to the book, so the leaf becomes a link;
- in the original position, a new leaf is searched for among the remaining moves.

So each expansion runs two searches.

One pass is called a "round" here:

1. negamax (bring the scores up to date)
2. choose all the leaves that meet the condition; their number is printed as `todo`
3. expand the chosen leaves, one at a time (or several at the same time)
4. negamax, and save at the chosen interval

Expansions change scores, so new leaves start to meet the condition. **The rounds are therefore repeated until nothing more is added.** At the end, `...finished` is printed. With wide conditions, there can be hundreds of rounds, taking hours or days.

- The starting position is **the current board**.
  - To expand the whole book from the initial position, type `init` first.
  - To focus on one opening, advance to it with something like `play f5d6c3`, then type the command.
- **If the current board position is not in the book, the command does nothing.** Nothing is printed, and `>` appears again at once. To start from a position that is not in the book, first add the moves leading to it with `book store`.
- Nothing is expanded beyond the depth of the book ([6.4](06-book-basics.md)).
- A line such as `Book deviate2...11/32 done: 11 positions, 11 links` means "11 of the 32 selected leaves have been expanded; this round has added 11 positions and 11 links".

### The conditions of the three commands

Each command takes two numbers.

| Command | First number | Second number | Leaves of exactly solved positions |
|---|---|---|---|
| `book deviate X Y` | How many discs "one side" may be off the best move (relative error) | How many discs the score may be away from the score of the starting position (absolute error) | Expanded |
| `book deviate2 X Y` | How many discs one move may lose (either side) | The limit for **the total of discs lost by both sides** from the starting position down to there | **Not expanded** |
| `book deviate3 X Y` | As `book deviate2` | As `book deviate2` | Expanded |

Without the numbers, all three use `2 4`.

A word other than `export` (the export below) after the numbers is an error.

- Edax prints `ERROR: book deviate2: unknown word … after the numbers` and does not expand the book.
- Up to v4.5.5-nikque.26, a typo such as `exprot` was skipped and the book was expanded.

**book deviate X Y** (the method of the original Edax): each round selects and expands leaves twice.

- The first time, it follows lines where the side to move in the starting position may also play moves up to X discs worse than the best, while the opponent plays only best moves.
- The second time, the roles are reversed.
- In both cases, only moves whose score is within "the starting position's score ± Y" are considered.

In short, it expands the lines where "**one side makes small mistakes and the other plays correctly**".

**book deviate2 X Y** (added in this fixed version): each time a move is chosen, the difference between its score and the score of the best move in that position is counted as "discs lost". It follows the lines where **each move loses at most X discs and the total from the starting position (Black and White together) is at most Y discs**, and expands the leaves within that range.

An example with `book deviate2 5 5`:

| Losses along the line | Within the range? |
|---|---|
| Black 2 discs, White 3 discs (total 5) | Yes |
| 5 discs in one move (total 5) | Yes |
| 6 discs in one move | No (over the limit for one move) |
| Black 3 discs, White 3 discs (total 6) | No (over the limit for the total) |

`book deviate2` **does not expand the leaves of exactly solved positions**. These are positions whose number of empty squares is within the exact range of their level (21 or fewer at level 18). In such a position, the leaf score is already exact, so adding the positions after it would not change any score. This avoids pointless expansion of the last layers of the book.

**book deviate3 X Y**: the same conditions as `book deviate2`, but the leaves of exactly solved positions are expanded too.

### Which one to use, and choosing the numbers

- **`book deviate2` is generally recommended.** Its range is defined by the total of both sides' mistakes, so the lines that actually occur in games are expanded evenly.
- The workload grows very quickly with larger numbers. On the small book above (level 8, depth 12), `book deviate 1 2` gave 110 positions, and `book deviate2 2 4` after it gave 1,952. (**These figures come from a small book and do not predict those for a large one.**) Start with small numbers (`1 2`, `2 4`) and widen them while watching the time and the number of positions.
- If the command is interrupted, everything saved up to then remains (7.5 below). Typing the same command again continues from there: leaves that were already expanded are now links and are not selected again.

### Writing the lines they walk to a text file, without expanding (export)

With `export <file> [leaf|ends|all]` after the numbers, each of the three commands **does not expand the book**. Instead, it writes the lines it walks from the current position to a text file (since v4.5.5-nikque.26).

```
book deviate2 2 4 export lines.txt ends
```

| Last word | Positions written |
|---|---|
| `leaf` (when left out) | the positions whose leaf the command would expand in its first round (a leaf not searched further yet) |
| `ends` | the ends of the lines walked (no move within the limits leads further into the book) |
| `all` | every position of the lines walked (the current position included) |

**The lines written**

- Each line is one position: the moves of the game up to the current position, followed by the moves from there to that position.
- The moves are written as `f5d6c3...` (lowercase). Passes are not written.
- A position reached by several lines (a transposition) is written once, with this line:
  - `book deviate`: the line with the fewest moves;
  - `book deviate2` and `book deviate3`: the line with the smallest total loss;
  - in both cases, the first one found among equal lines.
- A position that must pass and the position after the pass have the same line. Only one line is written for them.
- When the current position is the initial position, `all` writes the initial position itself as an empty line.
- The ends are these positions:
  - the positions where the game is over;
  - the positions beyond the depth of the book (`book depth`);
  - the positions with no move within the limits in the book.
- For `book deviate`, the lines of its two walks (above) are written together. Both walks are made on the book before any expansion.
- For `book deviate`, `ends` are the positions from which neither walk goes further.

**The range of the numbers**

- The total loss of `book deviate2` and `book deviate3` is at most 254, as when they expand.
- A larger number is reduced to 254, with the same warning as when they expand (`WARNING: total loss 300 reduced to 254`). `edax_book_deviate_export` of libedax does the same.
- Up to v4.5.5-nikque.26, libedax alone printed `cumulative loss = 300 is out of range. Set to 254`.

**The end, and file names**

- At the end, `Book deviate2 2 4 export ends...finished: 120 lines written (lines.txt)` is shown.
- When the current position is not in the book, an empty file is made.
- The book does not change. So nothing is saved, and no progress file is written.
- A file name that holds spaces is written between double quotes (`book deviate2 2 4 export "C:\My Books\lines.txt" ends`). The quotes are not part of the name.

**When the lines cannot be written**

Edax prints a line beginning with `ERROR:` and does not expand the book. This happens in these cases:

| Cause | Message |
|---|---|
| No file name | `ERROR: book deviate export: no file` |
| A file name that is too long (259 characters at most on Windows) | `the file name is too long` |
| A quote that is not closed | `the quote of the file name is not closed` |
| A last word other than `leaf`, `ends` and `all` | `unknown word` |
| The file cannot be written | `ERROR: book deviate2 2 4 export: cannot write the file lines.txt` |

Up to v4.5.5-nikque.26, a file name that was too long was silently cut short.

## 7.4 Automatic expansion: the other commands

| Command | Meaning | Intermediate file |
|---|---|---|
| `book enhance X Y` | Uses the lower and upper bounds of the scores to expand "the leaves that could change the score of a position once the error of the search is allowed for (X discs for a midgame search, Y discs for a search that reads to the end but not at 100%)". Repeated until nothing changes. Without the numbers: `2 4` | `.enh` |
| `book fill n` | When n moves from a position of the book lead to another position of the book, the positions and links in between are added. The position filled from gets its links at once (when its leaf was that move, it is searched again for its next leaf, as when `book deviate` expands it). Repeated until nothing is added. `book fill 1` (also when the number is left out) only makes the missing links of the positions one move after the positions of the book: no position is added | `.fill` |
| `book fill2 n` | As `book fill n`, but the first move from the position filled from is **its leaf move only**. It fills "the position after the leaf is not in the book, and the next one is" (with n = 2). Without a number, n is 2 | `.fill2` |
| `book leaf expand` | Expands the leaf of **the positions whose leaf scores better than every link** (their best move is still out of the book). After an expansion the next leaf may be better than the links again, so this is repeated until no such position remains. Positions without a link do not count | `.lexp` |
| `book play` | Expands the leaf of positions that have no link at all (the tips of the lines), and repeats until no such position remains within the depth of the book. That is, **every line is extended with best moves down to the depth of the book** | `.play` |

- `book enhance`, `book fill` and `book play` come from the original Edax.
- `book fill2` and `book leaf expand` were added in this fixed version (since v4.5.5-nikque.14). The same version changed `book fill` (except `book fill 1`).
- All of these commands have been used less on large books than the `book deviate` family. Try them on a small book first.

### Limiting the range: the two numbers of book fill, book fill2 and book leaf expand

With two extra numbers X and Y, as in `book fill 2 5 5`, `book fill2 2 5 5` or `book leaf expand 5 5`, the command applies **only to the range that `book deviate3 X Y` follows from the current position**.

- X is the maximum loss per move and Y the maximum total loss (both sides together), as for `book deviate2` and `book deviate3` in 7.3.
- Without the numbers, the command applies to the whole book.

| Command | Positions filled or expanded from |
|---|---|
| `book fill n X Y` | Every position that `book deviate3 X Y` follows |
| `book fill2 n X Y` | The positions whose leaf `book deviate3 X Y` would expand |
| `book leaf expand X Y` | The positions that `book deviate3 X Y` follows whose leaf is better than every link |

- The leaves of exactly solved positions count too (as with `book deviate3`).
- The n of `book fill` and `book fill2` means "**exactly n moves**": a gap shorter than n moves is not filled.
  - Where a single position is missing (book position → missing position → book position), n = 2 fills it.
  - Without a number, `book fill2` uses 2. `book fill2 1` adds no position, so it does nothing.
- The number of parallel searches follows `book-expand-tasks` (7.6), as for `book deviate`. With the built-in value (1), the positions are searched one after another.
- The positions to fill or expand from are found by several threads, without copying the book.

**Every round first adds links.**

- `book fill` and `book fill2` link the positions that have a move (any move) that leads to a position already in the book but is not a link.
- `book leaf expand` picks the positions whose leaf move leads to a position already in the book, and adds all their missing links. A position whose leaf became a link searches for a new leaf.
- Such positions appear when the same position is added from another line (`book deviate` also creates them).
- `book fix` links them as well. However, a newly searched leaf may again be better than the links, so these commands do the linking themselves and repeat until nothing is left to link, fill or expand.
- With the two numbers, this applies only to the positions that `book deviate3 X Y` follows.
- After `book fill` and `book fill2`, no missing link remains in the range. (Up to v4.5.5-nikque.13, `book fill` also added the missing links of the positions around the positions it filled from.)
- `book leaf expand` does not link the positions whose leaf does not lead to a book position. Use `book fix` to add their missing links.

Example output (the small book of 7.1, after `book deviate2 2 4`):

```
>book fill2 2
Book fill2 (links): 13 positions have a move to a position of the book that is not a link
Book fill2 (links)...13/13: 0 positions, 17 links
Book fill2 2: 36 positions to fill from
Book fill2...36/36: 36 positions, 75 links
 … (repeated until nothing is added) …
Book fill2 2... finished

>book fill 2 2 4
Book fill 2 2 4:
Book fill (links): 8 positions have a move to a position of the book that is not a link
Book fill (links)...8/8: 0 positions, 10 links
Book fill 781 positions selected, 52 to fill from
Book fill...52/52: 56 positions, 117 links
 …
Book fill 2 2 4...finished

>book leaf expand 3 6
Book leaf expand 3 6:
Book leaf expand (links): 6 leaves lead to a position of the book
Book leaf expand 3 todo
Book leaf expand...
Book leaf expand...1/3 done: 1 positions, 1 links
 …
Book leaf expand 3 6...finished
```

- The `(links)` lines give the number of positions linked.
- `positions selected` is the number of positions selected by the `book deviate3` traversal.
- `to fill from` is the number of those positions that are actually filled from.
- `positions, links` gives the positions and links added.

Example output on a small book:

```
>book play
Book play...153 todo
Book play...
Book play...1/153 done: 1 positions, 1 links
 …
```

## 7.5 Saving progress, and stopping

The automatic expansion commands run for a long time. **If they stop (power failure, PC restart, or stopped by you), everything after the last save is lost.** Two settings control how often the book is saved.

| Setting | Value in the package | Meaning |
|---|---|---|
| `book-save-interval` | 360 (minutes) | During the expansions, save each time this much time has passed. `0`: no save by time |
| `book-deviate-save-rounds` | 1 | For the `book deviate` family: save each time this many rounds that added positions or links have ended. `0`: only when the command ends |

- The save goes to `<book file name>.dev2` and so on (table in [6.6](06-book-basics.md)). `book.dat` itself is not rewritten while a command runs.
- With a large book, each save takes time and free disk space ([chapter 9](09-large-books.md)).
  - If the rounds are short, saving every round means spending most of the time saving. In that case, increase `book-deviate-save-rounds` (to 10, for example).
  - If one round takes hours, shorten `book-save-interval` instead.
- **With `0` and `0` (neither kind of save), nothing is saved until the command ends.** If it stops partway, everything is lost.

**Stopping**: the book commands cannot be cancelled by input.

- Typing `stop` does not stop them (since v4.5.5-nikque.13; a message says that nothing was stopped). In earlier versions, the position being searched was cut short and its incomplete result went into the book.
- Typing `quit` ends Edax **after the command has finished**.
- To stop at once, terminate the Edax program itself (close the window, or press Ctrl+C).
  - The book in memory is lost; what remains is the last saved file.
  - Even if Edax is terminated during a save, a partially written file never replaces the real one ("How saving works" in [6.6](06-book-basics.md)).

**Resuming after a stop**: the last intermediate file (`data/book.dat.dev2`, for example) should be newer than `data/book.dat`. Resume in one of these ways:

- Rename the files: keep `book.dat` under another name, rename `book.dat.dev2` to `book.dat`, then start Edax.
- Start Edax and type `book load data/book.dat.dev2`. In this case too, the automatic save goes to `book-file` (note 2 of [6.6](06-book-basics.md)).

Then type the same command again (`book deviate2 5 5`, for example).

When the same command is typed again, whether it searches again the work already done depends on the command:

- `book deviate` and its family, `book enhance`, `book leaf expand`, `book fill`, `book fill2`, `book play`, `book add`: the work already done is not searched again.
  - The moves expanded and the positions added are in the book.
  - Only the time to walk the book again is spent.
- `book correct` and the `book leaf recalculate` family (since v4.5.5-nikque.29; [8.6](08-book-maintenance.md), [8.7](08-book-maintenance.md)):
  - Each position searched again gets a mark in the book. A new run searches only the positions without a mark.
  - A book saved with marks cannot be read by v4.5.5-nikque.28 and earlier.
- `book learn <file>` (since v4.5.5-nikque.29; book learn in [7.2](07-book-learning.md)):
  - `<book file name>.learn` records how many lines have been learned.
  - With the same file, these lines are skipped.
- `book deepen` (since v4.5.5-nikque.29; [8.7](08-book-maintenance.md)): the positions given the level of the book are not searched again.
- Up to v4.5.5-nikque.28, `book correct`, the `book leaf recalculate` family, `book learn` and `book deepen` started over from the beginning.

## 7.6 Expanding several leaves in parallel: book-expand-tasks

Low-level searches are short, so giving many threads to one search does not make it much faster ([5.5](05-search.md)). With the setting `book-expand-tasks`, **several leaves are expanded in parallel**, which makes full use of the CPU. It applies to `book deviate`, `book deviate2`, `book deviate3`, `book enhance` and `book play`.

| `book-expand-tasks` | Behaviour |
|---|---|
| `1` (the value without `config.ini`) | One at a time, each search with all the threads. The behaviour of the original Edax |
| A number n | n leaves at the same time. One search uses `n-tasks ÷ n` threads |
| `auto` (the value in the `config.ini` of the package) | Decided from the level of the book: 2 threads per search up to level 18, 4 up to level 24, 8 above (the number of leaves expanded at the same time is `n-tasks` divided by that). In addition, up to level 18, when a round has at least 32 times `n-tasks` leaves to expand, `n-tasks` searches of 1 thread run at the same time |

- In the recorded measurement (`docs/validation/`) (32 logical CPUs, level 18, a book of 656.88 million positions, the first 10,000 leaves of `book deviate3 2 5`): about 805 to 832 seconds with `1`, about 174 seconds with `16` (2 threads each).
- **The resulting book is not the same as with `1`.**
  - The positions added and the link moves are the same, but the move and score of some leaves may differ by 1 or 2 discs.
  - Also, a position being expanded in parallel with others only sees the positions already added to the book. When two positions in the same round lead to the same new position, the link may not be created immediately. It is created in a later round or by `book fix`. Which happens depends on the order in which the searches finish (see `book-reproducible` below).
- Each parallel search has its own hash table, so more memory is used ([chapter 9](09-large-books.md)).
- A round with a single leaf behaves the same as expanding one at a time. A round with few leaves runs fewer searches in parallel.
- For exactly the behaviour of the original Edax, set `book-expand-tasks = 1`.

### The same book from run to run: book-reproducible and book-seed

When `book-expand-tasks` is 2 or more (including `auto`), a position being expanded sees the result of another one only if that result was added to the book first. This depends on the order in which the searches finish, so the same command can produce a slightly different book on each run.

In addition, the choice among book moves of equal score is random (even with `book-randomness = 0`), and its seed comes from the clock. So `book learn` learns different games on each run.

| Setting | Behaviour |
|---|---|
| `book-reproducible = on` | Every search of the book commands has one thread, and the results of the expansions done at the same time are put into the book in the order they were chosen. Before that, an expansion whose new position got other links since it was searched is searched again. The commands that expand give the book of `n-tasks 1`, whatever the number of threads |
| `book-seed = a number` | The seed of the random numbers of the book. `book learn` learns the same games from a run to the next |

- The defaults keep the previous behaviour (`off`, `auto`).
- With `on`, each one-thread search has the hash table of a one-thread run (57 MB with `hash-table-size = auto`). That is about 1.8 GB with `n-tasks` 32.
- "The same book" means identical content except the save date (8 bytes at the start of the file).
- With several threads, `book store`, `book add` and `book learn` produce the same book on each run, but not the book of `n-tasks 1`.
- These settings do not affect the searches for game moves (`play`, `go`, the games of `book learn` with `n-tasks 1` or `book-store-tasks 1`) or time-limited searches.

## 7.7 Examples of growing

**Example 1: exploring an opening you want to study**

```
>play f5d6c3d3c4
>book deviate2 3 6
```

Only the lines that follow the current board position (the opening called `tiger`) are expanded.

If this position is not in the book yet, the command does nothing. Type `book store` first to add the moves leading to it. The full order is: `play f5d6c3d3c4`, `book store`, `book deviate2 3 6`.

**Example 2: adding a file of games, then expanding the whole book**

```
>book add mygames.txt
>init
>book deviate2 2 4
```

**Example 3: making Edax play out a list of lines**

```
>book learn openings.txt
```

**Example 4: expanding a large book a little every day**

1. Set the save intervals and the number of parallel expansions in `config.ini`.
2. Start Edax and type `book deviate2 5 5`.
3. Stop it when your time is up.
4. The next day, resume from the last intermediate file (7.5).

The cautions for books of hundreds of millions of positions are collected in [chapter 9](09-large-books.md).

## 7.8 The commands of this chapter

| Command | Meaning |
|---|---|
| `book store` | Add the current game to the book |
| `book add file` | Add the games of a game file to the book |
| `book check file` | Count the moves of the games against the book |
| `book learn file` | Make Edax play out a list of lines against itself and add them to the book |
| `book deviate X Y` | Expand the lines where one side is off by up to X discs and the score is within ±Y |
| `book deviate2 X Y` | Expand the lines with losses of up to X discs per move and Y discs in total (leaves of exactly solved positions excluded) |
| `book deviate3 X Y` | The same (leaves of exactly solved positions included) |
| `export file` [`leaf`, `ends` or `all`] after X Y of the `book deviate` family | Write the lines it walks to a text file, without expanding the book (example: `book deviate2 2 4 export lines.txt ends`) |
| `book enhance X Y` | Expand the leaves that could change a score once the error of the search is allowed for |
| `book fill n` [X Y] | Fill in between the positions of the book (with X Y: in the range of `book deviate3` only) |
| `book fill2 n` [X Y] | Fill the gaps after the leaves |
| `book leaf expand` [X Y] | Expand the leaves better than every link |
| `book play` | Expand the positions without a link, one move at a time |

Next: [8. Maintaining a book](08-book-maintenance.md)
