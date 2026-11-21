# 11. Solving problems and measuring speed

[Contents](README.md) | Previous: [10. Game and problem files](10-files.md) | Next: [12. Using Edax from other programs](12-integration.md)

## 11.1 Solving a problem set: -solve

Edax searches the positions of a problem file (OBF; [10.3](10-files.md)) in order and shows the results as a table. When given as a startup option, Edax exits once the problems are solved.

```
wEdax-x86-64.exe -solve ..\problem\fforum-1-19.obf
```

This is the output for the first 3 problems of the set in the package.

```
 # | depth|score|       time   |  nodes (N)  |   N/s    | principal variation
---+------+-----+--------------+-------------+----------+---------------------
  1|   14   +18        0:00.000         76954            g8 H7 a8 A6 a4 A7 b6
  2|   14   +10        0:00.000         33373            a4 B7 a3 A2 b8 A7 g7
  3|   14   +02        0:00.000        151540            d1 G1 b8 C1 g3 A8 g2
---+------+-----+--------------+-------------+----------+---------------------
three.obf: 261867 nodes in  0:00.000
```

- Each line is one problem. The columns are those of the search table ([5.1](05-search.md)).
- When `depth` equals the number of empty squares and has no `@`, the result is an exact solution. The 3 problems above have 14 empty squares.
- The last line gives the total nodes and time for all the problems.
- **The search strength is set by `level`** (18 in the package: positions with 21 or fewer empty squares are solved exactly). To solve problems with more empty squares exactly, add `-l 60`.
- Options often used with it:
  - `-n 1`: 1 thread (the result and the node count are the same every time)
  - `-l 60`
  - `-h 24`: a larger hash table
  - `-q`: no table, only the last line
  - `-vv`: one line each time the depth increases

  ```
  wEdax-x86-64.exe -solve ..\problem\fforum-40-59.obf -l 60 -n 8 -h 24
  ```
- When the problem file contains answers (`move:score;`), Edax prints `Erroneous move: D1 expected, with score +2, …` at the right of the line **if the move it found is not the best move among the answers**. This can happen when solving at a level that is not exact. The example below is a problem with 14 empty squares solved at level 6.

  ```
    3|    6   +08        0:00.000         13193            g3 G1 b8 G2          Erroneous move: D1 expected, with score +2, error = …
  ```

In a running Edax, `solve file` does the same.

```
>solve ..\problem\fforum-1-19.obf
```

The problem sets in the package (folder `problem`):

| File | Content |
|---|---|
| `fforum-1-19.obf` | Endgame problems 1 to 19 |
| `fforum-20-39.obf` | Problems 20 to 39 |
| `fforum-40-59.obf` | Problems 40 to 59 |
| `fforum-60-79.obf` | Problems 60 to 79 |

The numbers of empty squares are:

- 14 to 16 in `fforum-1-19`
- up to 26 in `fforum-20-39`
- 20 to 34 in `fforum-40-59`
- up to 36 in `fforum-60-79`

The more empty squares, the longer exact solving takes. Solving the last two files with `-l 60` takes a long time (the time was not measured).

## 11.2 Measuring speed: bench

Edax solves a set of built-in endgame positions and shows the speed. This gives an idea of a PC's speed and lets you compare the executable variants (standard, `v3`, `v4`).

```
wEdax-x86-64.exe -bench 2 -n 2
```

```
 # | depth|score|       time   |  nodes (N)  |   N/s    | principal variation
---+------+-----+--------------+-------------+----------+---------------------
  1|   20   +20        0:00.047       3563782   75825149 a4 C8 a8 D7 c7 G2 a2
  2|   20   -02        0:00.078       8479888  108716513 h7 E6 f8 F7 h6 H3 e8
---+------+-----+--------------+-------------+----------+---------------------
2 positions solved: 12043670 nodes in  0:00.125 (96349360 nodes/s).
```

- In `-bench n`, n is the number of positions to solve. In a running Edax, use `bench n` (all the positions if n is omitted).
- `nodes/s` in the last line is the number of nodes per second.
- With few positions, the time is too short and the figures are unstable. To compare, measure several times with the same n and the same `-n` (number of threads). Other running programs slow it down.

## 11.3 Counting positions and lines

These commands count the lines or positions a few moves ahead of the current position. They are for testing the program and are not needed in normal use.

| Command | Meaning |
|---|---|
| `count games d` | The number of lines down to d moves ahead (counted fast, with a hash table) |
| `perft d` | The same count, without the hash table |
| `count positions d` | The number of different positions down to d moves ahead |
| `count shapes d` | The number of shapes of discs (colours not distinguished) down to d moves ahead |
| `estimate n` | Estimate the total number of lines by playing on at random n times |

An example down to 6 moves from the initial position (4 lines after the first move, 12 after the second, and so on):

```
>count games 6
  ply           moves        passes          wins         draws        losses    mobility        time   speed
   1,               4,            0,            0,            0,            0,    4 -  4,        0:00.000,  …
   2,              12,            0,            0,            0,            0,    3 -  3,        0:00.000,  …
   3,              56,            0,            0,            0,            0,    4 -  5,        0:00.000,  …
   4,             244,            0,            0,            0,            0,    2 -  6,        0:00.000,  …
   5,            1396,            0,            0,            0,            0,    3 -  9,        0:00.000,  …
   6,            8200,            0,            0,            0,            0,    1 - 11,        0:00.000,  …
```

The time grows very quickly as d increases. The startup option `-count games 10` also counts.

## 11.4 Other testing commands

The following commands also exist, but they were not run or checked for this manual. They are listed for completeness.

| Command | The built-in help (summary) |
|---|---|
| `microbench` | Measure the speed of the main functions, in CPU cycles |
| `script-to-obf input output` | Convert a problem file of the "script" format to OBF |
| `select-hard input output` | Select the hard problems of a problem set |
| `wtest file` (`-wtest`) | Check the theoretical values of a WTHOR database |
| `weval file`, `edaxify file` | Tests and conversions that use a WTHOR database |
| `seek board`, `mobility n`, `debug-pv move` | For development |

Next: [12. Using Edax from other programs](12-integration.md)
