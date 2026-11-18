# 4. Settings

[Contents](README.md) | Previous: [3. Commands for playing and analysing](03-playing.md) | Next: [5. Search basics](05-search.md)

You change Edax's behaviour with **settings**. There are three ways to set them, and all three use the same names.

| Way | When it applies | Example |
|---|---|---|
| Write it in the settings file `config.ini` | At every start | `level = 18` |
| Give an option when starting (command line) | For that one run | `wEdax-x86-64.exe -level 18` (short: `-l 18`) |
| Type it after `>` while Edax runs | From the next search or command that starts after it | `level 18` |

## 4.1 The order in which settings are read

At startup, Edax reads its settings in this order. **Later values override earlier ones.**

1. The values built into the program ("built-in value" in the table of 4.5)
2. `edax.ini` in the current folder (if there is one; the package has none)
3. `config.ini` **in the folder of the executable** (`bin/config.ini` in the package)
4. The options of the command line

Notes:

- Edax reads the `config.ini` next to the executable. If the executable is in another folder, a `config.ini` in the current folder is not read.
- If the executable is started by name alone, without a folder (because it is in a folder on the `PATH`), Edax cannot tell where the executable is. It then looks for `config.ini` in the current folder. If there is none, it prints this warning and uses the built-in values:

  ```
  WARNING: config.ini was not found in the current folder (Edax was started without its folder name): default settings are used
  ```
- If the name of the executable's folder is too long (more than 249 characters on Windows), Edax cannot build the path of its `config.ini`. Then:
  - Edax prints `WARNING: the name of the folder of Edax is too long: its config.ini is not read, default settings are used` and uses the built-in values.
  - Up to v4.5.5-nikque.26, it read the `config.ini` of the current folder and printed the wrong warning of the previous item.
- Typing `options` after `>` shows all current values.

## 4.2 How to write config.ini

`config.ini` is a plain text file. You can edit it with Notepad or any text editor. Write one setting per line, as `name = value`.

```
# strength of the search
level = 18
n-tasks = auto
book-file = data/book.dat
```

- Everything from `#` to the end of the line is a comment. Empty lines are ignored.
- `level = 18`, `level=18`, `level 18` and `set level 18` all mean the same.
- Names are not case-sensitive.
- Inside a name, spaces, `_` and `-` are treated as the same character. `book-depth`, `book_depth` and `Book Depth` are the same setting.
- For an `on`/`off` setting, you can also write `true`/`false`, `yes`/`no` or `1`/`0`.
- With `=`, the whole rest of the line is the value, so a file name may contain spaces.
- Full-width spaces and the full-width "＝" are accepted.
- The encoding is UTF-8 (with or without a BOM). Both Windows and Linux line endings work.
- An unknown name produces a warning at startup with the file name and line number, and the line is ignored:

  ```
  WARNING: config.ini:7: unknown or incomplete setting "levle" ignored
  ```
- A value that is not a number (`level = abc`), or not `on`/`off`, also produces a warning and is ignored.
- A number out of range produces a warning and is clamped to the nearest valid value.

Any command-line option can be written in `config.ini` by its name without the leading `-`. For example, `-n 16` becomes `n-tasks = 16`, and `-book-file data/my.dat` becomes `book-file = data/my.dat`.

## 4.3 Giving options on the command line

```
wEdax-x86-64.exe -l 12 -n 4 -book-file data/my.dat
```

- You can give any number of `-name value` pairs.
- Some options are switches without a value:
  - `-q` (quiet; the same as `-verbose 0`);
  - `-vv` (verbose; the same as `-verbose 2`);
  - `-cpu`: give each thread its own CPU. On Linux, the thread is bound to that CPU; on Windows, it is only a preference. On Linux, this option also stops the book commands from running searches in parallel (`book-expand-tasks`, `book-store-tasks`).
- `-?` or `-help` prints the list of options (in English) and exits. `-v` or `-version` prints the version.
- Some options do not change settings but change what Edax does at startup:

  | Option | Meaning | Chapter |
  |---|---|---|
  | `-solve file` | Solve a problem file, print the results and end | [11](11-solve-bench.md) |
  | `-bench n` | Measure the speed and end | [11](11-solve-bench.md) |
  | `-gtp`, `-nboard`, `-xboard`, `-ggs`, `-cassio` | Start with the language (protocol) used to talk to another program | [12](12-integration.md) |
  | `-o file` (`-option-file`) | Also read settings from that file (written like `config.ini`) | |

## 4.4 Changing settings while Edax runs

Type the name and the value after `>`.

```
>level 12
>n-tasks 4
>book-randomness 2
```

- If nothing is printed, the setting was accepted. An invalid value produces a warning:

  ```
  >level abc
  WARNING: level: "abc" is not a number; ignored
  ```
- Changing `hash-table-size` while Edax runs rebuilds the search tables at the new size. Their contents are lost.
- With `hash-table-size = auto`, changing `n-tasks` while Edax runs also rebuilds the tables for the new number of threads. For example, after starting with 1 thread, typing `n-tasks 16` raised memory use from 79 MB to 242 MB.
- Up to v4.5.5-nikque.12, only the `options` output changed; the tables kept their startup size.
- Changing `book-file` while Edax runs does not load another book. It only changes where later automatic saves go ([chapter 6](06-book-basics.md)). To load another book, use `book load file`.

## 4.5 The settings in the package's config.ini

The package's `bin/config.ini` contains these settings. The "built-in value" is the value used when there is no `config.ini`, or when the line is removed.

| Setting | In the package | Built-in value | Meaning |
|---|---|---|---|
| `level` | `18` | 18 | Strength of the search at start (0 to 60). [Chapter 5](05-search.md) |
| `n-tasks` | `auto` | number of logical CPUs | Number of threads of the search. `auto`: the number of logical CPUs of the PC |
| `book-depth` | `auto` | `auto` | Depth of the book at start. `auto`: the depth saved in the book file. A number from 1 to 60 sets that depth at start. [Chapter 6](06-book-basics.md) |
| `book-usage` | `on` | `on` | Play the moves of the book in games, or not |
| `book-save-interval` | `360` | 60 | Minutes between the timed saves while a book is being grown. `0`: no timed save. [Chapter 7](07-book-learning.md) |
| `book-deviate-save-rounds` | `1` | 1 | For `book deviate` and its family: save after this many rounds that added something. `0`: only at the end. [Chapter 7](07-book-learning.md) |
| `book-merge-auto-save` | `on` | `on` | After each successful `book merge`, save to `<book file>.mrg`. [Chapter 8](08-book-maintenance.md) |
| `hash-table-size` | `auto` | 21 | Size of the hash table of the search. `auto`: chosen from the number of threads. [Chapter 5](05-search.md) |
| `book-expand-tasks` | `auto` | 1 | Number of positions that the automatic book commands expand at the same time. Chapters [7](07-book-learning.md) and [9](09-large-books.md) |
| `book-leaf-recalculate-rounds` | `1` | 1 | Largest number of rounds of `book leaf recalculate` and its family. [Chapter 8](08-book-maintenance.md) |
| `book-store-tasks` | `auto` | `auto` | Number of games learned at the same time by the commands that grow a book from games. Chapters [7](07-book-learning.md) and [9](09-large-books.md) |
| `probcut-model` | `standard` | `standard` | Pruning model of the search. `refit` is experimental (it changes the search results; read [v4.5.5-nikque.5 in the release notes](../../RELEASE-NOTES.md#v455-nikque5) before using it) |

**Note on `level` in `config.ini`**: a `level` in `config.ini` (or `edax.ini`) only sets the starting level. In a timed game (`-t`, `-move-time`), it does not limit the search depth. To limit it, use `-l` on the command line or the `level` command ([3.8](03-playing.md)).

## 4.6 Other commonly used settings

| Setting | Built-in value | Meaning |
|---|---|---|
| `book-file` | `data/book.dat` | The book file. It is read at start, and the automatic saves go there (and to this name with an extension added) |
| `eval-file` | `data/eval.dat` | The evaluation data file |
| `book-randomness` | 0 | When playing from the book: by how many discs a move may be worse than the best one and still be chosen. [Chapter 6](06-book-basics.md) |
| `book-store-auto-save` | `on` | Save to `<book file>.store` after `book store` and `book learn`, or not. [Chapter 7](07-book-learning.md) |
| `game-time` (`-t`) | none | Time for a whole game. [3.8](03-playing.md) |
| `move-time` | none | Time for one move |
| `ponder` | `on` | Think during the opponent's turn |
| `mode` | 3 | The `mode` at start ([3.2](03-playing.md)) |
| `verbose` | 1 | How much is displayed (0 to 4; [3.10](03-playing.md)) |
| `search-log-file` | none | File where the search is logged |
| `ui-log-file` | none | File where the input and the display are logged |
| `auto-start`, `auto-swap`, `auto-store`, `auto-quit`, `repeat` | `off`, 0 | What happens when games are played in a row ([3.9](03-playing.md)) |

All settings are listed in the tables of [chapter 14](14-reference.md).

## 4.7 Choosing settings

| Use | Settings |
|---|---|
| Playing and analysing only | The `config.ini` of the package can be used as it is. Adjust the strength with `level` |
| Using the PC for other things at the same time | Set `n-tasks` to about half the number of logical CPUs (the PC stays more responsive) |
| The same results at every run (for checks) | `n-tasks = 1`, and `hash-table-size` given as a number ([5.6](05-search.md)) |
| Growing a book | Read chapters [7](07-book-learning.md) and [9](09-large-books.md), then choose `book-expand-tasks`, `book-store-tasks` and `book-save-interval` |
| Growing a book exactly as earlier versions (the original Edax) did | `book-expand-tasks = 1` and `book-store-tasks = 1` |

Next: [5. Search basics](05-search.md)
