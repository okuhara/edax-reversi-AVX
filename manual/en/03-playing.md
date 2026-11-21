# 3. Commands for playing and analysing

[Contents](README.md) | Previous: [2. Starting and playing a first game](02-quick-start.md) | Next: [4. Settings](04-settings.md)

This chapter explains the commands for playing and analysing games.

- The book commands (`book …`) are in chapters [6](06-book-basics.md) to [8](08-book-maintenance.md).
- The commands for files of many games (`base …`) are in [chapter 10](10-files.md).

Type a command after `>` and press Enter. Many commands have a short form (`u` for `undo`, and so on).

`help` (or `?`) prints Edax's built-in command list (in English). Up to v4.5.5-nikque.12, some of the "default" values in that list were those of old versions. The correct defaults are in chapters [4](04-settings.md) and [14](14-reference.md).

## 3.1 Playing moves

| Input | Meaning |
|---|---|
| `f5` (the name of a square) | Play that square. Upper or lower case |
| `ps` (or `pa`, `pass`) | Pass (only when no square can be played) |
| `play f5d6c3d3c4` | Play a sequence of moves. Spaces between the moves are allowed. Passes need not be written: they are inserted where needed |
| `play tiger` | Play the moves of an opening given by its (English) name |
| `go` | Make Edax play one move for the side to move |
| `stop` | Stop Edax while it thinks (it plays the move found so far, and the mode becomes `mode 3`) |

`play` continues from the current position. To start again from the initial position, type `init` first.

**Opening names**: as in `play tiger`, you can give `play` the English name of any opening that Edax knows. Conversely, `opening` (English) and `ouverture` (French) print the name of the opening of the current position. If the position has no name, they print `?`.

```
>play tiger
>opening
tiger
>ouverture
tigre
```

## 3.2 Who plays: mode

| Input | Black | White |
|---|---|---|
| `mode 0` | You | Edax |
| `mode 1` | Edax | You |
| `mode 2` | Edax | Edax |
| `mode 3` (at start) | You | You |

- `mode 2` makes Edax play against itself from the current position to the end of the game.
- In `mode 3`, Edax does not play by itself. This mode is useful for setting up positions and examining them with `hint`, or for making Edax play one move at a time with `go`.
- `m` is the short form of `mode`. Without a number, it means `mode 3`.

## 3.3 Back, forward, again

| Input | Meaning |
|---|---|
| `undo` (`u`) | Take back one move. With `mode 0` or `mode 1`, two moves (yours and Edax's) |
| `redo` (`r`) | Play again a move that was taken back (two moves with `mode 0` or `mode 1`) |
| `init` (`i`) | Set the usual initial position and start a new game |
| `new` (`n`) | Start a new game from the *first position* of the current game. Use it to play again from a position made with `setboard` or from the first position of a loaded game |

## 3.4 Examining moves: hint and analyze

### hint: the best moves in the current position

```
>hint 3
```

This shows the three best moves in the current position, best first. The number can be 1 to 60 (1 if omitted). If the number is larger than the number of legal moves, all moves are shown. For how to read the table, see [2.5](02-quick-start.md) and [chapter 5](05-search.md).

- `hint` does not change the position.
- It searches at the current `level`.
- When the book is in use and contains the current position, the book moves are shown first (lines with `book` in the `depth` column; [6.5](06-book-basics.md)).

### analyze: reviewing a finished game

```
>analyze 6
```

This examines **the last 6 moves** of the current game, one at a time from the end. For each, it compares the move that was played with the best alternative. Without a number, it examines all moves.

The search uses the current `level`. Analysing many moves takes a long time.

```
              N     played        alternative
ply  level   alt. move  score     score   move
---+-------+-----+-----------+--+---------------
 60    0      0    a3    +14
 59    2      1    A2    -14   >   -26     A3
 58    2      0    g2    +14
 57    4      3    H2    -14   >   -24     G2
 56    2      2    b1    +14   >    +5     g2
 55    2      5    A1    -14   >   -16     H2

      | rejections : discs | errors    : discs | error rate |
Black |   0 /   1  :    +0 |   0 /   2 :    +0 |      0.000 |
White |   0 /   1  :    +0 |   0 /   0 :    +0 |          - |
```

| Column | Meaning |
|---|---|
| `ply` | The number of the move |
| `level` | The depth at which that move was examined (as `18@73%`; [chapter 5](05-search.md)) |
| `N alt.` | The number of other moves that could be played |
| `move` and `score` under `played` | The move that was played, and its score (in this column Black's moves are in upper case, White's in lower case) |
| The sign in the middle | `>`: the played move is better; `=`: the same; `<`: another move is better |
| `score` and `move` under `alternative` | The best of the other moves, and its score |

When another move was better, the line ends with one of these:

| Text | Meaning |
|---|---|
| `<- Mistake` | A bad move, confirmed by exact solving |
| `<- Possible mistake` | Judged by a search that read to the end but with a confidence below 100% |
| `<- Edax disagrees` (`strongly` if the difference is 5 discs or more) | Judged by a midgame search (not certain) |

The two lines at the bottom summarise the results for Black and White:

- `rejections`: how many times a non-exact search found a better alternative (out of the moves examined that way), and the total number of discs lost.
- `errors`: the number of bad moves confirmed by exact solving (out of the moves examined exactly), and the total number of discs lost.
- `error rate`: the discs lost per move examined exactly. If no move of a colour was examined exactly, it shows a dash, `-`. Up to v4.5.5-nikque.12, it showed a non-numeric value such as `-nan(ind)`.

**Reviewing with the book**: `book analyze 6` does the same using the book's scores instead of a search. It works only for positions that are in the book ([chapter 6](06-book-basics.md)).

## 3.5 Setting up a position: setboard

To start from any position, give `setboard` the 64 squares of the board and the side to move.

```
>setboard -------- -------- -------- ---OX--- ---XO--- -------- -------- -------- X
```

- Write the 64 squares in the order A1, B1, …, H1, A2, …, H8.
- A black disc is `X` (or `*`, `b`), a white disc is `O` (or `w`), and an empty square is `-` (or `.`).
- Other characters (such as spaces) are ignored. So you can put a space after every 8 squares, as above.
- After the 64 squares, write the side to move as one character: `X` for Black, `O` for White.
- **If fewer than 64 squares are given, the side-to-move character is read as a square.** Edax then prints `WARNING: board_set: bad string input` and picks the side to move itself: Black if the number of empty squares is even, White if it is odd. Count carefully.

After `setboard`, that position becomes the *first position* of the game. `new` restarts from it; `init` returns to the usual initial position.

### Mirroring and rotating the board

These commands transform the current game (including its moves) into a symmetric one.

| Input | Meaning |
|---|---|
| `vmirror` | Swap top and bottom |
| `hmirror` | Swap left and right |
| `rotate 90` (`180`, `270`) | Rotate |
| `symetry n` (`n` from 0 to 15) | Choose one of the 8 symmetries by its number. From 8 up, the side to move is swapped as well |

## 3.6 Saving and loading a game

```
>save mygame.txt
>load mygame.txt
```

`save` (`s`) saves the current game. `load` (`o`, `open`) loads a game and goes to its last position. **The format is determined by the file name extension.**

| Extension | Format | Save | Load |
|---|---|---|---|
| `.txt` | Text: the moves on one line (`F5D6C3D3C4`) | yes | yes |
| `.sgf` | SGF | yes | yes |
| `.ggf` | GGF (the game format of GGS) | yes | yes |
| `.pgn` | PGN | yes | yes |
| `.edx` | Edax's own binary format | yes | yes |
| `.eps`, `.svg` | A picture of the board | yes | no |

- The file name is relative to the current folder (`bin`). You can include a folder, as in `save games/mygame.txt`; create the folder first.
- With an unknown extension, Edax prints `WARNING: Unknown game format extension: .abc` and saves nothing.
- When loading fails, Edax prints the reason and the position does not change. Examples: `WARNING: Cannot open file nofile.txt`, `WARNING: Unknown game format extension: .abc`, `WARNING: Illegal move #2: A1`. Up to v4.5.5-nikque.12, nothing was printed.
- **Save a game that started from a custom position (after `setboard`) as `.ggf`, `.pgn` or `.edx`.** `.txt` and `.sgf` files can be written, but they cannot be loaded back ([10.1](10-files.md)).
- A game that contains an illegal move is not loaded, and the board stays as it was before the load.
  - With `.ggf`, `.sgf` and `.pgn`, a warning such as `WARNING: error while importing a GGF game` is printed.
  - `.txt` is the exception: a warning is printed and the moves before the illegal one are loaded.
  - Up to v4.5.5-nikque.12, a move that flips no disc was placed on the board anyway, and the game was loaded ([10.1](10-files.md)).
- The game formats are described in [chapter 10](10-files.md).

## 3.7 Making Edax play a given opening: force

```
>force f5d6c3
```

From then on, as long as the game follows this line (or a symmetric one), Edax plays the moves of the line on its turn. When the line ends, or when the opponent leaves it, Edax searches and plays as usual. You can also give an opening name (`force tiger`).

- The line starts from the *first position* of the current game.
- `init` resets the forced line to "the first move is F5". This is also the state right after startup. The four first moves are equivalent by symmetry, so Edax plays F5 as Black's first move without searching.
- `setboard` removes the forced line.

## 3.8 Strength and time

| Input | Meaning |
|---|---|
| `level 12` | Set the strength of the search to 12 (0 to 60; [chapter 5](05-search.md)) |
| `game-time 5:00` | Give each side 5 minutes for the game. Edax decides how long to think from the time it has left |
| `move-time 10` | 10 seconds for each move |
| `ponder on` / `ponder off` | Think during the opponent's turn or not (on by default) |

- A time is written as `seconds`, `minutes:seconds` or `hours:minutes:seconds` (`30`, `5:00`, `1:30:00`).
- With `game-time` or `move-time`, Edax's search is limited by **time**, not by level.
  - If **you set a level yourself** (with the `level` command or `-l` on the command line), that level is the maximum search depth.
  - Otherwise, Edax searches as deep as the time allows. A `level` in `config.ini` does not act as a maximum.
- Typing `level` switches back from time-based to level-based search.
- If you change `game-time` during a game, the new time applies from the next game (`init`, `new`).
- To set it at startup, use `-t 5:00` (the same as `-game-time`) or `-move-time 10` on the command line.

Example: Edax started with 20 seconds for the game (part of the `options` output):

```
	search alloted time: 0:20.000
	search with: fixed time per game
```

## 3.9 Playing many games in a row

These settings control what happens when a game ends. All are `off` by default. Type, for example, `auto-start on`, or give `-auto-start on` at startup.

| Setting | What happens when a game ends |
|---|---|
| `auto-start on` | A new game starts by itself |
| `auto-swap on` | Edax takes the other colour (`mode 0` ↔ `mode 1`) |
| `auto-store on` | The game is added to the book (as `book store`; [chapter 7](07-book-learning.md)) |
| `auto-quit on` | Edax ends |
| `repeat n` | Play n games (`-repeat n` at start) |

The current values appear at the very end of the `options` output, under `Game play`. Below is the output after starting with `-auto-store on -auto-swap on`. From top to bottom, the lines are `mode`, `auto-start`, `auto-store`, `auto-swap`, `auto-quit` and `repeat`.

```
Game play
	mode: human/human
	start a new game after a game is over: false
	store each played game in the opening book: true
	change computer's side after each game: true
	quit when game is over: false
	repeat 0 games (before exiting)
```

- The `mode:` line shows **the startup value** (`-mode`, or `mode` in `config.ini`). Typing `mode 0` or similar while running does not change this line. You can tell the current mode by whether Edax plays by itself.
- Up to v4.5.5-nikque.12, the `auto-store`, `auto-swap` and `auto-quit` lines all showed the value of `auto-start`. This was only a display bug: the behaviour followed the actual settings.

## 3.10 Changing what is displayed

| Input | Meaning |
|---|---|
| `verbose 0` | Do not show the board and the search table (`You play …` and `Edax plays …` are still shown). Same as `-q` at start |
| `verbose 1` | The usual display (default) |
| `verbose 2` | Also show the progress of the search (one line each time the depth increases). Same as `-vv` at start |
| `noise n` | Do not show the progress lines of the search for depths below n |
| `width n` | Width of the search table in characters (80 by default) |
| `options` | Show all the current settings |
| `version` (`v`) | Show the version |

## 3.11 The commands of this chapter

| Command | Short name | Meaning |
|---|---|---|
| `help` | `?` | The built-in help (English). `help options`, `help commands`, `help book`, `help base`, `help test` show one part |
| `init` | `i` | New game from the initial position |
| `new` | `n` | New game from the current *first position* |
| `mode n` | `m` | Who plays (3.2) |
| (a square), `ps` | | Play a move, pass |
| `play moves` | | Play a sequence of moves |
| `force moves` | | Make Edax play a given opening |
| `go` | | Make Edax play one move |
| `stop` | | Stop thinking |
| `hint n` | | Show the n best moves |
| `undo`, `redo` | `u`, `r` | Back, forward |
| `analyze n` | `a` | Look back at the last n moves |
| `setboard board side` | | Set up a position |
| `vmirror`, `hmirror`, `rotate`, `symetry` | | Move the board to a symmetric one |
| `save file`, `load file` | `s`, `o` (`open`) | Save or load a game |
| `opening`, `ouverture` | | Name of the opening (English, French) |
| `options` | | Show the settings |
| `version` | `v` | Show the version |
| `quit` | `q`, `exit` | End |

Next: [4. Settings](04-settings.md)
