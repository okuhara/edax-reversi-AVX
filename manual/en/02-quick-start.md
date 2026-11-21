# 2. Starting and playing a first game

[Contents](README.md) | Previous: [1. Introduction](01-introduction.md) | Next: [3. Commands for playing and analysing](03-playing.md)

This chapter covers everything from starting Edax to playing a game against it and quitting.

## 2.1 Preparation

1. Download the ZIP file from the Releases page (its name looks like `edax-4.5.5-nikque.19.zip`) and extract it to any folder.
   - A folder with a short name and no spaces or non-ASCII characters (for example `C:\edax`) is recommended.
   - The reason: when a book is saved, a path that is too long can make the save fail ([chapter 13](13-troubleshooting.md)).
2. Open the `bin` folder of the extracted package. Check that it contains the executables, `config.ini` and the `data` folder (with `eval.dat` and `book.dat`).

**Important: start Edax from inside the `bin` folder.** At startup, Edax looks for `data/eval.dat` relative to the *current folder*. If you start it from another folder, it prints this message and exits:

```
Cannot open data/eval.dat
```

## 2.2 Starting

### Windows

Use either method:

- **From File Explorer**: double-click `wEdax-x86-64.exe` in the `bin` folder. A (black) console window opens.
- **From a terminal**:
  1. Open the `bin` folder in File Explorer.
  2. Type `powershell` in the address bar and press Enter. A terminal opens in that folder.
  3. Type:

     ```
     .\wEdax-x86-64.exe
     ```

The checks for this manual were made by starting Edax from a terminal.

### Linux and macOS

In a terminal, change to `bin` and run the executable. If the system refuses to run it right after extraction, make it executable.

```
cd edax/bin
chmod +x lEdax-x86-64
./lEdax-x86-64
```

Starting the Linux or macOS executables from the package ZIP was not tested for this manual. The behaviour of the Linux version was checked with builds from the source.

## 2.3 The first screen

When Edax starts, it shows the board and the prompt `>`.

```
  A B C D E F G H            BLACK            A  B  C  D  E  F  G  H
1 - - - - - - - - 1         0:00.000       1 |  |  |  |  |  |  |  |  | 1
2 - - - - - - - - 2    2 discs   4 moves   2 |  |  |  |  |  |  |  |  | 2
3 - - - . - - - - 3                        3 |  |  |  |  |  |  |  |  | 3
4 - - . O * - - - 4  ply  1 (60 empties)   4 |  |  |  |()|##|  |  |  | 4
5 - - - * O . - - 5    Black's turn (*)    5 |  |  |  |##|()|  |  |  | 5
6 - - - - . - - - 6                        6 |  |  |  |  |  |  |  |  | 6
7 - - - - - - - - 7    2 discs   4 moves   7 |  |  |  |  |  |  |  |  | 7
8 - - - - - - - - 8         0:00.000       8 |  |  |  |  |  |  |  |  | 8
  A B C D E F G H            WHITE            A  B  C  D  E  F  G  H

>
```

**The board on the left** is the current position.

| Character | Meaning |
|---|---|
| `*` | Black disc |
| `O` | White disc |
| `.` | A square where the side to move can play now |
| `-` | Empty square |

**The middle column** shows information about Black (upper half) and White (lower half).

| Text | Meaning |
|---|---|
| `0:00.000` | Time used by that colour (minutes:seconds) |
| `2 discs` | Number of discs of that colour |
| `4 moves` | Number of squares that colour can play |
| `ply  1 (60 empties)` | The number of the next move (the 1st) and the number of empty squares (60) |
| `Black's turn (*)` | The side to move (Black). With White to move: `White's turn (O)` |

**The board on the right** shows the order of the moves:

- the four starting discs are shown as `##` (black) and `()` (white);
- every other disc shows the number of the move that placed it.

When `>` is shown, Edax is waiting for input. Type a command and press Enter. Edax runs it, then shows the board and `>` again.

## 2.4 Playing against Edax

Right after startup, Edax **does not play any moves itself**: two human players take turns. To play against Edax, first use the `mode` command to tell it which side to play.

| Input | Meaning |
|---|---|
| `mode 0` | You are Black (first to move), Edax is White |
| `mode 1` | Edax is Black (first to move), you are White |
| `mode 2` | Edax plays both sides to the end (you watch) |
| `mode 3` | Edax does not play by itself (the state just after starting) |

In this example you play Black: type `mode 0`, then the square you want to play (here `f5`).

```
>mode 0
```

```
>f5

You play F5

  A B C D E F G H            BLACK            A  B  C  D  E  F  G  H
1 - - - - - - - - 1         0:01.140       1 |  |  |  |  |  |  |  |  | 1
2 - - - - - - - - 2    4 discs   3 moves   2 |  |  |  |  |  |  |  |  | 2
3 - - - - - - - - 3                        3 |  |  |  |  |  |  |  |  | 3
4 - - - O * . - - 4  ply  2 (59 empties)   4 |  |  |  |()|##|  |  |  | 4
5 - - - * * * - - 5    White's turn (O)    5 |  |  |  |##|()| 1|  |  | 5
6 - - - . - . - - 6                        6 |  |  |  |  |  |  |  |  | 6
7 - - - - - - - - 7    1 discs   3 moves   7 |  |  |  |  |  |  |  |  | 7
8 - - - - - - - - 8         0:00.000       8 |  |  |  |  |  |  |  |  | 8
  A B C D E F G H            WHITE            A  B  C  D  E  F  G  H


 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
18@73%  +00        0:00.016        245058   15316125 D6 c3 D3 c4 F4 c5 B3 c2
------+-----+--------------+-------------+----------+----------------------

Edax plays D6

  A B C D E F G H            BLACK            A  B  C  D  E  F  G  H
1 - - - - - - - - 1         0:01.140       1 |  |  |  |  |  |  |  |  | 1
2 - - - - - - - - 2    3 discs   5 moves   2 |  |  |  |  |  |  |  |  | 2
3 - - . - - - - - 3                        3 |  |  |  |  |  |  |  |  | 3
4 - - . O * - - - 4  ply  3 (58 empties)   4 |  |  |  |()|##|  |  |  | 4
5 - - . O * * - - 5    Black's turn (*)    5 |  |  |  |##|()| 1|  |  | 5
6 - - . O - - - - 6                        6 |  |  |  | 2|  |  |  |  | 6
7 - - . - - - - - 7    3 discs   4 moves   7 |  |  |  |  |  |  |  |  | 7
8 - - - - - - - - 8         0:00.016       8 |  |  |  |  |  |  |  |  | 8
  A B C D E F G H            WHITE            A  B  C  D  E  F  G  H

>
```

- `You play F5`: your move was accepted.
- The table in the middle (`depth|score|…`) shows Edax's search. See 2.5 below and [chapter 5](05-search.md) for how to read it.
- `Edax plays D6`: Edax played D6.
- `>` appears again: it is your turn. This continues until the game ends.

If you type a square that cannot be played, or an unknown word, Edax prints the following and the position does not change:

```
WARNING: Unknown command/Illegal move: "e3 "
```

## 2.5 Hints and taking back

**Hint**: `hint 3` shows the three best moves in the current position with their scores. Without a number, it shows one move.

```
>hint 3

 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
18@73%  +00        0:00.110      15578834  141625764 c3 D3 c4 F4 c5 B3 c2 E3
18@73%  -01        0:00.000         15137            c4 F4 f6 G5 f3 D3 c3 G6
18@73%  -02        0:00.000          3239            c5 F4 e3 C6 d3 F6 e6 D7
------+-----+--------------+-------------+----------+----------------------
```

Each line is one move. The two most important columns are:

- `score`: the score after that move, that is, the expected **disc difference from the side to move's point of view**. Above, c3 is `+00` (a draw is expected), c4 is `-01` (a loss by one disc) and c5 is `-02`.
- `principal variation`: the expected line of play from that move, with best play by both sides. Its first move is the move this line is about. **In this column, Black's moves are lower case and White's moves are upper case.**

`18@73%` in the `depth` column means "searched 18 moves ahead with 73% confidence" ([chapter 5](05-search.md)).

**Taking back**: `undo` (or `u`) takes back a move.

- In `mode 0` or `mode 1` (when Edax is your opponent), it takes back **two moves: yours and Edax's**.
- If you went back too far, `redo` (or `r`) replays the moves.

## 2.6 Passing and the end of the game

**Pass**: if it is your turn and you have no legal move (the middle column shows `0 moves`), type `ps` to pass.

```
>ps

You play PS
```

When Edax has to pass, it passes automatically.

**End of the game**: when neither side can move, the middle column shows `Game over` and the winner, and `*** Game Over ***` is printed below the board.

```
  A B C D E F G H            BLACK            A  B  C  D  E  F  G  H
1 * O O O O O O O 1         0:00.750       1 |55|56|47|40|48|45|53|54| 1
2 * O * * * * * * 2   25 discs   0 moves   2 |59|46|39|34|16|41|58|57| 2
3 O O O O * O O O 3                        3 |60|38|33|10| 3|11|30|42| 3
4 O O * * O * O O 4       Game over        4 |32|18|12|()|##| 2| 9|43| 4
5 O O * O * O O O 5       White won        5 |31|15| 6|##|()| 1|17|27| 5
6 O * O * O O O O 6                        6 |28|29| 7| 8| 5| 4|26|44| 6
7 * * * O O O O O 7   39 discs   0 moves   7 |37|35|24|14|13|19|49|52| 7
8 O * * * * * * O 8         0:00.000       8 |36|25|21|22|20|23|51|50| 8
  A B C D E F G H            WHITE            A  B  C  D  E  F  G  H

*** Game Over ***
```

In this example White wins, 39 discs to 25.

## 2.7 Another game, and quitting

| Input | Meaning |
|---|---|
| `init` (or `i`) | Start a new game from the usual initial position |
| `quit` (or `q`, `exit`) | End Edax |

On exit, Edax saves the book to its file (`data/book.dat`) if the book has changed. Playing games does not change the book ([chapter 6](06-book-basics.md)).

## 2.8 Changing the strength

Edax's strength is set by the **level** (0 to 60). The package sets it to 18. A lower number makes Edax weaker; a higher number makes it stronger.

```
>level 5
```

- The higher the level, the deeper Edax searches and the longer it takes. [Chapter 5](05-search.md) explains what a level means.
- Nothing is printed when the level changes. The `options` command shows the current value (the `search level` line).
- To use the same level every time Edax starts, change the number in `level = 18` in `config.ini` ([chapter 4](04-settings.md)).

## 2.9 Common problems

| What happens | Cause and what to do |
|---|---|
| `Cannot open data/eval.dat`, and Edax ends at once | Edax was not started from inside the `bin` folder. Go to `bin`, then start it |
| You play a move but Edax does not answer | The mode is still `mode 3` (the state just after starting). Type `mode 0` or `mode 1`. To make Edax play just one move now: `go` |
| `Unknown command/Illegal move` | A square that cannot be played, or a spelling mistake. The playable squares are the `.` of the left board |
| No square can be played and nothing moves on | Type `ps` to pass |
| Edax thinks and does not come back | The level is too high. Type `stop`: Edax stops thinking and plays the move it has found so far. After that the mode is `mode 3` (Edax does not play by itself), so lower the `level`, then type `mode 0` or `mode 1` again |
| The screen scrolls too much | The board is printed after every command. `verbose 0` stops the board and the search table (`verbose 1` brings them back) |

Next: [3. Commands for playing and analysing](03-playing.md)
