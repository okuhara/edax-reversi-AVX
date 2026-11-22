# 7. book を育てる

[目次](README.md) ｜ 前：[6. book の基礎](06-book-basics.md) ｜ 次：[8. book の保守](08-book-maintenance.md)

book に局面を足していくことを、このマニュアルでは「育てる」と言います（Edax の用語では「学習」）。育て方は、大きく 2 つあります。

| 育て方 | コマンド | 向いている場面 |
|---|---|---|
| **棋譜から**：打った対局・棋譜のファイルの局面を book に入れる | `book store`・`book add`・`book learn` | 自分の対局や、強い人・強いプログラムの棋譜を取り込む。調べたい定石を入れる |
| **自動で広げる**：book の中の「良さそうな Leaf」を、Edax が自分で選んで広げる | `book deviate`・`book deviate2`・`book deviate3`・`book enhance`・`book fill`・`book play` | book を、評価値の良い変化に沿って、広く・深くしていく |

実際には、両方を組み合わせます。棋譜で骨組みを入れ、自動で広げます。

**始める前に**、次の 2 つの節を読んでください。

- [6.6](06-book-basics.md)：どのファイルが読まれ、どこへ保存されるか
- [6.7](06-book-basics.md)：`book new` と、それまでの book のファイル

## 7.1 まず小さい book で試す

大きな book を育てる前に、低い level の小さい book で、コマンドの動きを見ておくことをおすすめします。次の例は、1 分ほどで終わります。表示は実際の出力です。数字は、PC と実行のたびに少し変わることがあります。

新しい名前の book のファイルを指定して起動します。

- `data/my.dat` は、まだ存在しない名前です。
- `-n 2` は 2 スレッド、`-l 8` は level 8 です。

```
wEdax-x86-64.exe -book-file data/my.dat -n 2 -l 8
```

level 8・深さ 12 の book にして、自動で広げます。

```
>book new 8 12
New book 8 49......done>

>book deviate 1 2
Book deviate 1 2:
Book deviate 1 todo
Book deviate...
Book deviate...1/1 done: 1 positions, 1 links
 …（何周も繰り返す）…
Book deviate 1 2...finished

>book info
Edax Book 4.5; 2026-10-7 17:10:44;
Positions: 110 (moves = 111 links + 108 leaves);
Level 8 : 110 nodes
Depth: 12
```

1 局面だった book が、110 局面になりました。条件を広げた別のコマンドで、さらに広げます。

```
>book deviate2 2 4
Book deviate2 2 4:
Book deviate2 32 todo
Book deviate2...
Book deviate2...1/32 done: 1 positions, 1 links
 …
Book deviate2 2 4...finished

>book info
Positions: 1952 (moves = 1989 links + 1949 leaves);
```

`quit` で終了すると、book は `data/my.dat` に保存されます。途中経過のファイル `data/my.dat.dev`・`data/my.dat.dev2` もできています（[6.6](06-book-basics.md)）。

## 7.2 棋譜から育てる

### book store：今の対局を入れる

盤の上で打った対局を、book に入れます。`play` で並べた手順でもかまいません。

```
>play f5d6c3d3c4
>book store
Searching positions...
Searching positions...2 done
Linking book...
Linking book...3 done
Negamaxing book...done
```

- 対局の局面のうち、次のものを、**終わりのほうから順に**探索して book に足します。
  - book の深さの範囲の局面
  - そのすぐ先の、いちばん深い層になる局面
- それより先の局面は入りません。途中までの対局でもかまいません。
- 足した後、Link の張り直しと negamax を行い、`<book のファイル名>.store` に保存します。
- 設定 `auto-store on` にしておくと、対局が終局するたびに、自動で `book store` が行われます（[3.9](03-playing.md)）。

### book add：棋譜のファイルを入れる

```
>book add games.txt
Adding 3 games to book...
Searching positions...
Searching positions...4 done
Adding games...3/3 done: 3 positions, 3 links
```

棋譜のファイルの全部の対局を、`book store` と同じように入れます。

- ファイルの形式は拡張子で決まります：`.txt`・`.ggf`・`.sgf`・`.pgn`・`.wtb`・`.edx`（[10 章](10-files.md)）。
- 結果は `<book のファイル名>.gam` に保存されます。

`.txt` の場合は、1 行に 1 局、手順を続けて書きます。

```
f5d6c3d3c4f4c5b3c2
f5f6e6f4e3c5c4e7c6e2
```

### book check：棋譜を book と比べる（book は変えない）

```
>book check games.txt
Checking 3 games to book...
Positions : 0 missing, 20 good, 4 bad (16.67% bad)
```

棋譜の中の「局面と、そこで打たれた手」を、book と比べて数えます。対象は次のとおりです。

- book の深さの範囲の局面だけ
- ふつうの初期局面から始まる棋譜だけ

| 語 | 意味 |
|---|---|
| `missing` | 局面が book にない |
| `good` | 打たれた手が、book の最善の手（と同じ評価値） |
| `bad` | 打たれた手が、book に記録されているが、最善より悪い |

### book learn：途中までの手順から、Edax に終局まで打たせて入れる

```
>book learn learn.txt
Playing games...
Playing games...2 done
Searching positions...
Searching positions...5 done
Linking book...
Linking book...2253 done
Negamaxing book...done
 …
3/3 games learned
```

ファイルの 1 行ごとに、次のことを行います。

1. その手順を打つ。
2. そこから Edax どうしで終局まで打つ。
3. その対局を `book store` する。

調べたい序盤の手順を並べておけば、その先を Edax が打ち継いで、book に入れてくれます。

ファイルの書き方：

```
f5d6c4
2,f5f6e6f4e3
# ここは説明の行
f5f4e3
```

- 1 行に 1 局。手順を続けて書きます（空白を入れてもよい）。
- `2,手順` のように、頭に数とコンマを書くと、その対局だけ、book から打つときの幅（`book-randomness`）をその数にします。
- 次の行は読み飛ばされます：空の行、`#` で始まる行、`//` を含む行。
- 打ち継ぎは、今の `level` で行われます。
- book にある局面では、book の手を打ちます（`book on` のとき）。
- 最後に `N/M games learned`（M 行のうち N 局を学習した）と表示されます。
- 打てない手のある行は、警告が出て、学習されません。
- 結果は `<book のファイル名>.store` に保存されます。

**途中から続ける**（v4.5.5-nikque.29 から）：

`.store` に保存するたびに、`<book のファイル名>.learn` という小さなテキストファイルも書きます。中身は次のとおりです。

- 棋譜のファイルの何行目まで学習したか
- 学習した行の中身の確認用の値
- そのときの book の局面の数・level・深さ

途中で止まったとき（Edax を閉じた、PC が止まった など）は、次のように続けます。

1. `.store` か、終了のときに保存された book を、`book load` で読みます。
2. **同じ棋譜のファイルで**、`book learn` をもう一度実行します。学習済みの行は飛ばして、残りの行だけを打って学習します。

```
Book learn: the first 120 lines of learn.txt (118 games) were learned by a run that did not end: they are skipped
```

- **ファイルの最後まで学習すると、`.learn` は消えます。**
- 次のときは、警告を出して、ファイルの初めから学習します。
  - 今の book が `.learn` の記録と違う（局面の数・level・深さが違う）とき
  - 棋譜のファイルの最初の行が、記録と違うとき
- 棋譜のファイルの後ろに行を足したときは、続きから学習できます。
- 設定 `book-store-auto-save = off`（`.store` を保存しない）では、`.learn` も書きません。
- v4.5.5-nikque.28 までの版には、`.learn` がありませんでした。もう一度実行すると、全部の行を初めから打ち直していました（`book-randomness` があると、前とは別の対局になります）。

### 棋譜の学習を同時に行う：book-store-tasks

`book store`・`book add`・`book learn` は、設定 `book-store-tasks` に従って、**複数の棋譜の局面の探索を同時に**進めます。

| `book-store-tasks` | 動作 |
|---|---|
| `auto`（既定） | `n-tasks` 本の棋譜を同時に学習する（1 本につき 1 スレッド）。いちばん速い |
| 数 n（2 以上） | n 本を同時に学習する（1 本につき `n-tasks ÷ n` スレッド） |
| `1` | 局面を 1 つずつ、全部のスレッドで探索する（元の Edax と同じ動作。できる book も同じ） |

- テストと実測の記録（`docs/validation/`） の実測（論理 CPU 32、level 18、棋譜 128 本）では、`auto` が `1` の 2.8〜3.3 倍の速さでした。
- `auto` でできる book は、`1` でできる book と、まったく同じにはなりません。
  - 一部の Leaf の手と評価値が、1〜2 石違うことがあります。
  - 違いの大きさは、探索のスレッドの数を変えたときと同じ程度です。
  - 元の Edax と同じ book が必要なときだけ、`1` にしてください。
- `book learn` は、`book-store-tasks` 本ずつをひと組にして、同時に対局します。
  - 同じ組の中の棋譜は、互いの学習の結果を使わずに対局します。
  - 組が終わるたびに、まとめて book に入ります。

## 7.3 自動で広げる：book deviate の仲間

### しくみ

これらのコマンドは、**今の盤の局面から**始めて book の Link をたどり、条件に合う **Leaf を選んで広げます**。

「広げる」とは、次の 2 つを行うことです。

- その Leaf の手を打った先の局面を探索して、book に足す（Leaf が Link になる）。
- 元の局面には、残りの手の中から次の Leaf を探索して付ける。

そのため、1 つ広げるごとに、探索を 2 回行います。

1 回の流れ（これを「1 周」と呼びます）：

1. negamax（評価値を最新にする）
2. 条件に合う Leaf を全部選ぶ。選んだ数が `todo` と表示される
3. 選んだ Leaf を、1 つずつ（または同時に）広げる
4. negamax して、決まった間隔で保存する

広げた結果、評価値が変わって、新しい Leaf が条件に合うようになります。そのため、**何も増えなくなるまで、周を繰り返します**。終わると `...finished` と表示されます。条件を広く取ると、周は何百回も続き、時間は何時間・何日にもなります。

- 始める局面は、**今の盤**です。
  - 初期局面から全体を広げるなら、`init` を実行してからコマンドを実行します。
  - ある定石だけを掘るなら、`play f5d6c3` のように進めてからコマンドを実行します。
- **今の盤の局面が book に入っていないと、コマンドは何もしません**。何も表示されずに、すぐ `>` に戻ります。book にない局面から始めたいときは、先に `book store` で、そこまでの手順を book に入れます。
- book の深さ（[6.4](06-book-basics.md)）より先には広げません。
- 表示の `Book deviate2...11/32 done: 11 positions, 11 links` は、次の意味です。
  - 選んだ 32 個のうち 11 個を広げ終えた。
  - この周で局面が 11、Link が 11 増えた。

### 3 つのコマンドの条件

どれも数を 2 つ取ります。

| コマンド | 1 つ目の数 | 2 つ目の数 | 完全読みの局面の Leaf |
|---|---|---|---|
| `book deviate X Y` | 「片方の側」が最善から外れてよい石数（相対誤差） | 始めた局面の評価値から離れてよい石数（絶対誤差） | 広げる |
| `book deviate2 X Y` | 1 手で失ってよい石数（どちらの側も） | 始めた局面からそこまでに、**両方の側が失った石数の合計**の上限 | **広げない** |
| `book deviate3 X Y` | `book deviate2` と同じ | `book deviate2` と同じ | 広げる |

数を省くと、どれも `2 4` です。

数の後ろに `export`（下の書き出し）以外の語があると、エラーになります。

- `ERROR: book deviate2: unknown word … after the numbers` と出て、book を広げません。
- v4.5.5-nikque.26 までは、`exprot` のような打ち間違いを読み飛ばして、book を広げていました。

**book deviate X Y**（元の Edax からある方法）：1 周の中で、選択と展開を 2 回行います。

- 1 回目は、次の手順をたどります。始めた局面の手番の側は、最善より X 石まで悪い手も選びます。相手は最善の手だけです。
- 2 回目は、その逆です。
- どちらも、評価値が「始めた局面の評価値 ± Y」の範囲にある手だけを対象にします。
- つまり「**片方だけが少し間違え、もう片方は正しく打つ**」変化を広げます。

**book deviate2 X Y**（この修正版で足した方法）：

- 手を 1 つ選ぶたびに、「その局面の最善の手との評価値の差」を「失った石数」として数えます。
- **1 手で失うのは X 石まで、始めた局面からの合計（黒と白を合わせて）が Y 石まで**の手順をたどり、その範囲の Leaf を広げます。

`book deviate2 5 5` の例：

| 手順の中の失点 | 範囲の中か |
|---|---|
| 黒が 2 石、白が 3 石（合計 5） | 中 |
| 1 手で 5 石（合計 5） | 中 |
| 1 手で 6 石 | 外（1 手の上限を超える） |
| 黒が 3 石、白が 3 石（合計 6） | 外（合計の上限を超える） |

`book deviate2` は、**完全読みの局面の Leaf を広げません**。

- 完全読みの局面とは、その局面の level で、空きの数が完全読みの範囲に入っている局面です。level 18 なら空き 21 以下です。
- 完全読みの局面は、Leaf の評価値がすでに正確です。その先を book に足しても、評価値は変わりません。
- そのため、book の終わりの層を無駄に広げずに済みます。

**book deviate3 X Y**：条件は `book deviate2` と同じで、完全読みの局面の Leaf も広げます。

### どれを使うか・数の決め方

- **ふつうは `book deviate2`** をおすすめします。「両方の側の間違いの合計」で範囲を決めるので、対局で実際に現れる変化を、偏りなく広げられます。
- 数を大きくすると、対象が急に増えます。
  - 上の小さい book（level 8・深さ 12）では、`book deviate 1 2` で 110 局面、続けて `book deviate2 2 4` で 1,952 局面になりました。
  - **これは小さい book での数で、大きな book での見込みにはなりません**。
  - 最初は小さい数（`1 2`、`2 4`）から始め、時間と局面の数を見ながら広げてください。
- 途中で止めても、そこまでに保存された分は残ります（下の 7.5）。
- 同じコマンドをもう一度実行すれば、続きから広げます。すでに広げた Leaf は、もう Link になっているので、選ばれません。

### 広げずに、たどる手順をテキストに書き出す（export）

3 つのコマンドの数の後ろに `export <ファイル> [leaf|ends|all]` を付けると、**book を広げません**。代わりに、そのコマンドが今の局面からたどる手順を、テキストファイルに書き出します（v4.5.5-nikque.26 から）。

```
book deviate2 2 4 export lines.txt ends
```

| 最後の語 | 書き出す局面 |
|---|---|
| `leaf`（省いたとき） | そのコマンドが 1 周目に広げる Leaf を持つ局面（まだ探索していない Leaf がある局面） |
| `ends` | たどる手順の端の局面（そこから先へ、条件に合う手が book にない局面） |
| `all` | たどる手順に出てくる局面すべて（今の局面も含む） |

**書き出す行**

- 1 行に 1 局面を書きます。行は、初手から今の局面までの手順に、今の局面からその局面までの手順を続けたものです。
- 手順は `f5d6c3…` の形（小文字）で書きます。パスは書きません。
- 違う手順で着く同じ局面（合流）は、1 回だけ書きます。そのときの手順は次のとおりです。
  - `book deviate`：手数がいちばん少ない手順
  - `book deviate2`・`book deviate3`：失った石数の合計がいちばん少ない手順
  - どちらでも、同じものがいくつかあるときは、先に見つかった手順
- パスしかない局面と、そのパスの後の局面は、同じ手順の行になります。このときは 1 行だけ書きます。
- 今の局面が初期局面のとき、`all` では、初期局面そのものの行は空の行です。
- 端になるのは、次の局面です。
  - 終局の局面
  - book の深さ（`book depth`）より先の局面
  - 条件に合う手が book にない局面
- `book deviate` は、2 回のたどり（上の説明）の分を合わせて書きます。どちらのたどりも、広げる前の book でたどります。
- `book deviate` の `ends` は、どちらのたどりも先へ進まない局面です。

**数の範囲**

- `book deviate2`・`book deviate3` の合計の上限は、254 までです。広げるときと同じです。
- 254 を超える数は、254 にします。このとき、広げるときと同じ警告（`WARNING: total loss 300 reduced to 254`）を出します。libedax の `edax_book_deviate_export` も同じです。
- v4.5.5-nikque.26 までは、libedax だけ `cumulative loss = 300 is out of range. Set to 254` と出していました。

**終わったとき・ファイル名**

- 終わると、`Book deviate2 2 4 export ends...finished: 120 lines written (lines.txt)` と出ます。
- 今の局面が book にないときは、空のファイルを作ります。
- book は変わりません。そのため、保存も途中経過のファイルもありません。
- 空白を含むファイル名は、`"…"` で囲みます（例：`book deviate2 2 4 export "C:\My Books\lines.txt" ends`）。引用符は名前に入りません。

**書き出せないとき**

`ERROR:` で始まる行が出て、book は広げません。次のときです。

| 原因 | 表示 |
|---|---|
| ファイル名がない | `ERROR: book deviate export: no file` |
| ファイル名が長すぎる（Windows では 259 字まで） | `the file name is too long` |
| 引用符が閉じていない | `the quote of the file name is not closed` |
| 最後の語が `leaf`・`ends`・`all` のどれでもない | `unknown word` |
| ファイルに書けない | `ERROR: book deviate2 2 4 export: cannot write the file lines.txt` |

v4.5.5-nikque.26 までは、長すぎるファイル名を、何も表示せずに切っていました。

## 7.4 自動で広げる：そのほかのコマンド

| コマンド | 意味 | 途中経過のファイル |
|---|---|---|
| `book enhance X Y` | 評価値の下限・上限を使って、「探索の誤差（中盤の探索で X 石、終局まで読むが 100% でない探索で Y 石）を見込むと、局面の評価値を変えるかもしれない Leaf」を広げる。変わるものがなくなるまで繰り返す。数を省くと `2 4` | `.enh` |
| `book fill n` | book の局面から n 手進めた先が、book の中の別の局面になるとき、その途中の局面と Link を足す。埋め始めの局面にも、その場で Link を張る（その局面の Leaf がその手だったときは、`book deviate` で広げたときと同じく、探索し直して次の Leaf を決める）。増えるものがなくなるまで繰り返す。`book fill 1`（数を省いたとき）は、book の局面の 1 手先の局面に欠けている Link が張られるだけで、局面は増えない | `.fill` |
| `book fill2 n` | `book fill n` と同じだが、埋め始めの局面からの 1 手目は **Leaf の手だけ**。「Leaf の先の局面が book になく、その次の局面は book にある」所を埋める（n = 2 のとき）。数を省くと 2 | `.fill2` |
| `book leaf expand` | **Leaf の評価値が、その局面のどの Link よりも大きい局面**（最善の手が、まだ book の外にある局面）の Leaf を広げる。広げると、次の Leaf がまた Link より良くなることがあるので、そういう局面がなくなるまで繰り返す。Link のない局面は対象にならない | `.lexp` |
| `book play` | Link を 1 つも持たない局面（手順の先端）の Leaf を広げる。これを、そういう局面が book の深さの範囲からなくなるまで繰り返す。つまり、**どの手順も、最善の手で book の深さまで伸ばす** | `.play` |
| `book play n` | 今の盤の局面から Link でたどれる、book の深さの範囲の Link のない局面から始める。そこから Leaf を 1 手ずつ伸ばす。空きが n 以上の局面を広げるので、最後に足す局面の空きは n − 1 になる。Link のある局面に合流した手順は、そこで終わる | `.play` |
| `book extend` | 今の盤の局面から Link でたどれる範囲で、**Leaf の評価値がどの Link にも劣らない局面**の Leaf を広げる。同点の局面と、Link のない局面も対象。そういう局面がなくなるまで繰り返す | `.ext` |
| `book exhaust` | 今の盤の局面から book の深さまで、**すべての合法手**（パスを含む）を足す。局面の数は深さとともに急に増える | `.exh` |

コマンドの由来と注意：

- `book enhance`・`book fill`・`book play` は、元の Edax からあるコマンドです。
- `book extend`・`book exhaust`・`book play n` は、Edax 4.7 のコマンドを取り入れたものです（v4.5.5-nikque.33 から）。
  - Edax 4.7 の数なしの `book play` は、今の盤の局面から空き 20 まで伸ばします。この版では、その動作は `book play 20` です。数なしの `book play` は、今までどおり book 全体が対象です。
  - `book play n` の n が book の深さより先のとき、その先の局面は `book prune` で消えます。
  - `book exhaust` は、先に `book depth` で深さを小さくしてから使います。
    - 初期局面から深さ 7 で約 1.3 万局面です。深さが 1 増えるごとに数倍になります。
    - 足す局面を全部集めてから探索します。集めている間は保存しません。メモリが足りなければ、book を変えずに終わります。
- `book play` は、v4.5.5-nikque.33 から `book-expand-tasks` に従って同時に広げます。
  - それより前の版では、設定にかかわらず 1 つずつでした。
  - 2 以上のときは、`book deviate` と同じく、実行ごとに book が少し変わります（7.6）。
  - 初期局面からたどれない局面の評価値も、前の版と違うことがあります。
- `book fill2`・`book leaf expand` は、この修正版で足したコマンドです（v4.5.5-nikque.14 から）。
- `book fill` も、同じ版で中身を変えました（`book fill 1` を除く）。
- どれも、大きな book での使用実績は `book deviate` 系列ほどありません。まず小さい book で、何が起きるかを見てから使ってください。

### 範囲を決める：book fill・book fill2・book leaf expand の 2 つの数

`book fill 2 5 5`・`book fill2 2 5 5`・`book leaf expand 5 5` のように、後ろに 2 つの数 X・Y を付けられます。このときは、**今の盤の局面から `book deviate3 X Y` がたどる範囲だけ**が対象になります。

- X は 1 手で失う石数の上限、Y は合計（黒と白を合わせて）の上限です。意味は 7.3 の `book deviate2`・`book deviate3` と同じです。
- 数を省くと、book 全体が対象です。

| コマンド | 埋める・広げる元になる局面 |
|---|---|
| `book fill n X Y` | `book deviate3 X Y` がたどる局面すべて |
| `book fill2 n X Y` | `book deviate3 X Y` が広げる Leaf を持つ局面 |
| `book leaf expand X Y` | `book deviate3 X Y` がたどる局面のうち、Leaf がどの Link よりも良い局面 |

- 完全読みの局面の Leaf も対象になります（`book deviate3` と同じ）。
- `book fill`・`book fill2` の n は「**ちょうど n 手先**」です。
  - n 手より近い所の欠けは埋めません。
  - 欠けている局面が 1 つだけの所（局面 → 欠けた局面 → book の局面）は、n = 2 で埋まります。
  - `book fill2` は、数を省くと 2 です。`book fill2 1` は、足す局面がないので何もしません。
- 同時に行う探索の数は、`book deviate` と同じく `book-expand-tasks`（7.6）に従います。設定がないとき（1）は、1 つずつ探索します。
- 埋める・広げる元になる局面は、複数のスレッドで探します。book のコピーは作りません。

**どの周も、初めに Link を張ります。**

- 選ぶ局面は、コマンドによって違います。
  - `book fill`・`book fill2`：「手の先の局面が、もう book にあるのに、その手が Link になっていない局面」（どの手でも）
  - `book leaf expand`：「Leaf の手の先の局面が、もう book にある局面」
- 選んだ局面の、欠けた Link を全部張ります。Leaf が Link になった局面は、Leaf を探し直します。
- こういう局面は、別の手順から同じ局面が足されるとできます（`book deviate` でもできます）。
- `book fix` でも張られます。しかし、探し直した Leaf がまた Link より良くなることがあります。そのため、これらのコマンドは自分で張り、張るもの・埋めるもの・広げるものがなくなるまで繰り返します。
- 範囲を付けたときは、`book deviate3 X Y` がたどる局面だけが対象です。
- `book fill`・`book fill2` の後は、範囲の中に欠けた Link は残りません。v4.5.5-nikque.13 までの `book fill` も、埋め始めの局面のまわりの局面の欠けた Link を張っていました。
- `book leaf expand` は、Leaf の手の先が book にない局面の欠けた Link は張りません。そちらは `book fix` で張ります。

表示の例（上の 7.1 の小さい book で、`book deviate2 2 4` の後）：

```
>book fill2 2
Book fill2 (links): 13 positions have a move to a position of the book that is not a link
Book fill2 (links)...13/13: 0 positions, 17 links
Book fill2 2: 36 positions to fill from
Book fill2...36/36: 36 positions, 75 links
 …（足すものがなくなるまで繰り返す）…
Book fill2 2... finished

>book fill 2 2 4
Book fill 2 2 4:
Book fill (links): 8 positions have a move to a position of the book that is not a link
Book fill (links)...8/8: 0 positions, 10 links
Book fill 781 positions selected, 52 to fill from
Book fill...52/52: 56 positions, 117 links
 …
Book fill 2 2 4...finished

>book leaf expand 3 6
Book leaf expand 3 6:
Book leaf expand (links): 6 leaves lead to a position of the book
Book leaf expand 3 todo
Book leaf expand...
Book leaf expand...1/3 done: 1 positions, 1 links
 …
Book leaf expand 3 6...finished
```

| 表示 | 意味 |
|---|---|
| `(links)` の行 | Link を張った局面の数 |
| `positions selected` | `book deviate3` のたどりで選ばれた局面の数 |
| `to fill from` | そのうち、実際に埋める元になる局面の数 |
| `positions, links` | 足した局面と Link の数 |

`book play` の、小さい book での表示の例です。

```
>book play
Book play...153 todo
Book play...
Book play...1/153 done: 1 positions, 1 links
 …
```

## 7.5 途中の保存と、止め方

自動で広げるコマンドは、長く動きます。止まったとき（停電、PC の再起動、自分で止めた）に失うのは、**最後に保存した後の分**です。保存の間隔は、2 つの設定で決めます。

| 設定 | 配布版の値 | 意味 |
|---|---|---|
| `book-save-interval` | 360（分） | 展開の途中で、この時間ごとに保存する。`0` は、時間では保存しない |
| `book-deviate-save-rounds` | 1 | `book deviate` 系列で、局面や Link が増えた周を、この数だけ終えるごとに保存する。`0` は、コマンドが終わったときだけ |

- 保存先は、`<book のファイル名>.dev2` など（[6.6](06-book-basics.md)の表）です。`book.dat` そのものは、コマンドの途中では書き換えられません。
- 大きな book では、1 回の保存に時間とディスクの空きが要ります（[9 章](09-large-books.md)）。
  - 周が短いのに毎周保存すると、保存ばかりしていることになります。そのときは `book-deviate-save-rounds` を大きくします（例：10）。
  - 逆に、1 周が何時間もかかるなら、`book-save-interval` を短くします。
- `book play`・`book play n`・`book extend`・`book exhaust` は、`book-save-interval` の時間ごとと、終わったときに保存します。
  - `book-deviate-save-rounds` は使いません。
  - `book-save-interval = 0` では、終わったときだけです。
  - `book play` は、v4.5.5-nikque.32 までは毎周保存していました。
- **`0` と `0` の組み合わせ（どちらの保存もしない）では、コマンドが終わるまで何も保存されません。** 途中で止まると、全部を失います。

**止め方**：book のコマンドは、途中で中断する入力を受け付けません。

- `stop` を入力しても、止まりません。
  - v4.5.5-nikque.13 からは、「何も止めなかった」という表示が出ます。
  - それより前の版では、探索中の 1 局面だけが打ち切られ、途中の結果が book に入っていました。
- `quit` を入力すると、**コマンドが終わってから**終了します。
- すぐに止めたいときは、Edax のプログラムそのものを止めます（ウィンドウを閉じる、Ctrl+C を押す）。
  - メモリの中の book は失われます。残るのは、最後に保存されたファイルです。
  - 保存の途中で止めても、書きかけのファイルが本物と入れ替わることはありません（[6.6](06-book-basics.md)の「保存のしくみ」）。

**止めた後の続け方**：最後の途中経過のファイル（例：`data/book.dat.dev2`）のほうが、`data/book.dat` より新しいはずです。次のどちらかで続けます。

- ファイルの名前を変える：`book.dat` を別の名前にして残し、`book.dat.dev2` を `book.dat` に変えてから、Edax を起動する。
- 起動してから `book load data/book.dat.dev2` を実行する。この場合も、自動の保存先は `book-file` です（[6.6](06-book-basics.md)の注意 2）。

そのうえで、同じコマンド（`book deviate2 5 5` など）をもう一度実行します。

同じコマンドで続けたときに、済んだ所をもう一度探索するかどうかは、コマンドによって違います。

- `book play n`：手順の先端を `<book のファイル名>.play-to` に記録します。
  - 途中経過の book（`.play`）を読み、同じ盤の局面で同じ n を実行すると、続きから進みます。
  - 続ける前に、このファイルを消したり移したりしないでください。book の深さより先の続きが分からなくなります。
- `book deviate` 系列・`book enhance`・`book leaf expand`・`book fill`・`book fill2`・`book play`・`book extend`・`book exhaust`・`book add`：済んだ所は探索し直しません。
  - 広げた手や足した局面が、book に残っているからです。
  - たどり直す手間だけかかります。
- `book correct`・`book leaf recalculate` 系列（v4.5.5-nikque.29 から。[8.6](08-book-maintenance.md)・[8.7](08-book-maintenance.md)）：
  - 探索し直した局面には、book の中でマークが付きます。続けたときは、マークのない局面だけを探索します。
  - マークの付いた途中の book は、v4.5.5-nikque.28 までの版では読めません。
- `book learn <ファイル>`（v4.5.5-nikque.29 から。[7.2](07-book-learning.md) の book learn）：
  - 学習した行の数が、`<book のファイル名>.learn` に書かれます。
  - 同じファイルで続ければ、学習済みの行を飛ばします。
- `book deepen`（v4.5.5-nikque.29 から。[8.7](08-book-maintenance.md)）：book の level にした局面は、探索し直しません。
- v4.5.5-nikque.28 までの版では、`book correct`・`book leaf recalculate` 系列・`book learn`・`book deepen` は、続けても最初から全部をやり直していました。

## 7.6 同時に広げる：book-expand-tasks

低い level の探索は短いので、1 つの探索に多くのスレッドを使っても、あまり速くなりません（[5.5](05-search.md)）。設定 `book-expand-tasks` で**複数の Leaf を同時に**広げると、CPU を使い切れます。`book deviate`・`book deviate2`・`book deviate3`・`book enhance`・`book play` に効きます。

| `book-expand-tasks` | 動作 |
|---|---|
| `1`（`config.ini` がないときの値） | 1 つずつ、全部のスレッドで探索する。元の Edax と同じ動作 |
| 数 n | n 個を同時に広げる。1 つの探索は `n-tasks ÷ n` スレッド |
| `auto`（配布版の `config.ini` の値） | book の level から決める：level 18 以下は 1 つの探索に 2 スレッド、24 以下は 4、それより上は 8（同時に広げる数は、`n-tasks` をそれで割った数）。さらに、level 18 以下で、その周の対象が `n-tasks` の 32 倍以上あるときは、1 スレッドの探索を `n-tasks` 個同時に動かす |

- テストと実測の記録（`docs/validation/`） の実測（論理 CPU 32、level 18、6 億 5,688 万局面の book、`book deviate3 2 5` の最初の 1 万件）：`1` で約 805〜832 秒、`16`（各 2 スレッド）で約 174 秒。
- **できる book は、`1` のときと同じにはなりません。**
  - 増える局面と Link の手は同じでも、一部の Leaf の手と評価値が 1〜2 石違うことがあります。
  - 同時に広げている局面どうしは、先に book に入ったものだけを見ます。そのため、同じ周の 2 つの局面が同じ新しい局面に行き着くとき、その場では Link が張られないことがあります。この Link は、後の周や `book fix` で張られます。
  - どちらになるかは、探索の終わる順で決まります（下の `book-reproducible` を参照）。
- 同時に動く探索は、それぞれハッシュ表を持つので、メモリが増えます（[9 章](09-large-books.md)）。
- 対象が 1 つだけの周は、1 つずつ広げるのと同じです。対象が少ない周では、同時に動く数も減ります。
- 元の Edax とまったく同じ動作にしたいときは、`book-expand-tasks = 1` にします。

### 毎回同じ book にする：book-reproducible と book-seed

ふだんは、実行するたびに book が少し変わることがあります。理由は 2 つあります。

- `book-expand-tasks` が 2 以上（`auto` を含む）のとき、同時に広げている局面は、ほかの局面の結果が先に book に入っていればそれを見て、まだならそれを見ずに探索します。どちらになるかは探索の終わる順で決まるので、同じコマンドでも結果が変わることがあります。
- book の同じ評価値の手からどれを選ぶかは、乱数で決まります（`book-randomness = 0` でも）。その種は時計から取られます。そのため、`book learn` は実行するたびに別の対局を学習します。

| 設定 | 動作 |
|---|---|
| `book-reproducible = on` | book のコマンドの探索をすべて 1 スレッドにし、同時に広げた結果を、選んだ順に book に入れる。入れる前に、新しい局面の Link が探索したときから変わっていれば、それだけ探索し直す。広げるコマンドは、スレッドの数によらず `n-tasks 1` のときと同じ book になる |
| `book-seed = 数` | book の乱数の種をその数にする。`book learn` が毎回同じ対局を学習する |

- 既定はどちらも従来どおり（`off`、`auto`）です。
- `on` のとき、1 スレッドの探索がそれぞれ、1 スレッドのときのハッシュ表（`hash-table-size = auto` で 57 MB）を持ちます。`n-tasks` が 32 なら、約 1.8 GB を使います。
- 同じになるのは、保存日時（ファイルの先頭の 8 バイト）を除いた内容です。
- `book store`・`book add`・`book learn` を複数のスレッドで行ったときは、毎回同じにはなりますが、`n-tasks 1` のときとは違います。
- 次の探索には効きません。
  - 対局の手を探す探索（`play`・`go`、`n-tasks 1` や `book-store-tasks 1` での `book learn` の対局）
  - 時間を決めた探索

## 7.7 育て方の例

**例 1：自分で研究したい定石を掘る**

```
>play f5d6c3d3c4
>book deviate2 3 6
```

今の盤（`tiger` と呼ばれる定石の局面）から先だけを広げます。

この局面が book にまだ入っていないときは、コマンドが何もしません。その場合は、先に `book store` を実行して、そこまでの手順を book に入れます。順番は `play f5d6c3d3c4`、`book store`、`book deviate2 3 6` です。

**例 2：棋譜の束を入れてから、全体を整える**

```
>book add mygames.txt
>init
>book deviate2 2 4
```

**例 3：手順のリストを Edax に打ち継がせる**

```
>book learn openings.txt
```

**例 4：毎日少しずつ広げる（大きな book）**

1. `config.ini` に、保存の間隔と、同時に行う数を決めておく。
2. 起動して `book deviate2 5 5` を実行し、時間が来たら止める。
3. 次の日は、最後の途中経過のファイルから続ける（7.5）。

数億局面の book での注意は、[9 章](09-large-books.md)にまとめてあります。

## 7.8 この章のコマンド

| コマンド | 意味 |
|---|---|
| `book store` | 今の対局を book に入れる |
| `book add ファイル` | 棋譜のファイルの対局を book に入れる |
| `book check ファイル` | 棋譜の手を book と比べて数える |
| `book learn ファイル` | 手順のリストを、Edax どうしで打ち継いで book に入れる |
| `book deviate X Y` | 片方の側が X 石まで外れ、評価値が ±Y の範囲の変化を広げる |
| `book deviate2 X Y` | 1 手 X 石まで・合計 Y 石までの失点の変化を広げる（完全読みの Leaf を除く） |
| `book deviate3 X Y` | 同じ（完全読みの Leaf も含む） |
| `book deviate` 系列の X Y の後ろに `export ファイル` ［`leaf`・`ends`・`all`］ | 広げずに、たどる手順をテキストファイルに書き出す（例：`book deviate2 2 4 export lines.txt ends`） |
| `book enhance X Y` | 探索の誤差を見込んで、評価値を変えそうな Leaf を広げる |
| `book fill n` ［X Y］ | book の局面と局面の間を埋める（X Y で `book deviate3` の範囲だけ） |
| `book fill2 n` ［X Y］ | Leaf の先の欠けを埋める |
| `book leaf expand` ［X Y］ | どの Link よりも良い Leaf を広げる |
| `book play` | Link のない局面を 1 手ずつ広げる |
| `book play n` | 今の局面から、Link のない局面の手順を空き n まで伸ばす |
| `book extend` | 今の局面から、どの Link にも劣らない Leaf を広げる |
| `book exhaust` | 今の局面から book の深さまで、すべての手を足す |

次：[8. book の保守](08-book-maintenance.md)
