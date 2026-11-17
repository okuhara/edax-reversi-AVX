# 6. book の基礎

[目次](README.md) ｜ 前：[5. 探索の基礎](05-search.md) ｜ 次：[7. book を育てる](07-book-learning.md)

## 6.1 book とは

**book**（定石ファイル、オープニングブック）は、序盤から中盤の局面と、その評価値を保存しておくデータです。Edax は、今の局面が book にあれば、**探索せずに book の手を打ちます**。book を使うと、次の利点があります。

- 序盤を、対局中の探索よりずっと深い読みに基づいて打てる。この読みは、book を作るときに時間をかけて行ったものです。
- 序盤の手に時間を使わない。
- 局面ごとの評価値を、すぐに見られる（研究に使える）。

book は、ふつう 1 つのファイルです。配布版では `bin/data/book.dat` です。

配布版の `book.dat` は、**初期局面が 1 つ入っているだけ**の、ほぼ空の book です。使い方は次のどちらかです。

- 自分で育てる（[7 章](07-book-learning.md)）。
- ほかの人が作った Edax の book（Edax 4.4・4.5 の形式）に置き換えて使う。

book のコマンドは、どれも `book` で始まります。短く `b` とも書けます。`book` の後ろの語は、大文字で書いても通ります（`Book Info`）。

## 6.2 book の中身：局面・Link・Leaf

book は、**局面の集まり**です。1 つの局面には、次の情報が記録されています。

| 項目 | 意味 |
|---|---|
| 盤 | 石の並び。回したり裏返したりして重なる 8 通りの形は、同じ局面として 1 つにまとめて持つ |
| level | その局面を探索したときの level（[5 章](05-search.md)） |
| 評価値 | その局面の評価値（手番の側から見た石差の見込み）と、その下限・上限 |
| **Link**（リンク） | 「この手を打つと、book の中の別の局面に行く」という手と、その評価値 |
| **Leaf**（リーフ） | Link になっていない手のうち**いちばん良い手 1 つ**と、その評価値（探索で求めた値） |
| 対局の数 | この局面を通る手順の数（勝ち・引き分け・負け・まだ終局していない手順） |

大事なのは **Link と Leaf の違い**です。

- **Link の手**は、その先の局面も book に入っています。その先でさらに読んだ結果が、評価値に反映されています。
- **Leaf の手**は、その先の局面がまだ book にありません。「ここから先はまだ調べていないが、残りの手の中ではこれがいちばん良さそう」という目印です。
- **book を育てるとは、良さそうな Leaf を Link に変えていくこと**です。つまり、その先の局面を book に足していきます。
- Link にも Leaf にもなっていない手は、book には記録されていません。探索が「Leaf の手より悪い」と判断した手です。

局面の評価値は、Link と Leaf の評価値のうち、いちばん良いものです。下の局面の評価値が変わると、上の局面の評価値も変わります。この「下から上へ評価値を伝え直す」計算を **negamax**（ネガマックス）と言います。book を変えるコマンドは、最後に自動で negamax を行います。

## 6.3 book の中を見る

### book show：今の局面

今の盤の局面が、book にどう入っているかを表示します。

```
>book show

Level: 8
Best score: +0 [-2, +2]
Moves: [d3:+0] <f4:-3>
       152 incomplete lines.
```

| 行 | 意味 |
|---|---|
| `Level: 8` | この局面は level 8 で探索された |
| `Best score: +0 [-2, +2]` | 評価値は +0。`[下限, 上限]` は、探索の誤差を見込んだ幅 |
| `Moves:` | 記録されている手。**`[手:評価値]` が Link、`<手:評価値>` が Leaf**。良い順に並ぶ |
| `152 incomplete lines.` | この局面から先に、終局まで行っていない手順が 152 本ある |

- 手の大文字・小文字は、手番を表します（黒の手が大文字、白の手が小文字）。
- 終局まで行っている手順があるときは、`Lines: N full games with …% win, …% draw, …% loss` という行も出ます。
- 今の局面が book になければ、**何も表示されません**。

対局を進めながら `book show` を実行すると、定石を 1 手ずつたどれます。

### book info：book 全体

```
>book info
Edax Book 4.5; 2026-10-7 17:14:18;
Positions: 2246 (moves = 2301 links + 2243 leaves);
Level 8 : 2246 nodes
Depth: 12
Memory occupation: 1160986
Hash balance: 0 < 0 < 2
```

| 行 | 意味 |
|---|---|
| 1 行目 | book の形式の版と、作った（最後に作り直した）日時 |
| `Positions` | 局面の数と、Link の数・Leaf の数 |
| `Level 8 : 2246 nodes` | level ごとの局面の数。ふつうは 1 行（book の level）だけ |
| `Depth: 12` | book の深さ（6.4） |
| `Memory occupation` | book がメモリの中で使っている大きさ（バイト） |
| `Hash balance` | 内部の表の偏り（気にしなくてよい） |

book の level と違う level の局面が混ざっていると、`book info` はその局面を表示します。

- 最初の 10 個まで、1 行ずつ表示します（`{board:…}` で始まる行）。
- 11 個以上あるときは、その後に `(2245 positions have another level than the book: only the first 10 are shown)` のように個数を表示します。
- v4.5.5-nikque.12 までの版では、全部の局面を表示していました。

### book stats：数の内訳

`book stats` は、次の数を表示します。

- 空きマスの数ごとの、局面・Link・Leaf の数
- 評価値ごとの局面の数

```
Stage distribution:
stage    positions        links       leaves      terminal nodes
   48          880            0          880          880
   49          625          909          625            0
   50          352          639          352            0
 …
   60            1            4            0            0
```

`stage` は空きマスの数です。`terminal nodes` は、Link を 1 つも持たない局面の数です。たとえば、その先が book にない、いちばん深い層の局面がこれにあたります。

## 6.4 book の level と深さ

book には、全体の設定が 2 つあります。

**level**：局面を探索するときの level です。

- book を育てるコマンドは、新しい局面を、この level で探索します。
- **level の違う book を混ぜると、評価値の確かさがまちまちになります**。そのため、1 つの book は 1 つの level で育てるのがふつうです。
- level を高くすると、評価値は確かになりますが、局面 1 つを足す時間が長くなります。

**深さ**（Depth）：book が「何手目まで」を持つかです。深さ 40 の book は、初期局面から 40 手ぶんを持ちます。くわしくは次のとおりです。

- 「次が 40 手目」までの局面（空きが 21 以上の局面）は、育てるコマンドの対象になり、Link を持てます。
- 40 手目を打った後の局面（空きが 20 の局面）は、**いちばん深い層**として、Leaf だけを持つ形で入ります。それより先は入りません。

level と深さの組み合わせには、目安があります。

- [5 章](05-search.md)の表のとおり、level 18 の探索は、空きが 21 以下で完全読みになります。
- book の深さを「完全読みが始まるところまで」にしておけば、book の終わりの局面の評価値は、正確な値になります。
- Edax が book を自動で作るとき（下の 6.6）は、level からこの深さを決めます。

| book の level | 自動で決まる深さ（`book info` の Depth） |
|---|---|
| 10 以下 | 61 − level×2（例：level 8 なら 45） |
| 11〜18 | 40 |
| 19〜24 | 37 |
| 25〜29 | 34 |
| 30〜35 | 31 |
| 36〜41 | 67 − level（例：level 36 なら 31） |
| 42 以上 | 25 |

深さは、`book depth n` で変えられます。

- 変わるのは、今の book の「育てる範囲」だけです。局面は消えません。
- 範囲の外になった局面を消すには、`book subtree` を使います（[8 章](08-book-maintenance.md)）。
- `config.ini` の `book-depth` に数を書くと、起動のたびにその深さになります。`auto` なら、book のファイルに保存されている深さのままです。

## 6.5 対局で book を使う

| 入力・設定 | 意味 |
|---|---|
| `book on`（設定 `book-usage = on`） | 対局で book の手を打つ（配布版の設定） |
| `book off`（`book-usage = off`） | book の手を打たない（毎回考える） |
| `book randomness n`（設定 `book-randomness = n`） | book から手を選ぶとき、局面の評価値より n 石まで悪い手も候補にする（既定は 0） |

Edax が book から手を選ぶときは、局面に記録されている手（Link と Leaf）が候補です。そのうち、**評価値が「局面の評価値 − randomness」以上の手の中から、ランダムに 1 つ**を選びます。

- randomness が 0 なら、最善の手だけが候補です。
- 最善の手（同じ評価値の手）が複数あるときは、その中からランダムに選びます。そのため、**同じ局面でも毎回同じ手とは限りません**。
- randomness を 2 にすると、「最善より 2 石悪い手」までを打つようになり、序盤の変化が増えます。そのぶん、少し損な手も打ちます。
- book にない局面に来たら、そこからは探索して打ちます。

Edax が book から打ったときは、探索の表の `depth` の欄に `book` と表示されます。

```
 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
book    -2                                          F4 e3 F6 d3
------+-----+--------------+-------------+----------+----------------------

Edax plays F4
```

`hint` も、book を使う設定（`book on`）で今の局面が book にあれば、**book に記録されている手を先に**表示します（`book` の行）。残りの手は、探索して表示します。

book を使わずに探索の結果だけを見たいときは、`book off` にしてから `hint` を実行します。

```
>hint 4
 depth|score|       time   |  nodes (N)  |   N/s    | principal variation
------+-----+--------------+-------------+----------+----------------------
book    +0                                          D3 c4 F4 f6 F3 e6 E7 f7
book    -3                                          F4
    8   -03        0:00.000         18124            G5 c6 C5 c4 B5 e6
    8   -04        0:00.000           711            F3 d3 F4 f6 G5 e6
------+-----+--------------+-------------+----------+----------------------
```

## 6.6 book のファイル

### どのファイルが読まれるか

Edax は、起動のときに、設定 `book-file` のファイル（既定は `data/book.dat`）を読みます。

**ファイルがないとき**は、メモリの中に新しい book（初期局面だけ）を作ります。

- level は設定の `level`、深さは 6.4 の表のとおりです。
- 画面には `New book 18 21...` のように表示されます。2 つ目の数は「空きがいくつの局面までを持つか」です。深さ 40 なら 21 です。
- この新しい book は、Edax を終了するときに、その名前のファイルとして保存されます。何もしなくても、初期局面 1 つの小さなファイルができます。

**ファイルはあるのに読めなかったとき**（壊れている、途中で切れている、など）も、新しい book で始まります。

- この場合は、book を変えないかぎり、終了のときに保存しません。
- **読めなかったファイルは、上書きされません**。後でその名前に保存することになったときは、元のファイルを先に `book.dat.damaged` という名前に変えて残します。その名前がすでにあれば `.damaged.1`…とします。

**ファイルはあるのに開けなかったとき**（ほかのプログラムが共有なしで開いている、読む権限がない、など）は、2 秒ほど待って開き直します。それでも開けなければ、新しい book で始めますが、ファイルは壊れたものとして扱いません。

- `Cannot open opening book data/book.dat; it is not loaded, and no save replaces it` と表示します。
- **この book は、そのファイルには保存しません**（終了のときも、`book save` でも）。`.damaged` も作りません。元のファイルは、そのまま残ります。
- ほかのプログラムが閉じた後に `book load data/book.dat` で読めば、ふつうに使えます。起動し直してもかまいません。
- 開けなかった間に育てた book を残したいときは、`book save data/other.dat` のように別の名前で保存してください。
- v4.5.5-nikque.26 までの版は、開けなかったファイルも壊れたものとして扱い、後の保存で `.damaged` に移して、新しい book で置き換えていました。

別の book に切り替えるには、次のどれかを行います。

- `config.ini` に `book-file = data/my.dat` と書いて、起動し直す。
- 起動のときに `-book-file data/my.dat` と指定する。
- 実行中に `book load data/my.dat` を実行する（下の注意を読んでください）。

### 保存のしくみ

| いつ | どこへ |
|---|---|
| `book save ファイル名` を実行したとき | そのファイル |
| `book save` だけを実行したとき（名前なし） | `book-file` のファイル |
| Edax を終了するとき（`quit`）。**book が変わっていて、まだ保存していないときだけ** | `book-file` のファイル |
| book を育てるコマンド・保守のコマンドの途中と終わり | `book-file` の名前に拡張子を足したファイル（下の表） |

**注意 1：`book save` だけを実行すると、`book-file` のファイルに保存します。** 保存先が表示されます。

```
>book save
Book saved to data/book.dat
```

これは v4.5.5-nikque.13 からの動作です。

- v4.5.5-nikque.12 までの版では、名前を省くと `Cannot save book to ; existing book was not replaced` というエラーになり、保存されませんでした。
- 古い版では、`book save data/book.dat` のように、必ず名前を指定してください。

**注意 2：`book load` で読んだ book の「自動の保存先」は、読んだファイルではなく `book-file` です。**

たとえば、次のように操作したとします。

1. `data/book.dat` で起動する。
2. `book load data/other.dat` を実行する。
3. book を育てて終了する。

このとき、育てた book は `data/book.dat` に保存されます（`other.dat` は元のまま）。**それまでの `data/book.dat` は、`data/book.dat.old` という名前で残ります**。これは v4.5.5-nikque.13 からの動作で、`book new` の後と同じしくみです（[6.7](06-book-basics.md)）。`book import` でテキストファイルを取り込んだ後の終了も、同じです。

```
WARNING: data/book.dat holds the book in use before "book load": it is kept as data/book.dat.old
```

- 読んだファイルのほうに保存したいときは、`book save data/other.dat` を自分で実行してください。
- いつも同じ book を使うなら、`book-file` で指定するほうが安全です。
- **`book-file` の名前に拡張子を足したファイル**を読んだときは、同じ book の続きとして扱います。これは `data/book.dat.dev2`・`.store`・`.mrg` など、book のコマンドが自分で保存したファイルです。
  - この場合は、これまでどおり `book-file` を置き換えます（`.old` は残しません）。
  - 途中経過のファイルから続けるたびに、大きな book のコピーが増えないようにするためです。
  - `book.dat.old`・`book.dat.damaged` を読んだときは、`.old` を残します。
- 読んだ book を変えずに終了したときは、何も保存されません。
- v4.5.5-nikque.12 までの版では、それまでの `book-file` のファイルは残らず、上書きされます。

**注意 3：保存の間は、book と同じくらいの空きがディスクに要ります。**

- Edax は、いったん別の名前（`ファイル名.tmp.番号`）で全部を書きます。書き終えてから、元のファイルと入れ替えます。
- 途中で失敗しても、元のファイルは残ります。

### コマンドが自分で作るファイル

book を育てる・保守するコマンドは、途中経過や結果を、`book-file` に拡張子を足した名前で保存します。たとえば `book-file` が `data/book.dat` なら、`data/book.dat.dev2` などです。**元の `book.dat` は、これらの保存では上書きされません。**

| 拡張子 | 作るコマンド |
|---|---|
| `.store` | `book store`・`book learn`（設定 `book-store-auto-save = off` で作らない） |
| `.learn` | `book learn`（学習した行の記録。小さなテキストファイル。最後まで学習すると消える） |
| `.gam` | `book add` |
| `.dev`・`.dev2`・`.dev3` | `book deviate`・`book deviate2`・`book deviate3` |
| `.enh` | `book enhance` |
| `.fill`・`.fill2` | `book fill`・`book fill2` |
| `.play` | `book play` |
| `.leaf`・`.leaf2`・`.leaf3`・`.leaf4` | `book leaf recalculate`・`2`・`3`・`4` |
| `.lexp` | `book leaf expand` |
| `.mrg` | `book merge`（設定 `book-merge-auto-save = off` で作らない） |
| `.err` | `book correct` |
| `.dep` | `book deepen` |

- これらは、中身はふつうの book のファイルです。`book load data/book.dat.dev2` で読めます。名前を `book.dat` に変えれば、そのまま使えます。
- 育てるコマンドが無事に終わった後、`book save` をせずに Edax を終了すると、育てた book は `book-file`（`data/book.dat`）にも保存されます。
- つまり、ふつうに使っているかぎり、**`book.dat` が最新**です。拡張子の付いたファイルは「途中で止まったときの保険」です。
- **Windows では、`book-file` のフォルダーを含めた長さを 240 文字以内にしてください。** 長すぎると、拡張子の付いたファイルが保存できません（[13 章](13-troubleshooting.md)）。

## 6.7 新しい book を作る

```
>book new 18 40
New book 18 21......done>
```

この例は、level 18・深さ 40 の、初期局面だけの book をメモリの中に作ります。

- 今の book は、メモリから捨てられます。変えた book をまだ保存していないなら、先に `book save` で保存してください。
- 数字を省くと、level 21・深さ 36 になります。
- 作った book は、まだファイルになっていません。`book save data/my.dat` で保存するか、Edax を終了すれば `book-file` に保存されます。

### book new の後の保存と、それまでの book

`book new` の後、**ファイル名を指定しない保存**で `book-file` のファイルが新しい book に置き換わることがあります。そのまま Edax を終了したときと、`book save` だけを実行したときです。このときは、**それまでのファイルが、`.old` を付けた名前で残ります**（v4.5.5-nikque.13 から）。

```
>book new 8 12
New book 8 49......done>
>quit
WARNING: data/book.dat holds the book in use before "book new": it is kept as data/book.dat.old
```

この例では、終了の後、次のようになりました。

- `data/book.dat` は、新しい book（初期局面 1 つ、84 バイト）になった。
- それまでの book（98,976 バイト）は、`data/book.dat.old` という名前で残った。

それまでの book に戻す手順は次のとおりです。

1. Edax を終了する。
2. `book.dat` を削除するか、別の名前にする。
3. `book.dat.old` の名前を `book.dat` に変える。

細かい動作は次のとおりです。

- `book.dat.old` がすでにあるときは、`book.dat.old.1`（その次は `.old.2`…）という名前になります。前に残したファイルは、上書きされません。
- 次のときは、何も残しません。
  - それまでの `book-file` のファイルが、初期局面だけの book だったとき（配布版の `book.dat` など）
  - ファイルがなかったとき
- `book new` の後で、**ファイル名を指定して** `book save data/book.dat` を実行したときは、指定どおりに置き換えます（`.old` は残しません）。
- `book new` の後で、新しい book を育ててから終了したときも、`.old` が残ります。`book.dat` が育てた新しい book、`book.dat.old` がそれまでの book になります。
- `book new` の後で「やはり元の book を使う」ときは、`book load data/book.dat` を実行して読み直します。ファイルは何も変わりません。
- `.old` のファイルを、Edax が自分で削除することはありません。要らなくなったら、自分で削除してください。大きな book では、book と同じ大きさです。

`book load` で別のファイルを読んだ後の保存と、`book import` の後の保存でも、同じしくみが働きます（[6.6](06-book-basics.md)の注意 2）。

**v4.5.5-nikque.12 までの版では、それまでのファイルは残りません。**

- `book new` の後でそのまま Edax を終了すると、`book-file` のファイルが、新しい空の book で上書きされます。
- テストでは、98,976 バイトあった `book.dat` が、`book new` と `quit` だけで、初期局面 1 つの 84 バイトのファイルに置き換わりました。
- 古い版を使うときは、大事な book を使っている間に `book new` を実行しないでください。

**これまでの book と新しい book を取り違えないために**：

- 新しい book を育て始めるときは、`book new` を使うより、`config.ini` の `book-file` を新しい名前（例：`data/my.dat`）にしてから起動するほうが安全です。
- その名前のファイルがなければ、Edax は、設定の `level` で新しい book を作って始めます。
- 大事な book のファイルは、ときどき別の場所にコピーを取っておいてください。

## 6.8 この章のコマンド

| コマンド | 意味 |
|---|---|
| `book show` | 今の局面の、book の中身を表示 |
| `book info` | book 全体の情報 |
| `book stats` | 局面・Link・Leaf の数の内訳 |
| `book on`・`book off` | 対局で book を使う・使わない |
| `book randomness n` | book から手を選ぶときの幅 |
| `book depth n` | book の深さ（育てる範囲）を変える |
| `book new level 深さ` | 新しい空の book を作る |
| `book load ファイル`（`book open`） | book を読む |
| `book save ファイル` | book を保存する（ファイル名を省くと、`book-file` のファイルへ） |
| `book analyze n`（`book a`） | 今の対局の最後の n 手を、book の評価値で振り返る（[3.4](03-playing.md)） |
| `book verbose n` | book のコマンドの表示の量（0＝表示しない、1＝ふつう、2＝探索の表も表示） |

次：[7. book を育てる](07-book-learning.md)
