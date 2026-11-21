# 15. Glossary

[Contents](README.md) | Previous: [14. Reference](14-reference.md)

Terms used in this manual and on the Edax screen.

| Term | On the screen | Meaning |
|---|---|---|
| Book | book | Data that holds positions of the opening and the midgame with their scores ([chapter 6](06-book-basics.md)) |
| Confidence (%) | `@73%` and the like | How cautious the search is when it prunes branches based on an estimate. At 100% nothing is pruned based on an estimate ([5.3](05-search.md)) |
| Depth (of a book) | `Depth` | How many moves deep the book goes ([6.4](06-book-basics.md)) |
| Depth (of a search) | depth | How many moves ahead the search looks |
| Disc difference | score | The difference between the numbers of discs at the end of the game |
| Empties | `empties` | The number of empty squares. 60 at the start of a game |
| Evaluation data | `eval.dat` | The data used to score midgame positions. Edax does not start without it |
| Exact solving | solve, exact | Searching every line to the end of the game to get the exact disc difference. A search at 100% whose depth equals the number of empties ([5.2](05-search.md)) |
| Expand | expand | Search the position after a leaf move of the book and add it to the book. The leaf becomes a link |
| First position | initial position | The position from which the current game started. Usually the initial position; `setboard` and loading a game change it |
| Game | game | The record of a game's moves |
| Hash table | hash table | The table in which the search stores the results for positions it has examined. Setting: `hash-table-size` ([5.6](05-search.md)) |
| Initial position | | The usual starting position of Othello (4 discs in the centre) |
| Intermediate file | `.dev2`, `.store` and so on | A book that a book command saves, during or at the end of its run, under the `book-file` name plus an extension ([6.6](06-book-basics.md)) |
| Leaf | leaf, `<move:score>` | In a book position, the single best move among those that are not links. The position after it is not yet in the book |
| Level | level | The number (0 to 60) that sets the strength of the search. The depth and confidence are derived from it and from the number of empties ([5.2](05-search.md)) |
| libedax | | The library that makes Edax callable as functions from other programs ([12.2](12-integration.md)) |
| Link | link, `[move:score]` | In a book position, a move whose resulting position is also in the book |
| Logical CPU | | The number of CPUs that the operating system sees (the number of cores, or twice that number) |
| Negamax | negamax | The computation that propagates book scores from deeper positions up to shallower ones |
| Node | node | One position examined by the search. `nodes` is their number, `N/s` the number per second |
| OBF | `.obf` | The format of problem files ([10.3](10-files.md)) |
| Opening | opening | A known sequence of moves at the start of the game. The `opening` command shows the opening's name, if it has one |
| Pass | `PS`, `pa` | Giving the turn to the opponent because no legal move is available |
| Position | position, board | The discs on the board and the side to move. A book stores the 8 symmetric forms as one position |
| Principal variation | principal variation, PV | The expected line when both sides play their best |
| Protocol | protocol | The fixed set of commands used to communicate with another program (GTP, NBoard, XBoard, …; [12.1](12-integration.md)) |
| Round | round | One cycle of "select → expand → negamax" in a command that expands the book automatically ([7.3](07-book-learning.md)) |
| Score | score | The expected disc difference when both sides play best moves to the end, from the side to move's point of view (`+` is good) |
| Search | search | Looking ahead at future moves |
| Side to move | `Black's turn`, `White's turn` | The side that plays next |
| Temporary file | `book.dat.tmp.number` | The file to which the whole book is first written during a save. Once complete, it replaces the real file ([6.6](06-book-basics.md)) |
| Thread | task, thread | A unit of work that the CPU can run in parallel with others. Setting: `n-tasks` |
| todo | `todo` | The number of leaves selected for a round by a command that expands the book automatically |

[Back to the contents](README.md)
