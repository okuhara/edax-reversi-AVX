# 14. Reference

[Contents](README.md) | Previous: [13. Troubleshooting](13-troubleshooting.md) | Next: [15. Glossary](15-glossary.md)

All the commands and settings of this version, compiled from the lists in the source. Items with an empty "Chapter" column are for development and testing and are not explained in this manual.

## 14.1 Commands for playing and analysing

| Command | Short name | Arguments (those in [ ] may be left out) | Meaning | Chapter |
|---|---|---|---|---|
| `help` | `?` | [`options`, `commands`, `book`, `base`, `test`] | The built-in help (English) | [3](03-playing.md) |
| `init` | `i` | | New game from the usual initial position | [3.3](03-playing.md) |
| `new` | `n` | | New game from the current *first position* | [3.3](03-playing.md) |
| `load` | `o`, `open` | file | Load a game | [3.6](03-playing.md) |
| `save` | `s` | file | Save the game | [3.6](03-playing.md) |
| `quit` | `q`, `exit` | | Exit | [2.7](02-quick-start.md) |
| `undo` | `u` | | Take back a move | [3.3](03-playing.md) |
| `redo` | `r` | | Replay a move that was taken back | [3.3](03-playing.md) |
| `mode` | `m` | [0 to 3] (3 if left out) | Who plays | [3.2](03-playing.md) |
| (the name of a square) | | | Play that move. `ps` (`pa`) is a pass | [3.1](03-playing.md) |
| `play` | | moves, or the name of an opening | Play a sequence of moves | [3.1](03-playing.md) |
| `force` | | moves, or the name of an opening | Make Edax play a given opening | [3.7](03-playing.md) |
| `go` | | | Make Edax play one move | [3.1](03-playing.md) |
| `hint` | | [1 to 60] (1 if left out) | Show the best moves | [3.4](03-playing.md) |
| `stop` | [`id=N`] | | Stop thinking (the mode becomes `mode 3`) and discard the queued commands. With `id=N`, only book analysis N stops (nothing else changes) | [3.1](03-playing.md), [12.4](12-integration.md) |
| `analyze` | `a`, `analyse` | [number of moves] (all if left out) | Look back at the last n moves of the game | [3.4](03-playing.md) |
| `setboard` | | board side | Set up a position | [3.5](03-playing.md) |
| `vmirror`, `hmirror` | | | Swap top and bottom, left and right | [3.5](03-playing.md) |
| `rotate` | | [90, 180, 270] (90 if left out) | Rotate | [3.5](03-playing.md) |
| `symetry` | `symmetry` | 0 to 15 | Choose a symmetry by its number | [3.5](03-playing.md) |
| `opening`, `ouverture` | | | Name of the opening (English, French) | [3.1](03-playing.md) |
| `options` | | | Show the current settings | [4.1](04-settings.md) |
| `version` | `v` | | Show the version | |
| (the name of a setting) | | value | Change a setting (14.5) | [4.4](04-settings.md) |
| `book`, `b` | | a book command (14.2) | | [6](06-book-basics.md) to [8](08-book-maintenance.md) |
| `base` | | a base command (14.3) | | [10.2](10-files.md) |
| `solve` | | file | Solve a problem set | [11.1](11-solve-bench.md) |
| `bench` | | [count] | Measure the speed | [11.2](11-solve-bench.md) |
| `count` | | `games`, `positions`, `shapes` [depth] [board size 6 to 8] | Count lines and positions | [11.3](11-solve-bench.md) |
| `perft` | | [depth] | Count lines (without the hash table) | [11.3](11-solve-bench.md) |
| `estimate` | | [number of times] | Estimate the number of lines | [11.3](11-solve-bench.md) |
| `microbench`, `script-to-obf`, `select-hard`, `wtest`, `weval`, `edaxify`, `seek`, `mobility`, `debug-pv` | | | For development and testing | [11.4](11-solve-bench.md) |
| `nboard 1`, `xboard`, `protocol_version`, `engine-protocol init` | | | Switch to another protocol | [12.1](12-integration.md) |

- Lines that start with `#`, and empty lines, are ignored.
- When commands are read from a file (`wEdax-x86-64.exe < commands.txt`), they run from top to bottom. Put `quit` at the end.

## 14.2 Book commands

These are entered after `book` (or `b`). The word that follows may be in upper case.

| Command | Arguments (those in [ ] may be left out; the value in ( ) is used then) | Meaning | File it saves by itself | Chapter |
|---|---|---|---|---|
| `book show` | | How the current position is recorded | | [6.3](06-book-basics.md) |
| `book info` | | Information about the whole book | | [6.3](06-book-basics.md) |
| `book stats` | | The counts in detail | | [6.3](06-book-basics.md) |
| `book on`, `book off` | | Use the book in a game or not | | [6.5](06-book-basics.md) |
| `book randomness` | [n] (0) | The margin for choosing a move from the book | | [6.5](06-book-basics.md) |
| `book depth` | [n] (36) | The depth setting | | [6.4](06-book-basics.md) |
| `book new` | [level] (21) [depth] (36) | A new empty book. **The current book in memory is discarded.** At later saves without a file name, the previous `book-file` file is kept under an `.old` name | | [6.7](06-book-basics.md) |
| `book load`, `book open` | file | Load a book. After another file is loaded, saves without a file name keep the previous `book-file` file under an `.old` name | | [6.6](06-book-basics.md) |
| `book save` | [file] (the file of `book-file`) | Save the book | | [6.6](06-book-basics.md) |
| `book verbose` | n | How much is printed (0 to 2) | | [8.7](08-book-maintenance.md) |
| `book analyze`, `book a` | [number of moves] | Look back at a game with the scores of the book | | [3.4](03-playing.md) |
| `book store` | | Add the current game | `.store` | [7.2](07-book-learning.md) |
| `book add` | file | Add a file of games | `.gam` | [7.2](07-book-learning.md) |
| `book check` | file | Compare games with the book | | [7.2](07-book-learning.md) |
| `book learn` | file | Play out a list of lines and add them | `.store`, `.learn` | [7.2](07-book-learning.md) |
| `book deviate` | [X] (2) [Y] (4). X from −129 to 129, Y from 0 to 65 | Automatic expansion (lines where one side is off) | `.dev` | [7.3](07-book-learning.md) |
| `book deviate2` | [X] (2) [Y] (4). X from 0 to 129, Y from 0 to 7740 | Automatic expansion (total of the losses; leaves of exactly solved positions excluded) | `.dev2` | [7.3](07-book-learning.md) |
| `book deviate3` | the same | The same (leaves of exactly solved positions included) | `.dev3` | [7.3](07-book-learning.md) |
| `export` of `book deviate`, `deviate2` and `deviate3` | `export file` [`leaf`, `ends` or `all`] (`leaf`) after X Y | Write the lines walked to a text file, without expanding the book (since v4.5.5-nikque.26) | | [7.3](07-book-learning.md) |
| `book enhance` | [X] (2) [Y] (4). 0 to 129 | Expand the leaves that could change a score | `.enh` | [7.4](07-book-learning.md) |
| `book fill` | [n] (1) [X Y]. n: 1 to 61, X: 0 to 129, Y: 0 to 7740 | Fill in between positions. With X Y: in the range of `book deviate3 X Y` only | `.fill` | [7.4](07-book-learning.md) |
| `book fill2` | [n] (2) [X Y]; the ranges of the numbers as for `book fill` | Fill the gaps after the leaves | `.fill2` | [7.4](07-book-learning.md) |
| `book play` | | Extend the tips of the lines down to the depth | `.play` | [7.4](07-book-learning.md) |
| `book play` | n. 0 to 60 | From the current position, extend the tips of the lines down to n empty squares (since v4.5.5-nikque.33) | `.play`, `.play-to` | [7.4](07-book-learning.md) |
| `book extend` | | From the current position, expand the leaves that no link is better than (since v4.5.5-nikque.33) | `.ext` | [7.4](07-book-learning.md) |
| `book exhaust` | | Add every move from the current position down to the depth of the book (since v4.5.5-nikque.33) | `.exh` | [7.4](07-book-learning.md) |
| `book negamax` | | Compute the scores again | | [8.2](08-book-maintenance.md) |
| `book fix` | | Check, linking, negamax | | [8.2](08-book-maintenance.md) |
| `book merge` | file | Take in another book | `.mrg` | [8.3](08-book-maintenance.md) |
| `book subtree` | | Keep only what follows the current position | | [8.4](08-book-maintenance.md) |
| `book prune` | | Keep only the lines where one side plays best moves | | [8.4](08-book-maintenance.md) |
| `book reduce` | X Y. X: 0 to 129, Y: 0 to 254 | Keep only the positions that `book deviate3 X Y` walks from the current position | | [8.4](08-book-maintenance.md) |
| `book export`, `book import` | file | Write to a text file, read back | | [8.5](08-book-maintenance.md) |
| `book leaf recalculate`, `book leaf recalculate3` | as `book deviate` | Compute the leaves again | `.leaf`, `.leaf3` | [8.6](08-book-maintenance.md) |
| `book leaf recalculate2`, `book leaf recalculate4` | as `book deviate2` | Compute the leaves again | `.leaf2`, `.leaf4` | [8.6](08-book-maintenance.md) |
| `book leaf expand` | [X Y]. X: 0 to 129, Y: 0 to 7740 | Expand the leaves better than every link. With X Y: in the range of `book deviate3 X Y` only | `.lexp` | [7.4](07-book-learning.md) |
| `book correct` | | Verify the exactly solved positions | `.err` | [8.7](08-book-maintenance.md) |
| `book deepen` | | Search again the positions of a lower level with the level of the book | `.dep` | [8.7](08-book-maintenance.md) |
| `book extract` | [`draw`] file | Write the best lines as games | | [8.8](08-book-maintenance.md) |
| `book extract all` | file | Write all the lines as games (since v4.5.5-nikque.33) | | [8.8](08-book-maintenance.md) |
| `book problem` | [empties] (24) [count] (10) | Show positions as problems | | [8.8](08-book-maintenance.md) |
| `book feed-hash` | | Put the values of the book into the hash table | | [8.7](08-book-maintenance.md) |
| `book moveinfo` | `id=N` [`base=`, `stages=`, `umi=`, `parts=`, …] | Book value, precision stage, missing leaves, umigame numbers and positions of every legal move (in the background; answers: `@` lines) | | [12.4](12-integration.md) |
| `book missing` | `id=N` [`sq=`, `stage=`, `x= y=`, `n=`] | The lines to the leaves missing for the next stage (in the background) | | [12.4](12-integration.md) |
| `book levels` | `id=N` [`scope=all`] | The positions per level (in the background) | | [12.4](12-integration.md) |
| `book bestlines` | `id=N` [`loss=`, `depth=`, `solved=`, `file=`, `which=`] | The number of lines of best moves, and a file of the moves to their ends (in the background) | The file given | [12.4](12-integration.md) |
| `book findline` | `id=N` `target=position` [`n=`] | The lines from the current position to a position, through book positions only (in the background) | | [12.4](12-integration.md) |
| `book reducecount` | `id=N` [`x=` (2), `y=` (4)] | The positions that `book reduce X Y` would keep and remove (in the background; the book is not changed) | | [12.4](12-integration.md) |

## 14.3 Base commands

These are entered after `base` ([10.2](10-files.md)).

| Command | Arguments | Meaning |
|---|---|---|
| `convert` | input output | Change the format |
| `append` | input output | Add the games of the input to the output (since v4.5.5-nikque.33) |
| `unique` | input output | Keep one copy of each game |
| `compare` | file1 file2 | Count the positions in common |
| `check` | file [empties] (24) | Look for bad moves in the endgame |
| `correct` | file [empties] (24) | Correct the bad moves of the endgame (the file is rewritten) |
| `complete` | file | Play unfinished games on to the end (the file is rewritten) |
| `problem` | file [empties] (24) output | Write positions to a problem file |
| `tofen` | file [empties] (24) output | Write positions in FEN |

## 14.4 Startup-only options

| Option | Meaning | Chapter |
|---|---|---|
| `-?`, `-help` | Show the list of options and exit | |
| `-v`, `-version` | Show the version | |
| `-solve file` | Solve a problem set and exit | [11.1](11-solve-bench.md) |
| `-bench n` | Measure the speed and exit | [11.2](11-solve-bench.md) |
| `-count games` (`positions`, `shapes`) depth [`6x6`] | Count and exit | [11.3](11-solve-bench.md) |
| `-wtest file` | Test with a WTHOR file | |
| `-gtp`, `-nboard`, `-xboard`, `-cassio`, `-ggs` (`-edax` is the normal screen) | Choose a protocol | [12.1](12-integration.md) |
| `-o file` (`-option-file`) | Read one more settings file | [4.3](04-settings.md) |
| `-q`, `-vv` | The same as `-verbose 0`, `-verbose 2` | [3.10](03-playing.md) |
| `-cpu` | Bind the threads to CPUs | [4.3](04-settings.md) |
| `-info` | Print additional lines of information | |

## 14.5 Settings

Each setting can be given in three ways ([chapter 4](04-settings.md)):

- in `config.ini` (`name = value`)
- on the command line (`-name value`)
- while running (`name value`)

The "built-in value" is the value used when nothing is given.

### Settings used often

| Name (short name) | Values | Built-in value | `config.ini` of the package | Meaning |
|---|---|---|---|---|
| `level` (`l`) | 0 to 60 | 18 | 18 | The strength of the search |
| `n-tasks` (`n`) | 1 to the number of logical CPUs (64 at most), `auto` | The number of logical CPUs | `auto` | The number of search threads |
| `hash-table-size` (`h`) | 10 to 30 (25 at most in the 32-bit versions), `auto` | 21 | `auto` | The size of the hash table (changing it while running rebuilds the tables) |
| `game-time` (`t`) | time | none | | Time for the whole game |
| `move-time` | time | none | | Time for one move |
| `ponder` | `on` / `off` | `on` | | Think during the opponent's turn too |
| `mode` | 0 to 3 | 3 | | The `mode` at startup |
| `verbose` | 0 to 4 | 1 | | How much is printed |
| `noise` | 0 to 60 | 0 | | Do not show the progress of the search below this depth |
| `width` | 3 to 250 | 80 | | Width of the table |
| `eval-file` | file | `data/eval.dat` | | The evaluation data |
| `book-file` | file | `data/book.dat` | | The book file |
| `book-usage` | `on` / `off` | `on` | `on` | Use the book in a game |
| `book-randomness` | a number, 0 or more | 0 | | The margin for choosing a move from the book |
| `book-depth` | 1 to 60, `auto` | `auto` | `auto` | The depth of the book at startup |
| `book-save-interval` | 0 to 525600 (minutes) | 60 | 360 | Saving by time while the book is grown |
| `book-deviate-save-rounds` | 0 to 1000000 | 1 | 1 | Saving by rounds, for the `book deviate` family |
| `book-merge-auto-save` | `on` / `off` | `on` | `on` | The `.mrg` file after `book merge` |
| `book-store-auto-save` | `on` / `off` | `on` | | The `.store` file after `book store` and `book learn` |
| `book-expand-tasks` | 1 to the number of logical CPUs, `auto` | 1 | `auto` | The number of positions expanded at the same time |
| `book-store-tasks` | 1 to the number of logical CPUs, `auto` | `auto` | `auto` | The number of games learned at the same time |
| `book-leaf-recalculate-rounds` | 1 to 1000000 | 1 | 1 | The limit for the number of passes of the `book leaf recalculate` family |
| `book-seed` | 0 to 2^64 - 1, `auto` | `auto` | `auto` | The seed of the random choice among book moves of equal score (`auto`: from the clock) |
| `book-reproducible` | `on` / `off` | `off` | `off` | Book commands give the same book from one run to the next ([chapter 7](07-book-learning.md)) |
| `probcut-model` | `standard` / `refit` | `standard` | `standard` | The pruning method (`refit` is experimental) |
| `book-reduce-index` | `binary` / `table` | `table` | | How `book reducecount` finds a position from its number (since v4.5.5-nikque.22; [8.4](08-book-maintenance.md)) |
| `book-reduce-walk` | `loss` / `group` | `loss` | | The order of the walk of `book reducecount` with threads (since v4.5.5-nikque.22) |
| `book-reduce-reuse` | `on` / `off` | `on` | | `book reduce` uses the result of the `book reducecount` just before (since v4.5.5-nikque.22) |
| `book-reduce-shrink` | `on` / `off` | `on` | | Fewer buckets after `book reduce` (since v4.5.5-nikque.22) |
| `auto-start`, `auto-swap`, `auto-store`, `auto-quit` | `on` / `off` | `off` | | What happens when a game ends ([3.9](03-playing.md)) |
| `repeat` | a number | 0 | | The number of games played in a row |
| `search-log-file`, `ui-log-file` | file | none | | Log files |
| `name` | text | `Edax 4.5.5` | | The name given under a protocol |
| `echo` | `on` / `off` | `off` | | Print the commands that are received |

A time is written as `seconds`, `minutes:seconds`, `hours:minutes:seconds` or `days:hours:minutes:seconds` (1 second at least).

### Settings for development and testing

These are normally left unchanged. For their meaning, see the built-in help (`-help`) and the source.

| Name | Values | Built-in value | The built-in help (summary) |
|---|---|---|---|
| `depth` (`d`), `selectivity` | a number | none | Give the depth and the confidence of the search directly (used by `-solve` and the like) |
| `alpha`, `beta` | −64 to 64 | −64, 64 | The range of scores to search |
| `speed` | a number, `auto` | `auto` | The speed of the search (nodes per second) used to share out the time. `auto`: measured |
| `nps` | a number | 0 | Count time in nodes (a clock for testing) |
| `inc-pvnode-sort-depth`, `inc-cutnode-sort-depth`, `inc-allnode-sort-depth` | a number | In the output of `options`: pv = 0, all = −2, cut = −3 | Adjust the depth used to sort the moves |
| `probcut-d` | a decimal | 0.25 | The depth ratio of the pruning |
| `pv-debug`, `pv-check`, `pv-guess` | `on` / `off` | `off` | Checks of the principal variation |
| `all-best` | `on` / `off` | `off` | (not used in this build) |
| `game-file` | file | `data/game.ggf` | (not used by the screen of this build) |
| `ggs-host`, `ggs-port`, `ggs-login`, `ggs-password`, `ggs-open`, `ggs-log-file` | text | none | The connection to GGS |
| `debug-cassio`, `follow-cassio` | (switches) | | For Cassio |

## 14.6 File extensions

| Extension | Content | Chapter |
|---|---|---|
| `.dat` (`book.dat`) | A book | [6.6](06-book-basics.md) |
| `.dat` (`eval.dat`) | The evaluation data | [1.3](01-introduction.md) |
| `book.dat.store`, `.gam`, `.dev`, `.dev2`, `.dev3`, `.enh`, `.fill`, `.fill2`, `.play`, `.leaf` to `.leaf4`, `.lexp`, `.mrg`, `.err`, `.dep` | A book saved by a book command itself (an ordinary book file) | [6.6](06-book-basics.md) |
| `book.dat.tmp.number` | The temporary file of a save in progress (if one is left over, it can be deleted) | [13.3](13-troubleshooting.md) |
| `book.dat.damaged` (`.damaged.1`, …) | A book file that could not be read, kept instead of being overwritten | [13.1](13-troubleshooting.md) |
| `book.dat.old` (`.old.1`, …) | The book file from before `book new`, `book load` of another file or `book import`, kept instead of being overwritten | [6.6](06-book-basics.md), [6.7](06-book-basics.md) |
| `.txt`, `.sgf`, `.ggf`, `.pgn`, `.wtb`, `.edx` | Games | [10.1](10-files.md) |
| `.eps`, `.svg` | A picture of the board | [3.6](03-playing.md) |
| `.obf` | A problem set | [10.3](10-files.md) |
| `config.ini`, `edax.ini` | Settings | [chapter 4](04-settings.md) |

## 14.7 Quick table of levels

| Level | Midgame reading | Exact solving from |
|---|---|---|
| 1 to 10 | level moves (100%) | level×2 empties or fewer |
| 11 to 18 | level moves (73%) | 21 or fewer |
| 19 to 24 | level moves (73%) | 24 or fewer |
| 25 to 29 | level moves (73%) | 27 or fewer |
| 30 to 35 | level moves (73%) | 30 or fewer |
| 36 to 59 | level moves (73%) | level−6 or fewer |
| 60 | — | always |

The detailed table is in [5.2](05-search.md).

Next: [15. Glossary](15-glossary.md)
