# 13. 困ったときは

[目次](README.md) ｜ 前：[12. ほかのプログラムから使う](12-integration.md) ｜ 次：[14. コマンドと設定の一覧](14-reference.md)

よく出る表示と、起きやすいことを、場面ごとにまとめます。Edax の表示は英語です。

- `WARNING:` は「続けられるが、気を付けて」という意味です。
- `ERROR:` は「その操作は行われなかった」という意味です。`ERROR:` の行には、ソースのファイル名と行の番号も出ます。

## 13.1 起動のとき

| 表示・起きたこと | 意味 | 対処 |
|---|---|---|
| `Cannot open data/eval.dat` と出て終わる | 評価データが見つからない。Edax は、今いるフォルダーの `data/eval.dat` を探す | `bin` フォルダーの中から起動する。または `-eval-file フォルダー/eval.dat` で場所を教える |
| 何も出ずに、すぐ終わる（`v3`・`v4` の実行ファイル） | CPU が、その命令セット（AVX2・AVX-512）に対応していない可能性 | 名前に `v3`・`v4` の付かない実行ファイルを使う（この場合の表示は、確かめていません） |
| `WARNING: config.ini was not found in the current folder (Edax was started without its folder name): default settings are used` | 実行ファイルをフォルダー名なしで起動したので、`config.ini` の場所が分からなかった | フォルダーを付けて起動する（`.\wEdax-x86-64.exe`）。または、今いるフォルダーに `config.ini` を置く |
| `WARNING: the name of the folder of Edax is too long: its config.ini is not read, default settings are used` | 実行ファイルのフォルダーの名前が長すぎて、`config.ini` のパスを作れない | 名前の短いフォルダーに置く（[4.1](04-settings.md)） |
| `WARNING: config.ini:7: unknown or incomplete setting "levle" ignored` | `config.ini` の 7 行目の名前が間違っている | その行を直す（[4.2](04-settings.md)） |
| `WARNING: level: "abc" is not a number; ignored` | 数を書くところに、数でないものが書いてある | 値を直す |
| `WARNING: n-tasks = 99 is out of range. Set to 32` のような表示 | 値が範囲の外。範囲の端の値に直された | そのままでも動く。気になるなら値を直す |
| `New book 18 21...` | `book-file` のファイルがないので、新しい book を作った | 初めての起動なら正常。book があるはずなら、`book-file` の名前と、起動したフォルダーを確かめる |
| `data/book.dat could not be loaded: it is kept as data/book.dat.damaged` | book のファイルが読めなかった（壊れている、途中で切れている）。後で保存するときに、元のファイルを別の名前で残した | `.damaged` のファイルが、元の book。本当に壊れているなら、コピーや途中経過のファイル（`.dev2` など）から戻す。v4.5.5-nikque.26 までの版では、ほかのプログラムが開いていただけのときも、こうなる。そのときは、名前を戻せば読める |
| `Cannot open opening book data/book.dat; it is not loaded, and no save replaces it (it may be used by another program)` | book のファイルを開けなかった（ほかのプログラムが共有なしで開いている、読む権限がない、など）。2 秒ほど待っても開けなかった。新しい book で始めたが、このファイルには保存しない（`.damaged` も作らない） | ほかのプログラムがファイルを閉じてから、`book load data/book.dat` で読む。または、Edax を起動し直す（[6.6](06-book-basics.md)） |
| `Cannot save book to data/book.dat: the file could not be opened when the book was loaded …` | 上の場合に、そのファイルへ保存しようとした（`book save`、終了のとき）。元のファイルは、そのまま | 育てた book を残すなら、`book save data/other.dat` のように、別の名前で保存する |

## 13.2 対局のとき

| 表示・起きたこと | 意味 | 対処 |
|---|---|---|
| `WARNING: Unknown command/Illegal move: "e3 "` | 打てないマスか、知らないコマンド | 打てるマスは、左の盤の `.`。コマンドの綴りは [14 章](14-reference.md) |
| `WARNING: Unknown command/Illegal move: "leaf expand 30 30" (book command: type "book leaf expand 30 30" or "b leaf expand 30 30")` | book のコマンドを、`book` を付けずに入力した | 案内のとおり、`book`（または `b`）の後に書く。この案内は、v4.5.5-nikque.23 から出る |
| 手を打っても Edax が打ってこない | `mode 3` になっている（起動した直後、`stop` の後） | `mode 0`（自分が黒）か `mode 1`（自分が白）。1 手だけなら `go` |
| 打てるマスがなくて進まない | パスの番 | `ps` |
| Edax が考え続けて戻ってこない | level が高い、または持ち時間が長い | `stop`。その後 `level` を下げて `mode 0`／`mode 1` |
| Edax が、考えずにすぐ打つ | book の手を打っている（表の `depth` が `book`）。または level が低い | book を使いたくなければ `book off` |
| 同じ局面なのに、毎回違う手を打つ | book の中の、同じ評価値の手から、ランダムに選んでいる。または複数スレッドの探索の揺れ | [6.5](06-book-basics.md)・[5.7](05-search.md) |
| `WARNING: Cannot open file mygame.txt` | `load` のファイルが開けない（名前・フォルダーの間違い）。盤は前のまま | ファイルの名前と場所を確かめる。フォルダーは、今いるフォルダー（`bin`）から見た場所 |
| `WARNING: Illegal move #2: A1` | `load` の棋譜の中に、打てない手がある（`#2` は、0 から数えた手の番号）。その棋譜は読み込まれず、盤は前のまま | 棋譜のファイルを直す（[10.1](10-files.md)） |
| `WARNING: game text: "A1F4" is not a move that can be played here: the rest of the line is not read` | `.txt` の棋譜の行に、打てない手（または、手として読めない文字）がある。その手前までが読み込まれた | 行を直す。途中の局面から始まる対局を `.txt` で保存した行は、読み戻せない（[10.1](10-files.md)） |
| `load` しても何も表示されず、何も変わらない（v4.5.5-nikque.12 までの版） | ファイルが開けない、または棋譜の中に打てない手がある。古い版は、理由を表示しない | ファイルの名前と場所、棋譜の中身を確かめる |
| `WARNING: Unknown game format extension: .abc` | 拡張子が、知っている形式でない | `.txt`・`.sgf`・`.ggf`・`.pgn`・`.edx` を使う（[3.6](03-playing.md)） |
| `WARNING: error while importing a SGF game`（GGF・PGN でも） | 棋譜の中に、打てない手がある。または、途中の局面から始まる SGF。その棋譜は読み込まれず、盤は前のまま | 棋譜のファイルを直す。途中の局面から始まる対局は、`.ggf`・`.pgn`・`.edx` で保存する（[10.1](10-files.md)）。**v4.5.5-nikque.12 までの版では、石を返さない手が置かれた、おかしな盤になることがある。そのときは `init` でやり直す** |
| `WARNING: uncomplete game.`（v4.5.5-nikque.12 までの版） | 終局していない対局の `.pgn` を読み込んだ | 正常。読み込みは、できている |
| `WARNING: board_set: bad string input` | `setboard` の盤が 64 マスに足りない、または手番がない | 64 マスを数え直し、最後に手番（`X` か `O`）を書く（[3.5](03-playing.md)） |
| 持ち時間を指定したのに、level の深さで読みをやめる | `-l`（または `level` コマンド）を指定していると、それが上限になる | `-l` を付けない。`config.ini` の `level` は上限にならない（[3.8](03-playing.md)） |

## 13.3 book のとき

| 表示・起きたこと | 意味 | 対処 |
|---|---|---|
| `book show` で何も表示されない | 今の局面が book にない | 正常。`book store` などで入れる |
| `@error word=id`（番号のない 1 行） | book の解析のコマンド（`book moveinfo` など）か `stop` の `id=` が、int の範囲の整数でない。何も行わず、何も止めない（v4.5.5-nikque.19 から） | `id=` を整数にする。ファイルの名前に `id=` があるときは `"…"` で囲む（[12.4](12-integration.md)） |
| `book deviate`（`book deviate2` など）が、何も表示せずにすぐ終わる | 今の盤の局面が book にない | 初期局面から始めるなら `init`。途中の局面から始めるなら、先に `book store` |
| `Book deviate2 0 todo` で終わる | 条件に合う Leaf がない（もう広げ終えた、または数が小さすぎる） | 数（X・Y）を大きくする。book の深さ（`book info` の Depth）も確かめる |
| `Cannot save book to ; existing book was not replaced` | v4.5.5-nikque.12 までの版で、`book save` にファイル名を書いていない | `book save data/my.dat` のように名前を書く（v4.5.5-nikque.13 からは、名前を省くと `book-file` のファイルに保存する） |
| `WARNING: data/book.dat holds the book in use before "book new": it is kept as data/book.dat.old`（`"book load"`・`"book import"` のこともある） | `book new`・`book load 別のファイル`・`book import` の後の、名前を書かない保存（終了、`book save` だけ）で、`book-file` のファイルが別の book に置き換わった。それまでのファイルは、`.old` を付けた名前で残した | 正常。それまでの book に戻すなら、Edax を終了してから、`.old` のファイルの名前を元に戻す（[6.7](06-book-basics.md)）。要らなければ、`.old` のファイルを消す |
| `WARNING: wrong depth: % depth …` | `book import` のファイルの、深さの行が読めない（1〜60 の数でない） | 取り込みは、できている。`book info` で深さを確かめ、`book depth 数` で直す（[8.5](08-book-maintenance.md)） |
| `WARNING: wrong board: % depth 12` と `1 lines of … hold no position: skipped`（v4.5.5-nikque.12 までの版） | v4.5.5-nikque.13 からの版が `book export` で書いたファイルを、古い版で `book import` した。最後の行（深さ）を読めずに飛ばした | 局面は、全部読まれている。`book info` で深さを確かめ、`book depth 数` で直す（[8.5](08-book-maintenance.md)） |
| `Cannot open temporary book …`（保存できない） | 保存先に書けない：フォルダーがない、書き込みの許可がない、**フォルダーを含めた名前が長すぎる**（下の 13.4） | フォルダーを作る・短い場所に置く |
| `Cannot write complete book to …; existing book was not replaced` | 書いている途中で失敗した（ディスクの空きが足りない、など）。元のファイルは残っている | ディスクの空きを作って、もう一度保存する。**Edax を終了する前に**行う（終了すると、メモリの中の book は失われる） |
| `WARNING: Book … was not loaded; current book retained` | `book load` のファイルが読めなかった。今の book は、そのまま | 名前と場所を確かめる（v4.5.5-nikque.12 までの版では、ファイルがないとき、その前に `New book …` とも表示される。失敗の途中の表示で、今の book には影響しない） |
| `(2245 positions have another level than the book: only the first 10 are shown)` | `book info` の表示。book の level と違う level の局面が 11 個以上ある（level の違う book を merge した、など） | 正常。level の違う局面を混ぜたくなければ、同じ level で育てた book どうしを merge する（[8.3](08-book-maintenance.md)） |
| `cannot open …`、`WARNING: Book … was not merged` | `book merge` のファイルが読めなかった。今の book は、そのまま | 名前と場所を確かめる |
| `WARNING: Unknown book command: "book link"` | そういう book のコマンドはない | [14 章](14-reference.md)の一覧を見る。Link を張り直したいときは `book fix` |
| `[stop: nothing is stopped while a book or base command is running]` | book・base のコマンドの実行中に `stop` を入力した。何も止めていない | すぐ止めたいなら、Edax そのものを止める（[7.5](07-book-learning.md)） |
| `quit` を入力したのに終わらない | book のコマンドが終わるのを待っている | 待つ。待てないなら、Edax そのものを止める（最後の保存の後の分は失われる） |
| `WARNING: book subtree: this position is deeper than the book depth; the book is not changed` | 今の盤の局面が、book の深さより先にある | `book depth` を確かめる。局面を戻してから行う |
| `WARNING: book reduce: this position is deeper than the book depth; the book is not changed`、`WARNING: book reduce: this position is not in the book; the book is not changed` | 今の盤の局面が、book の深さより先にある、または book にない | 局面を戻してから行う（ふつうは `init` で初期局面に戻す） |
| `WARNING: book reduce: two loss limits are needed, …` | `book reduce` に数が 2 つない、範囲の外（X は 0〜129、Y は 0〜254）、または数の後ろに別の語がある | `book reduce 2 4` のように 2 つの数を書く |
| `not enough memory to search the positions at the same time` | 同時に行う探索のためのメモリが足りない。数を減らして続けている | そのままでも動く。`book-expand-tasks` を小さくするか、`hash-table-size` を `auto` にする |
| `not enough memory to play N games at the same time: they are learned one after the other` | 同時に学習するためのメモリが足りない。1 局ずつ学習している | `book-store-tasks` を小さくする |
| `cannot create a thread: the search uses N threads instead of M` | スレッドを作れなかった。作れた数で続けている | そのままでも動く。`n-tasks` を小さくする |
| book のファイルが、急に小さくなった（84 バイトなど） | `book new` の後で終了した、または book のファイルがない状態で起動して終了した | `book new` の後なら、それまでの book は、同じフォルダーの `.old` の付いたファイルに残っている（v4.5.5-nikque.13 から。[6.7](06-book-basics.md)）。それより前の版では残らないので、写し・途中経過のファイルから戻す |
| `book.dat.tmp.12345` のようなファイルが残っている | 保存の途中で、Edax が止められた | 消してよい（Windows では、次の保存のときに Edax が消す）。`book.dat` は元のまま |
| 同じ手順で育てたのに、book が毎回少し違う | 複数スレッドの探索と、同時に行う展開は、実行ごとに結果が少し変わる。book の手を使う対局は、同点の手をランダムに選ぶ | 正常。まったく同じ book が必要なら、`n-tasks = 1`、`book-expand-tasks = 1`、`book-store-tasks = 1`（とても遅くなる） |
| book を育てている間、PC が重い | 全部の CPU を使っている | `n-tasks` を減らして起動し直す |

## 13.4 Windows で、book の途中経過のファイルが保存されない

Edax は、book を保存するとき、いったん「保存する名前＋`.tmp.`＋番号」という名前で書きます。

- Windows では、フォルダーを含めたファイル名は 259 文字までです。そのため、`book-file` が長いと、この一時ファイルを作れません。
- とくに、`.store`・`.dev2` のように拡張子を足した名前への保存は、その分だけ長くなります。

保存できないときは、次のように表示されます。

```
Cannot open temporary book …
```

- その保存だけが行われません。メモリの中の book はそのままで、処理は続きます。
- **`book-file` の、フォルダーを含めた長さを 240 文字以内にしてください。** 240 文字以内なら、どの保存も収まります。
- 相対パス（`data/book.dat`）で指定していても、数えられるのは、ドライブ名から始まる全部の長さです。Edax を深いフォルダーに展開していないか、確かめてください。

## 13.5 それでも分からないとき

- `options` で、今の設定を確かめます。思っていた `config.ini` が読まれているかが分かります。
- `book info` で、book の局面の数・level・深さを確かめます。
- 設定 `ui-log-file`（入力と表示のログ）・`search-log-file`（探索のログ）にファイル名を書いて起動すると、何が起きたかを後から見られます。
- 配布版の `RELEASE-NOTES.ja.md` には、版ごとに次のことが書いてあります。
  - 「互換性」：動作が変わるところ
  - 「注意」：直していないもの・知っておくこと

次：[14. コマンドと設定の一覧](14-reference.md)
