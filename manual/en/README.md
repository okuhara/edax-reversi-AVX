# Edax 4.5.5 (fixed version) — User Manual

[日本語](../ja/README.md)

Version described: **v4.5.5-nikque.29** (`Nikque/edax-reversi-AVX`). Some parts do not apply to earlier versions; those places say "since v4.5.5-nikque.N".

Edax is a program that plays Othello (Reversi). It can also analyse games, solve endgames exactly and build opening data (a *book*). This manual starts with launching the program and playing one game, and goes on step by step to growing and maintaining a book of several hundred million positions.

## Reading order

If you are new, read chapters 1, 2 and 3 in order. If you want to build a book, continue with chapters 6, 7 and 8.

| Chapter | Content |
|---|---|
| [1. Introduction](01-introduction.md) | Edax and this fixed version. What the package contains, which executable to use, the words of this manual |
| [2. Starting and playing a first game](02-quick-start.md) | How to start, how to read the screen, playing a game against Edax, quitting |
| [3. Commands for playing and analysing](03-playing.md) | Entering moves, taking back, hints, setting up a position, saving and loading games, analysing a game, time limits |
| [4. Settings](04-settings.md) | `config.ini`, command-line options, changes while running. The list of settings and how to choose them |
| [5. Search basics](05-search.md) | Level, depth, the "%", exact solving, threads, the hash table, how to read the search output |
| [6. Book basics](06-book-basics.md) | What a book is. Positions, links and leaves, reading `book show`, files and how saving works |
| [7. Growing a book](07-book-learning.md) | Adding games (`book store`, `book add`, `book learn`), automatic expansion (`book deviate` and its family, `book enhance`, …), saving while it runs |
| [8. Maintaining a book](08-book-maintenance.md) | Fixing, merging, cutting, exporting, computing scores again |
| [9. Large books](09-large-books.md) | Memory, time, settings and cautions for books of tens to hundreds of millions of positions |
| [10. Game and problem files](10-files.md) | Game formats, the `base` commands, the problem format (OBF) |
| [11. Solving problems and measuring speed](11-solve-bench.md) | `-solve`, `bench`, counting positions |
| [12. Using Edax from other programs](12-integration.md) | Connecting a GUI (GTP, NBoard, XBoard, …), the library (libedax), edax_runner, the book analysis commands for a GUI |
| [13. Troubleshooting](13-troubleshooting.md) | Common messages, what they mean and what to do |
| [14. Reference](14-reference.md) | All commands, all settings, file extensions, the table of levels |
| [15. Glossary](15-glossary.md) | The words used in this manual |

## Conventions

- `Text like this` is either screen output or something you type.
- Input examples start with `>`. This is the prompt that Edax prints when it waits for input; do not type it.

  ```
  >hint 3
  ```

  This means "type `hint 3` and press Enter".
- A square is named by its column (`A` to `H`) and its row (`1` to `8`). `A1` is the top-left corner and `H8` the bottom-right one. Input is not case-sensitive.
- On the board shown on screen:
  - `*` is a black disc;
  - `O` is a white disc;
  - `-` is an empty square;
  - `.` is a square where the side to move can play.
- The examples use the name of the Windows executable (`wEdax-x86-64.exe`). On Linux and macOS, only the executable name differs (see the table in [chapter 1](01-introduction.md)).

## How reliable this manual is

- This manual was first written from the source code of v4.5.5-nikque.13 and from actual runs of the Windows version.
- Changes in later versions (v4.5.5-nikque.14 to 29) were added from their source code and from runs. New commands and settings are marked "since v4.5.5-nikque.N".
- The screen examples are real output (long ones are shortened).
- Anything that was not verified by running it is marked as such.
- Speed and memory figures are given together with the PC and the book they were measured on. **A figure measured on a small book does not predict the figure for a large one.**
