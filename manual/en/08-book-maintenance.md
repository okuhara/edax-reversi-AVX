# 8. Maintaining a book

[Contents](README.md) | Previous: [7. Growing a book](07-book-learning.md) | Next: [9. Large books](09-large-books.md)

This chapter explains the commands that repair, merge, trim and export a finished book, and recompute its scores.

**Every command changes the book in memory.** The changes reach a file:

- when you type `book save`;
- when the command saves automatically (table in [6.6](06-book-basics.md));
- when Edax exits (to `book-file`, if the book was changed).

**Nothing can be undone.** Back up the file of any book you care about before maintaining it.

## 8.1 An overview of the maintenance commands

| What you want | Command | Section |
|---|---|---|
| Carry the scores from the bottom to the top again | `book negamax` | 8.2 |
| Make the missing links and repair wrong positions | `book fix` | 8.2 |
| Merge two books into one | `book merge` | 8.3 |
| Make the book shallower, or keep what follows one opening only | `book depth` + `book subtree` | 8.4 |
| Drop the lines where both sides play badly | `book prune` | 8.4 |
| Remove what `book deviate3 X Y` cannot reach, to make the book lighter | `book reduce` | 8.4 |
| Write to a text file and read back | `book export`, `book import` | 8.5 |
| Compute the scores of the leaves again, after `eval.dat` was replaced for instance | the `book leaf recalculate` family | 8.6 |
| Verify the values of the exactly solved positions | `book correct` | 8.7 |
| Take games or problems out of the book | `book extract`, `book problem` | 8.8 |

## 8.2 book negamax and book fix

### book negamax

```
>book negamax
Negamaxing book...done
Sorting book...done>
```

Starting from the initial position and following the links, this recomputes the score of every position from the values of the positions below it. Then it sorts the moves of each position, best first.

- The commands that change the book run negamax automatically, so you rarely need to type it. You do need it after loading an intermediate file, especially one saved during `book leaf recalculate`.
- Positions that cannot be reached from the initial position through links (orphan positions that no position links to) are not recomputed.

### book fix

```
>book fix
Fixing book...
Fixing book...0 done
Linking book...
Searching positions...
Searching positions...6 done
Linking book...2246 done
Negamaxing book...done
Sorting book...done>
```

This checks the whole book and repairs it, in this order:

1. **Check** (`Fixing book`): positions with invalid content (such as a recorded move that is illegal) are found and rebuilt. `0 done` means none was found.
2. **Linking** (`Linking book`): in every position, each move that leads to another book position is made a link. If a position's leaf move became a link, a new leaf is searched for (`Searching positions`).
3. negamax and sorting.

When `book fix` helps:

- **After growing with `book-expand-tasks` at 2 or more (including `auto`).** Positions expanded in parallel do not see each other, so a link may be missing where another line reaches the same position ([7.6](07-book-learning.md)). `book fix` creates it.
- When you start using someone else's book, or a book made by an old version.
- When the book behaves strangely (for example, Edax stops in the middle of the book).

It takes time on a large book ([chapter 9](09-large-books.md)). A rebuilt position is searched at the book's level.

## 8.3 Merging books: book merge

```
>book load data/a.dat
>book merge data/b.dat
Checking book data/b.dat...
Merging book data/b.dat...
Merging book data/b.dat...1 positions added
Linking book...
 …
Negamaxing book...done
Sorting book...done>
Merged book saved to data/my.dat.mrg
```

This merges another book file (the source) into the current book (the destination).

- **Only the positions that exist only in the source** are added. For a position in both books, the destination's version is kept (not overwritten).
- After adding, linking, checking, negamax and sorting are all done. There is no need to run `book fix` as well.
- With the setting `book-merge-auto-save = on` (default), the result is saved automatically to `<book file name>.mrg`.
  - This save does not overwrite the original book file.
  - To use the `.mrg` file, load it with `book load` or rename it.
- If the source file does not exist or is damaged, nothing is merged and the current book is kept.

  ```
  cannot open data/nofile.dat
  WARNING: Book data/nofile.dat was not merged
  ```
- The source is not loaded into memory (its file is read twice). Only the destination book uses memory.

**Cautions**

- Both books should have been grown at **the same level**. With different levels, the reliability of the scores becomes inconsistent within the merged book, and linking needs more searches.
- For a position in both books, the destination's values remain. Use the newer, more reliable book as the destination.
- You can grow the same book in parallel on several PCs or in several folders and merge the results afterwards. In this fixed version, however, a single Edax instance can make full use of many threads ([7.2](07-book-learning.md), [7.6](07-book-learning.md)), so this is less necessary.

## 8.4 Trimming a book

### book depth and book subtree: making it shallower, keeping one part

`book subtree` keeps **only the positions that can be reached from the current board position by following links** (within the book's depth) and removes all others.

**Use 1: making the book shallower** (for example, from depth 40 to 39)

```
>book depth 39
>init
>book subtree
```

1. `book depth 39` changes the depth setting.
2. `init` returns to the initial position.
3. `book subtree` removes the positions beyond depth 39. Orphan positions that cannot be reached from the initial position are removed too.

**Use 2: extracting the lines that follow one opening**

```
>play f5d6c3d3
>book subtree
{board:--------------------OX-----XO-----XXO-------O------------------- X; level:8; best: +0 [-2, +2];moves: [F4:+0] <F6:-5>}
Book subtree 13... done
done
 …
>book save data/sub.dat
```

Only the current board position and the lines that follow it remain.

- **The positions from the initial position down to it are removed too.** So the resulting book is for studying from that position (`book show` at the initial position then prints nothing).
- To keep the original book, save the result with `book save` under **another name**, then reload the original book before quitting (see the cautions below).
- If the current board position is two or more moves beyond the book's depth, a warning is printed and nothing is done (since v4.5.5-nikque.13; earlier versions emptied the book).

  ```
  WARNING: book subtree: this position is deeper than the book depth; the book is not changed
  ```
- `book subtree` does not rebuild the links after trimming (since v4.5.5-nikque.12). Run `book fix` before or after if needed.

### book prune: dropping the lines where both sides play badly

```
>book prune
{board:---------------------------OX------XO--------------------------- X; level:8; best: +0 [-2, +2];moves: [D3:+0] [C4:+0] [F5:+0] [E6:+0]}
Book prune 808... done
Book prune 1448... done
done
 …
>book info
Positions: 1269 (moves = 1296 links + 1265 leaves);
```

This keeps only the positions that can be reached from the initial position by lines where "**one side plays any move and the other plays only best moves**". Both cases are covered: Black plays any move, and White plays any move. Positions reached after both sides have played a non-best move are removed. In the example above, 2,262 positions were reduced to 1,269.

Use it to make a smaller book for playing. What remains is the range where you can answer with the best move whatever the opponent plays. On a book grown for study to cover mistakes by both sides (expanded with `book deviate2`), most of the book is removed.

### book reduce: keeping only what book deviate3 X Y visits, to make the book lighter

This command exists since v4.5.5-nikque.20.

```
>init
>book reducecount id=1 x=2 y=4
@end id=1 kept=19 removed=25184 positions=25203 ms=16
>book reduce 2 4
Book reduce 2 4: 19 positions kept, 25184 removed
Fixing book...0 done
Negamaxing book...done
Sorting book...done
>book info
Positions: 19 (moves = 22 links + 17 leaves);
```

This keeps only the positions that `book deviate3 X Y` visits from the current board position, and removes all others. In the example above (a small test book), 25,203 positions were reduced to 19. Usually, return to the initial position with `init` first.

What remains:

- The current board position, and the positions reached from it by moves that each lose at most X discs, with a total loss of at most Y discs along the way.
- This is the range that `book deviate3 X Y` visits when it looks for positions to expand. If the same position can be reached in several ways, the way with the smallest total loss counts.
- Also kept: the positions just beyond the book's depth, and game-over positions, that a visited position reaches by such a move.

How it works:

- It first recomputes the values of the positions after the current board position (as `book deviate3` does).
- A move to a removed position may become the leaf of its position (as with `book prune`).
- The values of the kept positions are those recomputed in the first step and do not change afterwards.
- Running `book deviate3 X Y` from the same position afterwards selects the same first leaves as before.

Restrictions and messages:

- X is 0 to 129, and Y is 0 to 254. Edax shows a warning and does nothing if two numbers are not given, if a number is out of range, or if other words follow the numbers (since v4.5.5-nikque.21).
- From a position other than the initial one, the positions from the initial position up to it are removed too, as with `book subtree`.
- If the current board position is not in the book, or is two or more moves beyond the book's depth, a warning is shown and nothing is done.

Related features:

- You can check the numbers first with `book reducecount id=N x=X y=Y`. It does not change the book ([12.4](12-integration.md)). It counts with the same recomputed values as `book reduce`, so the numbers match.
- If `book reduce` runs after the count, with the book unchanged, from the same position and with the same X Y, it reuses the count's result without recomputing the values or traversing again (since v4.5.5-nikque.22; setting `book-reduce-reuse`).
- The result of the count (about 2 bytes per position) is kept until `book reduce`, a command that changes the book, or the next `book reducecount`.
- If another analysis (`book moveinfo`, ...) needs memory for its marks meanwhile, the result is discarded and its memory is given to that analysis.
  - `book reduce` then recomputes and traverses again. The same positions are kept.
  - Up to v4.5.5-nikque.26, the result was kept until the book changed. The marks of the other analyses could take about 16 bytes each.
- After most positions have been removed, the book shrinks its number of buckets and releases memory (since v4.5.5-nikque.22; setting `book-reduce-shrink`). The positions are then saved in a different order (the positions themselves are the same).

### Cautions when trimming

**Since v4.5.5-nikque.13, a book trimmed with `book subtree` or `book prune` is saved to the `book-file` file when Edax then exits.** The book from before the trimming is overwritten. The same applies to a book trimmed with `book reduce` (since v4.5.5-nikque.20).

- To keep the book from before the trimming, **back up the file first**. Alternatively, save the trimmed book with `book save another-name`, then reload the original book with `book load` before quitting.
- Changing only the depth setting with `book depth` does not trigger a save.

## 8.5 Exporting to and importing from a text file

```
>book export data/my.txt
>book import data/my.txt
```

`book export` writes the book to a text file, one position per line.

```
----------X--O----XXOX--XXXOO----OOOO-----O--------------------- X,8,F4,5
```

Each line contains:

- the board (64 squares). **The side to move's discs are `X` and the opponent's are `O`**, and the side-to-move field is always `X`;
- the level;
- the leaf move;
- its score.

Links are not written; they are rebuilt on import.

The last line of the file contains the book's depth (since v4.5.5-nikque.13).

```
% depth 12
```

`book import` reads such a file and **replaces the current book with it**.

- After reading, it does the linking, the check, negamax and the sorting.
- The depth is taken from the last line, `% depth`. For example, a book of depth 12 that is exported and imported gives `Depth: 12`.
- The imported book is saved to `book-file` when Edax exits. The previous `book-file` file is kept under its name with `.old` added (note 2 of [6.6](06-book-basics.md)).

Notes:

- If the file cannot be opened or contains no positions, the current book is kept (`was not imported; current book retained` is printed).
- Lines that cannot be read as a position are skipped, and the import continues.
  - A line whose leaf score is outside -64 to 64 is one of them.
  - Up to v4.5.5-nikque.26, such a score was read wrongly: 300 was read as 44, for example.
- Because the links are rebuilt, the imported book may differ in some leaves from the book before export. In a test with a book of 2,246 positions, a second export made after the import differed in 7 lines.
- **A file without the `% depth` line** (exported by a version up to v4.5.5-nikque.12, or made by hand):
  - The depth becomes **the smallest depth that contains the deepest position**.
  - If the book had not been grown to its chosen depth, the result is shallower than the original. For example, a book made with depth 30 that holds only 4 positions down to move 3, exported by an old version and imported, gives `Depth: 3`.
  - After the import, check `Depth` with `book info`. If it is wrong, correct it with a command such as `book depth 30`.
  - If what follows `% depth` is not a number from 1 to 60, `WARNING: wrong depth: …` is printed, and the result is the same as without the line.
- **When imported by a version up to v4.5.5-nikque.12**, the depth is one more than this (`Depth: 13` for a book grown to depth 12).
  - These versions cannot read the last line of a file exported by the new version.
  - They print `WARNING: wrong board: % depth 12` and `WARNING: 1 lines of … hold no position: skipped`, and skip that line. All positions are still read.
- For a large book, the text file is much larger than the book file (for the small book above: 170 KB against 99 KB). Use `book save` for everyday saving.

## 8.6 Recomputing the leaf scores: book leaf recalculate

The score of a leaf in the book is the value from the search that created that leaf. When `eval.dat` is replaced, positions added afterwards get values from the new evaluation function, but **existing leaves keep their old values**.

The `book leaf recalculate` family searches the leaves again and replaces their values with those of the current evaluation function. No position is added.

| Command | Range followed | Leaves computed again |
|---|---|---|
| `book leaf recalculate X Y` | As `book deviate X Y` | The leaves that `book deviate X Y` would choose to expand |
| `book leaf recalculate2 X Y` | As `book deviate2 X Y` | The leaves that `book deviate2 X Y` would choose to expand |
| `book leaf recalculate3 X Y` | As `book deviate X Y` | All the leaves of the positions followed |
| `book leaf recalculate4 X Y` | As `book deviate2 X Y` | All the leaves of the positions followed |

- The meaning of X and Y, and starting from the current board position, are the same as for the corresponding `book deviate` or `book deviate2` ([7.3](07-book-learning.md)).
- **The leaves of exactly solved positions are not recomputed**, because their value does not depend on the evaluation function. Positions without a leaf (all moves are links) are not affected either.
- `3` and `4` also recompute the leaves of the deepest layer (positions with a leaf and no link).
- Each search runs at the level recorded in its position. The number of parallel searches follows `book-expand-tasks`.
- The progress and the result are saved to `<book file name>.leaf`, `.leaf2`, `.leaf3`, `.leaf4`.
  - **A file saved partway is in the state "some leaves are updated, negamax not done yet"**. Run `book negamax` after loading it.
  - **A book saved partway can be continued** (since v4.5.5-nikque.29): see "Continuing a run that did not end" below.

Example output (small book):

```
>book leaf recalculate4 2 4
Book leaf recalculate4 2 4:
Book leaf recalculate4 123 todo
Book leaf recalculate4...
Book leaf recalculate4 2 4...finished: 123 leaves, 0 scores changed (0 up, 0 down, largest +0 / -0), 0 moves changed
```

The last line gives:

- the number of leaves recomputed;
- the number of scores that changed (how many went up, how many went down, and the largest changes);
- the number of moves that changed.

### Continuing a run that did not end (since v4.5.5-nikque.29)

Each recomputed leaf gets a **mark** in the book.

When a run stops before its end (Edax closed, the PC stopped, …), continue it as follows:

1. Load the intermediate file (`.leaf`, …) or the book saved at the exit with `book load`.
2. Run **the same command again**. The marked leaves are not searched again; only the others are. The number of marked leaves is shown first:

```
Book leaf recalculate4: 1234 leaves already searched again by a run that did not end: they are not searched again
```

- **When the command ends, all the marks are removed.** A finished book has no mark.
- Within a run too, a leaf recomputed in a pass is not recomputed in the later passes.
  - This applies when `book-leaf-recalculate-rounds` is 2 or more.
  - With one-thread searches, the leaf would get the same value again.
- `leaf recalculate`, `2`, `3` and `4` use the same mark. After a run has stopped, continue with the same command and the same X and Y.
- The marks are the two upper bits of the level byte of each position in the book file (the level is 0 to 60). The file size does not change.
- **A book with marks cannot be read by the versions up to v4.5.5-nikque.28** (they treat it as damaged).
  - To use such a book with an older version or another tool, first let the command end with the new version.
- Up to v4.5.5-nikque.28 there was no mark. Running the command again started over from the beginning.

### One pass may not be enough: book-leaf-recalculate-rounds

By default, the command **traverses the range once and ends**. However, recomputing leaves changes scores, and so changes the range that is traversed. Running the same command again brings in leaves that were outside the range the first time.

With the setting `book-leaf-recalculate-rounds = n` (since v4.5.5-nikque.13), the cycle "traverse → recompute → negamax" is repeated up to n times. It **stops after the first pass in which no leaf changed**. The default is 1.

- Example measured for nikque.13 (a book of 6.49 million positions, `book leaf recalculate4 1 1`): repeated one pass at a time, the number of leaves to recompute was 4,066 → 780 → 247 → 485 → 386, and the number of changed scores was 1,419 → 411 → 42 → 46 → 0. With `book-leaf-recalculate-rounds = 10`, it stopped after 5 passes.
- Values from a multi-threaded search vary slightly between runs, so a pass with no changes may never come. The command then ends after n passes.
- **Even if `eval.dat` was not replaced, some leaves get a different value when recomputed.** This is because the search that created the leaf and a separate search run afterwards do not have exactly the same conditions. See [the eval2 release notes for v4.5.5-nikque.13](https://github.com/Nikque/edax-reversi-AVX/blob/eval2/README-NIKQUE.en.md#changes-in-v455-nikque13) for the measurements.

### Example steps after replacing eval.dat

1. Back up the book file.
2. Start Edax with the new `eval.dat` (`-eval-file`, or replace the file).
3. After `init`, type `book leaf recalculate4 X Y` (X and Y: the numbers you usually give to `book deviate2`).
4. When it has finished, check with `book info` and `book show`, and save with `book save`.

The time is proportional to the number of leaves. In the recorded measurement (a book of 661.62 million positions, level 18, 32 threads), `book leaf recalculate2 5 5` had 2.75 million leaves to recompute and processed about 2,350 per minute. The total time was not measured.

## 8.7 Other maintenance

### book correct: verifying the exactly solved positions

```
>book correct
Correcting solved positions...
Correcting solved positions...0 done (0 error found)
 …
```

This re-searches the leaf of every position in the exact range (positions whose number of empty squares is solved exactly at their level). If the result differs from what is recorded, the leaf is corrected. Then it does the same work as `book fix`.

- The number of corrections is shown as `error found`.
- The progress is saved to `.err`.
- **A book saved partway can be continued** (since v4.5.5-nikque.29).
  - Each position checked gets a mark.
  - When a run stops before its end, load `.err` or the book saved at the exit with `book load`. Then type `book correct` again.
  - The marked positions are skipped, and only the others are searched. It first shows:

```
Correcting solved positions: 50000 already done by a run that did not end, 150000 left
```

- **When every position has been checked, all the marks are removed.**
- How the marks work, and which older versions cannot read a book with marks, are as in "Continuing a run that did not end" in 8.6.
- Up to v4.5.5-nikque.28 there was no mark. A new run searched every solved position again from the beginning. On a large book this takes days, so a run that stopped lost all its work.

On a correctly built book, this command should report `0 error found`. Use it when you suspect that bugs in an old version, or results of interrupted searches, went into the book.

**Up to v4.5.5-nikque.12, this command removed the leaves of game-over and pass positions and corrupted the book's scores.** `error found` showed a large number, and scores became ±127. This is fixed in v4.5.5-nikque.13. A book damaged by `book correct` in an old version was repaired by `book correct` in v4.5.5-nikque.13 (verified on a small test book).

### book deepen: searching the positions of a lower level again with the level of the book

It applies to the positions whose level searches less deep than the level of the book. Each of them gets the level of the book, and its leaf is searched again at that level (since v4.5.5-nikque.29).

- "Less deep" means one of these, at the number of empty squares of the position:
  - a smaller depth;
  - the same depth with more pruning (a lower selectivity).
- The positions of a deeper level than the book are left as they are.
- A position searched again keeps the level of the book.
- When a run stops before its end, load the intermediate file (`.dep`) or the book saved at the exit. Then type `book deepen` again: only the positions left are searched (the level itself is the mark).
- The level of a book is the one given to `book new`, or the highest level of the positions read by `book import`.
- No command changes the level of a book. So the command is useful only for books that hold positions of several levels (books of different levels merged with `book merge`, imported books).

**Up to v4.5.5-nikque.28, this command did not work.**

- It re-searched the leaves of the positions whose level differed from the book's level. However, it searched at the current level of each position, and did not change the level.
- Nothing became deeper, and a new run searched all these positions again.
- This was the code of the original Edax (4.3).

### book verbose: how much is printed

`book verbose 0` silences the book commands. `1` (normal) shows the progress. `2` also shows the search tables.

### book feed-hash

This loads the book's scores from the current board position onwards into the search hash table, so that the next search can use the book's values. Nothing is printed.

## 8.8 Extracting data from the book

### book extract: the best lines as games

```
>book extract data/lines.txt
49 games extracted
```

Starting from the initial position, this follows the best-scoring move in each position (all of them when several are equal) and writes the resulting lines to a game file. The format is determined by the extension ([chapter 10](10-files.md)).

- A line ends with a move out of the book, with the end of the game, or where it joins a line already written (since v4.5.5-nikque.33). Earlier versions wrote the rest again for each join, so the number of lines could become very large.
- **If the output file already exists, the games are appended to it.**
- It can also be written `book extract draw file` (since v4.5.5-nikque.33).
- `book extract all file` writes **all the lines** of the book (since v4.5.5-nikque.33).
  - The lines start with f5. The other first moves are symmetries of it.
  - A line ends with a move that leaves the book, at the end of a game, or where it joins another line.
  - It corresponds to `book extract file` of Edax 4.7.
  - If the book has no position after f5, the file is not touched.
- When writing fails, the output file is left as it was.

### book problem: positions as problems

```
>book problem 52 3
Extracting 3 positions at 52 ...
-----------------OOOO----XOXXX----OOO--------------------------- X % bm E2:+1; ba C6:-2;
```

This prints up to 3 book positions with 52 empty squares in which the best move is unique (its score is better than that of the second-best move).

- Without the numbers, it uses 24 empty squares and 10 positions.
- The positions that come first in the book's internal order are chosen.
- Each line contains the board, the side to move, `bm` (the best move and its score) and `ba` (the second-best move and its score).

## 8.9 The commands of this chapter

| Command | Meaning |
|---|---|
| `book negamax` | Compute the scores again and sort the moves |
| `book fix` | Check, linking, negamax, sorting |
| `book merge file` | Take in another book |
| `book depth n` | Change the depth setting |
| `book subtree` | Keep only what follows the position on the board |
| `book prune` | Keep only the lines where one side plays best moves |
| `book reduce X Y` | Keep only the positions that `book deviate3 X Y` walks from the position on the board |
| `book export file`, `book import file` | Write to a text file, read back |
| `book leaf recalculate` (`2`, `3`, `4`) `X Y` | Compute the scores of the leaves again |
| `book correct` | Verify the exactly solved positions |
| `book deepen` | Search again the positions of a lower level with the level of the book |
| `book extract file`, `book extract draw file` | Write the best lines to a game file |
| `book extract all file` | Write all the lines to a game file |
| `book problem empties count` | Show positions as problems |
| `book feed-hash` | Put the values of the book into the hash table |
| `book verbose n` | How much is printed |

Next: [9. Large books](09-large-books.md)
