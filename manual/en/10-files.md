# 10. Game and problem files

[Contents](README.md) | Previous: [9. Large books](09-large-books.md) | Next: [11. Solving problems and measuring speed](11-solve-bench.md)

## 10.1 Game formats

Edax determines the format of a game file from **the file name extension**.

| Extension | Format | One game: save and load (`save`, `load`) | Files of many games (`book add`, `base …`) |
|---|---|---|---|
| `.txt` | Text. One game per line | yes | yes |
| `.ggf` | GGF (the format of the internet game server GGS) | yes | yes |
| `.sgf` | SGF | yes | yes |
| `.pgn` | PGN | yes | yes |
| `.wtb` | WTHOR database | no | yes |
| `.edx` | Edax's own binary format | yes | yes |
| `.eps`, `.svg` | A picture of the board (written by `save` only) | save only | no |

### The text format (.txt)

The simplest format, and convenient to write by hand. Each line is one game, with the moves written one after another.

```
F5D6C3D3C4F4C5B3C2
f5f6e6f4e3c5c4e7c6e2
```

- Upper or lower case is accepted. Edax writes upper case.
- Passes are not written; they are inserted where needed.
- When a game that does not start from the usual initial position (a game after `setboard`) is saved as `.txt`, the first position is written before the moves, as `board side;`.

  ```
  -------------------O-------OO------XO--------------------------- X;D2C3
  ```

  **However, a line in this form cannot be loaded back.**
  - A `.txt` file is always read as moves from the usual initial position.
  - Loading this line prints the warning `WARNING: game text: … is not a move that can be played here` and reads no moves.
  - The result is a game with no moves at the usual initial position, and the game in progress is lost.

  To keep a game that starts from a set-up position, use one of the formats in the table below.

### Saving a game that starts from a set-up position

The same game (2 moves from a position set up with `setboard`) was saved in the 5 formats and loaded back, with these results:

| Format | Result of loading back |
|---|---|
| `.ggf`, `.pgn`, `.edx` | The first position and the moves, as they were |
| `.txt` | **Cannot be read back.** No move is read; the result is a game without moves at the usual initial position (a warning is printed) |
| `.sgf` | **Cannot be read back.** `WARNING: error while importing a SGF game` is printed and nothing is loaded (the board stays as it was before `load`) |

A game that starts from the usual initial position can be loaded back in every format.

Up to v4.5.5-nikque.12, loading this `.sgf` printed the same warning but then left an incorrect board: the moves were placed on the usual initial position without flipping any discs.

### Examples of the other formats

The same 5-move game (`F5D6C3D3C4`), saved in each format.

SGF:

```
(;FF[4]GM[2]AP[edax:4.5.5]
PC[Edax]DT[2026-10-07]
PB[?]PW[?]
SZ[8]AB[e4][d5]AW[d4][e5]PL[B]
RE[Void]
B[f5];W[d6];
B[c3];W[d3];
B[c4];)
```

GGF:

```
(;GM[othello]PC[Edax]PB[?]PW[?]RE[-127.000]BO[8 -------- -------- -------- ---O*--- ---*O--- -------- -------- -------- *]B[F5]W[D6]B[C3]W[D3]B[C4];)
```

- In SGF a pass is written `B[PA]` or `W[PA]`. Since v4.5.5-nikque.13 an SGF file with passes can be loaded back; earlier versions failed to load it.
- **A game file with an illegal move** (a move that flips no discs, or a move on an occupied square):
  - With `.ggf`, `.sgf`, `.pgn` or `.edx`, the game is not loaded. The board and the game stay as they were before `load`.
  - The illegal move is reported, as in `WARNING: Illegal move #2: A1`. `#2` is the move number, counted from 0.
  - With `.ggf`, `.sgf` and `.pgn`, a warning such as `WARNING: error while importing a GGF game` is printed before it.
  - `.txt` alone behaves differently: the moves before the illegal one are loaded, and `WARNING: game text: "A1F4" is not a move that can be played here: the rest of the line is not read` is printed. The same happens when `book add` reads the file.
  - Up to v4.5.5-nikque.12, a move that flips no discs was placed on the board without flipping anything, and the game was loaded. The result is a board that cannot occur in a real game. With an old version, if a warning is printed during a load, do not use that board: start again with `init`. Old versions also give no warning when a `.txt` line is only partly read.
- Up to v4.5.5-nikque.12, loading a `.pgn` of an unfinished game prints `WARNING: uncomplete game.` (the game is still loaded).
- A PGN file is read up to 60 moves.
- A saved game holds at most 60 moves.

## 10.2 Files of many games: the base commands

The commands that start with `base` process game files (containing many games) as a whole. They do not involve the current game or the book.

| Command | Meaning |
|---|---|
| `base convert input output` | Change the format (the extensions give the formats) |
| `base unique input output` | Keep one copy of each game |
| `base compare file1 file2` | Show the number of positions that the two files have in common |
| `base check file n` | Examine the endgame of each game by exact solving (from the positions with n or fewer empty squares; 24 if left out) and show the games that hold bad moves |
| `base correct file n` | Examine in the same way and replace the bad moves with the best ones. **The file is rewritten** |
| `base complete file` | Edax plays the unfinished games on to their end. **The file is rewritten** |
| `base problem file n output` | Write the position with n empty squares of each game to a problem file |
| `base tofen file n output` | Write the same positions in the notation called FEN |

The word after `base` may be written in upper case.

Examples:

```
>base convert games.txt games.sgf
WARNING: Cannot open file games.sgf

>base unique games.txt uniq.txt
WARNING: Cannot open file uniq.txt

>base compare games.txt uniq.txt
games.txt : 24 positions - 0 original positions
uniq.txt : 24 positions - 0 original positions
24 common positions

>base check full.txt 12
F5F4E3F6E6C5C6D6G4D3F3C4E7D7B5E2G5B4F7E8C8D8F8C7B8G6H5A6B6G3A5A4C3D2B7A8A7B3C2D1F2H3H4H6F1B2C1E1G7H8G8H7G1H1A1B1H2G2A2A3
Game #0 contains 3 errors
1/1 100.0 % done.
```

**Notes**

- **If the output file already exists, the games are appended to it** (it is not overwritten). Running the same command twice with the same name doubles the games. To create the file from scratch, delete the old one first or use another name.
- When the output is a new file, `WARNING: Cannot open file …` is printed, as in the examples above.
  - It means that Edax tried to read the existing file before appending to it, and there was none.
  - **The file is created.**
  - The same message appears after `base correct` and `base complete`.
- `base check` and `base correct` solve every game exactly. With a large n they take a very long time.
- `base complete` plays on at the `level` setting (since v4.5.5-nikque.13). Earlier versions kept the depth of the last search, so they either tried to solve to the end and never finished, or played nonsense moves. The search table is printed for every move.
- `base correct` and `base complete` rewrite the file only when it could be read. A file that cannot be read is left unchanged.
- While a book or `base` command is running, `stop` does not stop it ([7.5](07-book-learning.md)).

## 10.3 Problem files (OBF)

These files give Edax positions to solve ([chapter 11](11-solve-bench.md)). The extension is `.obf`. Each line is one position.

```
--XXXXX--OOOXX-O-OOOXXOX-OXOXOXXOXXXOXXX--XOXOXX-XXXOOO--OOOOO-- X; G8:+18; H1:+12; H7:+6; A2:+6;
```

- First comes the board (64 squares; `X` black, `O` white, `-` empty), then the side to move (`X` or `O`), then `;`.
- The `move:score;` items that follow are known answers (the disc difference after that move). They are optional. When they are present, Edax compares its own answer with them.
- Everything from `%` to the end of the line is a comment and is ignored.

The `problem` folder of the package holds endgame problem sets: 4 files such as `fforum-1-19.obf`, with problems published in the bulletin of the French Othello federation.

`base problem` and `book problem` ([8.8](08-book-maintenance.md)) write positions in this format.

Next: [11. Solving problems and measuring speed](11-solve-bench.md)
