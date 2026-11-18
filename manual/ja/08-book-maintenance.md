# 8. book の保守

[目次](README.md) ｜ 前：[7. book を育てる](07-book-learning.md) ｜ 次：[9. 大きな book を扱う](09-large-books.md)

この章では、できた book を「直す・まとめる・切り詰める・書き出す・評価値を計算し直す」コマンドを説明します。

**どのコマンドも、メモリの中の book を変えます。** ファイルに残るのは、次のときです。

- `book save` を実行したとき
- コマンドが自分で保存するとき（[6.6](06-book-basics.md)の表）
- Edax を終了したとき（book が変わっていれば `book-file` へ）

**やり直しはできません。** 大事な book を保守する前に、ファイルのコピーを取っておいてください。

## 8.1 保守のコマンドの一覧

| したいこと | コマンド | 節 |
|---|---|---|
| 評価値を、下から上へ伝え直す | `book negamax` | 8.2 |
| 欠けている Link を張り、おかしな局面を直す | `book fix` | 8.2 |
| 2 つの book を 1 つにまとめる | `book merge` | 8.3 |
| book を浅くする・ある定石の先だけ残す | `book depth`＋`book subtree` | 8.4 |
| 悪い手ばかりの変化を捨てる | `book prune` | 8.4 |
| `book deviate3 X Y` でたどれない局面を消して軽くする | `book reduce` | 8.4 |
| テキストファイルに書き出す・読み戻す | `book export`・`book import` | 8.5 |
| `eval.dat` を替えた後などに、Leaf の評価値を計算し直す | `book leaf recalculate` 系列 | 8.6 |
| 完全読みの局面の値を確かめ直す | `book correct` | 8.7 |
| book から棋譜や問題を取り出す | `book extract`・`book problem` | 8.8 |

## 8.2 book negamax と book fix

### book negamax

```
>book negamax
Negamaxing book...done
Sorting book...done>
```

初期局面から Link をたどって、全部の局面の評価値を、下の局面の値から計算し直します。その後、各局面の手を良い順に並べ替えます。

- book を変えるコマンドは、自分で negamax を行います。そのため、ふつうは自分で実行する必要はありません。
- 実行するのは、途中経過のファイル（とくに `book leaf recalculate` の途中の保存）を読み込んだ後などです。
- 初期局面から Link でたどれない局面は、計算し直されません。どの局面からも Link が張られていない、孤立した局面です。

### book fix

```
>book fix
Fixing book...
Fixing book...0 done
Linking book...
Searching positions...
Searching positions...6 done
Linking book...2246 done
Negamaxing book...done
Sorting book...done>
```

book 全体を点検して、整えます。順に、次のことを行います。

1. **点検**（`Fixing book`）：中身のおかしい局面（打てない手が記録されている、など）を見つけて、作り直す。`0 done` なら、おかしな局面はなかった。
2. **Link の張り直し**（`Linking book`）：どの局面についても、「打つと book の中の別の局面に行く手」が Link になっているようにする。Leaf の手が Link になった局面は、新しい Leaf を探索して求める（`Searching positions`）。
3. negamax と並べ替え。

`book fix` が役に立つ場面：

- **`book-expand-tasks` を 2 以上（`auto` を含む）にして育てた後**。
  - 同時に広げた局面どうしは互いを見ません。そのため、「別の手順でも同じ局面に行ける」ときの Link が欠けていることがあります（[7.6](07-book-learning.md)）。
  - `book fix` で、この Link が張られます。
- ほかの人の book や、古い版で作った book を使い始めるとき。
- book の様子がおかしいとき（Edax が book の途中で止まる、など）。

大きな book では時間がかかります（[9 章](09-large-books.md)）。局面を作り直すときは、その局面を book の level で探索します。

## 8.3 book をまとめる：book merge

```
>book load data/a.dat
>book merge data/b.dat
Checking book data/b.dat...
Merging book data/b.dat...
Merging book data/b.dat...1 positions added
Linking book...
 …
Negamaxing book...done
Sorting book...done>
Merged book saved to data/my.dat.mrg
```

今の book（統合先）に、別の book のファイル（統合元）を取り込みます。

- **統合元にしかない局面だけ**を足します。両方にある局面は、統合先のものを残します（上書きしません）。
- 足した後、Link の張り直し・点検・negamax・並べ替えまでを行います。`book fix` を別に実行する必要はありません。
- 設定 `book-merge-auto-save = on`（既定）なら、結果を `<book のファイル名>.mrg` に自動で保存します。
  - 元の book のファイルは、この保存では上書きされません。
  - `.mrg` を使うには、`book load` で読むか、名前を変えます。
- 統合元のファイルがない・壊れているときは、何も取り込まずに、今の book を保ちます。

  ```
  cannot open data/nofile.dat
  WARNING: Book data/nofile.dat was not merged
  ```
- 統合元は、メモリに読み込まれません（ファイルを 2 回読みます）。使うメモリは、統合先の book の分だけです。

**気を付けること**

- 2 つの book は、**同じ level** で育てたものにしてください。level が違うと、次の問題が起きます。
  - まとめた book の中で、評価値の確かさがまちまちになる。
  - Link の張り直しで必要になる探索が増える。
- 同じ局面が両方にあるとき、残るのは統合先の値です。「新しいほう・確かなほうを統合先にする」と覚えてください。
- 別々の PC やフォルダーで同じ book を並行して育て、後でまとめる、という使い方ができます。ただし、この修正版では 1 つの Edax で多くのスレッドを使い切れるので（[7.2](07-book-learning.md)・[7.6](07-book-learning.md)）、その必要は少なくなっています。

## 8.4 book を切り詰める

### book depth と book subtree：浅くする・一部だけ残す

`book subtree` は、**今の盤の局面から、Link をたどって行ける局面だけ**（book の深さの範囲のもの）を残し、ほかを全部消します。

**使い方 1：book を浅くする**（例：深さ 40 の book を 39 にする）

```
>book depth 39
>init
>book subtree
```

1. `book depth 39` で深さの設定を変える。
2. `init` で初期局面に戻す。
3. `book subtree` を実行する。

これで、深さ 39 より先の局面が消えます。初期局面からたどれない、孤立した局面も消えます。

**使い方 2：ある定石の先だけを取り出す**

```
>play f5d6c3d3
>book subtree
{board:--------------------OX-----XO-----XXO-------O------------------- X; level:8; best: +0 [-2, +2];moves: [F4:+0] <F6:-5>}
Book subtree 13... done
done
 …
>book save data/sub.dat
```

今の盤の局面と、その先だけが残ります。

- **初期局面からそこまでの局面も消えます**。そのため、できた book は、その局面から始める研究用です。初期局面で `book show` を実行しても、何も表示されなくなります。
- 元の book を残したいなら、`book save` で**別の名前**に保存し、終了する前に元の book を読み直してください（下の注意）。

そのほかの動作：

- 今の盤の局面が、book の深さより 2 つ以上先のときは、警告が出て、何もしません。これは v4.5.5-nikque.13 からです。それより前の版では、book が空になっていました。

  ```
  WARNING: book subtree: this position is deeper than the book depth; the book is not changed
  ```
- `book subtree` は、切り詰めた後に Link の張り直しを行いません（v4.5.5-nikque.12 から）。必要なら、前か後に `book fix` を実行します。

### book prune：悪い手ばかりの変化を捨てる

```
>book prune
{board:---------------------------OX------XO--------------------------- X; level:8; best: +0 [-2, +2];moves: [D3:+0] [C4:+0] [F5:+0] [E6:+0]}
Book prune 808... done
Book prune 1448... done
done
 …
>book info
Positions: 1269 (moves = 1296 links + 1265 leaves);
```

初期局面から、次の手順でたどれる局面だけを残します。

- 「**片方の側はどんな手でも打つが、もう片方は最善の手だけを打つ**」手順
- 黒がどんな手でも打つ場合と、白がどんな手でも打つ場合の両方

両方の側が最善でない手を打った先の局面は、消えます。上の例では、2,262 局面が 1,269 局面になりました。

使いどころ：

- 対局で使う book を小さくしたいときに使います。相手がどう打っても、自分は最善で応じられる範囲が残ります。
- 研究用に「両者の間違い」まで育てた book（`book deviate2` で広げたもの）に使うと、その大半が消えます。

### book reduce：book deviate3 X Y でたどれる局面だけを残して軽くする

v4.5.5-nikque.20 からのコマンドです。

```
>init
>book reducecount id=1 x=2 y=4
@end id=1 kept=19 removed=25184 positions=25203 ms=16
>book reduce 2 4
Book reduce 2 4: 19 positions kept, 25184 removed
Fixing book...0 done
Negamaxing book...done
Sorting book...done
>book info
Positions: 19 (moves = 22 links + 17 leaves);
```

今の盤の局面から `book deviate3 X Y` がたどる局面だけを残し、ほかを消します。上の例（テスト用の小さな book）では、25,203 局面が 19 局面になりました。ふつうは、`init` で初期局面に戻してから実行します。

**残る局面**

- 今の局面と、そこから「1 手の損失（石数）が X 以下、そこまでの損失の合計が Y 以下」の手でたどれる局面です。
- これは、`book deviate3 X Y` が広げる局面を探すときにたどる範囲と同じです。
- 別の手順から同じ局面に来るときは、損失の合計がいちばん小さい経路で決まります。
- たどった局面からその手で行ける、book の深さの次の局面と終局の局面も残ります。

**動作**

- 実行すると、まず今の局面から先の評価値を計算し直します（`book deviate3` と同じ）。
- 消した局面への手は、その局面の Leaf の候補になります（`book prune` と同じ）。
- 残った局面の評価値は、先に計算し直した後の値から変わりません。
- その後に同じ局面から `book deviate3 X Y` を実行すると、消す前と同じ Leaf を最初に選びます。
- 初期局面でない局面で実行すると、`book subtree` と同じく、初期局面からそこまでの局面も消えます。

**数の範囲と警告**

- X は 0〜129、Y は 0〜254 です。
- 次のときは、警告が出て何もしません。
  - 数が 2 つないとき
  - 数が範囲の外のとき
  - 数の後ろに別の語があるとき（v4.5.5-nikque.21 から）
  - 今の局面が book にないとき
  - 今の局面が、book の深さより 2 つ以上先のとき

**先に数える：book reducecount**

- 消える数は、`book reducecount id=N x=X y=Y` で先に確かめられます。book は変えません（[12.4](12-integration.md)）。
- `book reduce` と同じく、評価値を計算し直した値で数えるので、同じ数になります。
- 数えた後、book を変えずに、同じ局面・同じ X Y で `book reduce` を実行すると、数えた結果を使います。計算し直しとたどる処理を省きます（v4.5.5-nikque.22 から。設定 `book-reduce-reuse`）。
- 数えた結果（局面 1 つに 2 バイトほど）は、`book reduce`、book を変えるコマンド、次の `book reducecount` のどれかまで持ちます。
- その間に、ほかの解析（`book moveinfo` など）がマークのためのメモリを必要とするときは、数えた結果を捨てて、メモリをそちらに譲ります。
  - 捨てた後の `book reduce` は、計算し直してたどります。残る局面は同じです。
  - v4.5.5-nikque.26 までは、数えた結果を、book が変わるまで持ち続けていました。ほかの解析のマークは、1 つ 16 バイトほどの形になることがありました。

**メモリの返却**

- ほとんどの局面を消した後は、バケツの数を減らしてメモリを返します（v4.5.5-nikque.22 から。設定 `book-reduce-shrink`）。
- このとき、保存する book の局面の並びが変わります（中身は同じ）。

### 切り詰めるときの注意

**v4.5.5-nikque.13 から、`book subtree`・`book prune` で切り詰めた book は、そのまま Edax を終了すると、`book-file` のファイルに保存されます。** 切り詰める前の book は、上書きされます。`book reduce`（v4.5.5-nikque.20 から）も同じです。

- 切り詰める前の book を残したいときは、次のどちらかを行ってください。
  - **先にファイルのコピーを取る**。
  - 切り詰めた book を `book save 別の名前` で保存し、終了する前に `book load` で元の book を読み直す。
- `book depth` で深さの設定を変えただけでは、保存されません。

## 8.5 テキストファイルに書き出す・読み戻す

```
>book export data/my.txt
>book import data/my.txt
```

### book export

`book export` は、book を、1 行に 1 局面のテキストファイルに書き出します。

```
----------X--O----XXOX--XXXOO----OOOO-----O--------------------- X,8,F4,5
```

行の中身は、次のとおりです。

- 盤（64 マス）。**手番の側の石が `X`、相手の石が `O`** です。手番の欄はいつも `X` です。
- level
- Leaf の手
- その評価値

Link は書き出されません（取り込むときに張り直されます）。

ファイルの最後の行には、book の深さが書かれます（v4.5.5-nikque.13 から）。

```
% depth 12
```

### book import

`book import` は、そのファイルを読んで、**今の book と入れ替えます**。

- 読んだ後、Link の張り直し・点検・negamax・並べ替えを行います。
- 深さは、最後の行の `% depth` のとおりになります。深さ 12 の book を書き出して取り込むと、`Depth: 12` です。
- 取り込んだ book は、Edax を終了するときに `book-file` に保存されます。それまでの `book-file` のファイルは、`.old` を付けた名前で残ります（[6.6](06-book-basics.md)の注意 2）。
- ファイルが開けない、局面が 1 つもない、というときは、今の book を保ちます（`was not imported; current book retained` と表示）。
- 局面として読めない行は、飛ばして続けます。
  - Leaf の評価値が -64〜64 の外の行も、読めない行です。
  - v4.5.5-nikque.26 までは、たとえば 300 を 44 として読んでいました。
- Link を張り直すので、取り込んだ book は、書き出す前の book と一部の Leaf が違うことがあります。2,246 局面の book で試すと、取り込んだ後にもう一度書き出したファイルは、7 行が違いました。

**`% depth` の行がないファイル**を取り込んだとき：

- 対象は、v4.5.5-nikque.12 までの版が書き出したファイルや、自分で作ったファイルです。
- 深さは、**いちばん深い局面がちょうど入る深さ**になります。
- book を、決めた深さまで育て切っていなかったときは、元より浅くなります。たとえば、深さ 30 で作って 3 手目までの 4 局面しかない book を、古い版で書き出して取り込むと、`Depth: 3` になります。
- 取り込んだ後に `book info` で深さ（Depth）を確かめてください。違っていれば、`book depth 30` のように実行して直します。
- `% depth` の後ろが 1〜60 の数でないときは、`WARNING: wrong depth: …` と表示されて、行がないときと同じになります。

**v4.5.5-nikque.12 までの版で取り込んだとき**：

- 深さは、それより 1 つ大きくなります。深さ 12 まで育った book なら `Depth: 13` です。
- これらの版は、新しい版が書き出したファイルの最後の行を読めません。`WARNING: wrong board: % depth 12` と `WARNING: 1 lines of … hold no position: skipped` を表示して、その行を飛ばします（局面は、全部読まれます）。

**ファイルの大きさ**：大きな book では、テキストファイルは book のファイルよりずっと大きくなります。上の小さい book では、99KB に対して 170KB でした。ふだんの保存には `book save` を使ってください。

## 8.6 Leaf の評価値を計算し直す：book leaf recalculate

book の中の Leaf の評価値は、その Leaf を作ったときの探索の値です。

- `eval.dat` を別のものに替えると、新しく足す局面は、新しい評価関数の値になります。
- しかし、**前からある Leaf は古い値のまま**です。
- `book leaf recalculate` の仲間は、Leaf を探索し直して、今の評価関数の値に置き換えます。局面は足しません。

| コマンド | たどる範囲 | 計算し直す Leaf |
|---|---|---|
| `book leaf recalculate X Y` | `book deviate X Y` と同じ | `book deviate X Y` が「広げる」と選ぶ Leaf |
| `book leaf recalculate2 X Y` | `book deviate2 X Y` と同じ | `book deviate2 X Y` が「広げる」と選ぶ Leaf |
| `book leaf recalculate3 X Y` | `book deviate X Y` と同じ | たどった局面の全部の Leaf |
| `book leaf recalculate4 X Y` | `book deviate2 X Y` と同じ | たどった局面の全部の Leaf |

- X・Y の意味と、今の盤から始めることは、対応する `book deviate`・`book deviate2` と同じです（[7.3](07-book-learning.md)）。
- **完全読みの局面の Leaf は、計算し直しません**。評価関数によらず同じ値だからです。
- Leaf のない局面（全部の手が Link）も対象外です。
- `3`・`4` は、いちばん深い層（Link がなく Leaf だけの局面）の Leaf も計算し直します。
- 探索は、その局面に記録されている level で行います。同時に行う数は、`book-expand-tasks` に従います。
- 途中経過と結果は、`<book のファイル名>.leaf`・`.leaf2`・`.leaf3`・`.leaf4` に保存されます。
- **途中で保存されたファイルは、「一部の Leaf だけ新しく、negamax はまだ」の状態**です。読み込んで使うときは、`book negamax` を実行してください。
- **途中で保存された book から、続きを計算できます**（v4.5.5-nikque.29 から）。くわしくは下の「途中から続ける」を見てください。

表示の例（小さい book）：

```
>book leaf recalculate4 2 4
Book leaf recalculate4 2 4:
Book leaf recalculate4 123 todo
Book leaf recalculate4...
Book leaf recalculate4 2 4...finished: 123 leaves, 0 scores changed (0 up, 0 down, largest +0 / -0), 0 moves changed
```

最後の行には、次の数が出ます。

- 計算し直した Leaf の数
- 評価値が変わった数（上がった数・下がった数・いちばん大きな変化）
- 手が変わった数

### 途中から続ける（v4.5.5-nikque.29 から）

計算し直した Leaf には、book の中で**マーク**が付きます。

途中で止まったとき（Edax を閉じた、PC が止まった など）は、次のように続けます。

1. 途中経過のファイル（`.leaf` など）か、終了のときに保存された book を、`book load` で読みます。
2. **同じコマンドをもう一度**実行します。マークの付いた Leaf は計算し直さず、残りの Leaf だけを計算します。始めに、マークの付いた数が出ます。

```
Book leaf recalculate4: 1234 leaves already searched again by a run that did not end: they are not searched again
```

- **コマンドが最後まで終わると、マークはすべて外れます**。終わった book には、マークがありません。
- 同じ実行の中でも、前の回で計算し直した Leaf は、後の回で計算し直しません。
  - `book-leaf-recalculate-rounds` が 2 以上のときです。
  - 探索が 1 スレッドなら、計算し直しても同じ値です。
- `leaf recalculate`・`2`・`3`・`4` は、同じマークを使います。途中で止まった後は、同じコマンドと同じ X・Y で続けてください。
- マークは、book のファイルの各局面の level の欄（1 バイト。level は 0〜60）の、上の 2 ビットに書きます。ファイルの大きさは変わりません。
- **マークの付いた book は、v4.5.5-nikque.28 までの版では読めません**（壊れた book として扱われます）。
  - マークの付いた book を古い版やほかのツールで使いたいときは、先に新しい版で、コマンドを最後まで終わらせてください。
- v4.5.5-nikque.28 までの版には、マークがありませんでした。もう一度実行すると、最初から計算し直していました。

### 1 回では足りないことがある：book-leaf-recalculate-rounds

このコマンドは、既定では **1 回たどって終わり**ます。

- Leaf を計算し直すと評価値が動き、「たどる範囲」も動きます。
- そのため、同じコマンドをもう一度実行すると、1 回目には範囲の外だった Leaf が、新しく対象になります。

設定 `book-leaf-recalculate-rounds = n`（v4.5.5-nikque.13 から）にすると、「たどる → 計算し直す → negamax」を最大 n 回繰り返します。**Leaf が 1 つも変わらなかった回で終わります**。既定は 1 です。

- テストと実測の記録（`docs/validation/`） の例（649 万局面の book、`book leaf recalculate4 1 1`）：
  - 1 回ずつ繰り返すと、対象は 4,066 → 780 → 247 → 485 → 386 でした。
  - 評価値が変わった数は、1,419 → 411 → 42 → 46 → 0 でした。
  - `book-leaf-recalculate-rounds = 10` にすると、5 回で終わりました。
- 複数スレッドの探索は、実行ごとに値が少し変わります。そのため、「変わらなかった回」が来ないことがあります。そのときは n 回で終わります。
- **`eval.dat` を替えていなくても、計算し直すと値が変わる Leaf があります**。作ったときの探索と、後から単独で行う探索では、条件が少し違うためです。くわしい数字は、テストと実測の記録（`docs/validation/nikque.ja.md`）の v4.5.5-nikque.12・13 の節にあります。

### eval.dat を替えたときの手順の例

1. book のファイルのコピーを取る。
2. 新しい `eval.dat` で Edax を起動する（`-eval-file` か、ファイルの置き換え）。
3. `init` の後、`book leaf recalculate4 X Y` を実行する。X・Y は、ふだん `book deviate2` に使っている数にする。
4. 終わったら `book info`・`book show` で確かめ、`book save` で保存する。

時間は、Leaf の数に比例します。実測の記録（6 億 6,162 万局面の book、level 18、32 スレッド）は次のとおりです。

- `book leaf recalculate2 5 5` の対象は 275 万件でした。
- 探索の速さは、1 分に約 2,350 件でした。
- 全部を行った時間は測られていません。

## 8.7 そのほかの保守

### book correct：完全読みの局面を確かめ直す

```
>book correct
Correcting solved positions...
Correcting solved positions...0 done (0 error found)
 …
```

完全読みの範囲にある局面の Leaf を探索し直し、記録と違っていたら直します。完全読みの範囲にある局面とは、その局面の level で、空きの数が完全読みになる局面です。

- その後、`book fix` と同じ処理を行います。
- 直した数が `error found` に出ます。
- 途中経過は `.err` に保存されます。
- **途中で保存された book から、続きを確かめられます**（v4.5.5-nikque.29 から）。
  - 確かめ直した局面には、マークが付きます。
  - 途中で止まったときは、`.err` か、終了のときに保存された book を `book load` で読みます。そして、もう一度 `book correct` を実行します。
  - マークの付いた局面は飛ばし、残りだけを探索します。始めに次のように出ます。

```
Correcting solved positions: 50000 already done by a run that did not end, 150000 left
```

- **全部の局面を確かめ終わると、マークはすべて外れます**。
- マークのしくみと、古い版で読めないことは、8.6 の「途中から続ける」と同じです。
- v4.5.5-nikque.28 までの版には、マークがありませんでした。もう一度実行すると、完全読みの局面を全部、最初から探索し直していました。大きな book では、これに何日もかかります。そのため、途中で止まると、それまでの探索が無駄になっていました。

正しく作られた book なら、`0 error found` になるはずのコマンドです。古い版の不具合や、途中で止めた探索の結果が入った疑いがあるときに使います。

**v4.5.5-nikque.12 までの版では、このコマンドが book の評価値を壊していました。**

- 終局の局面とパスの局面の Leaf を消していました。`error found` に大きな数が出て、評価値が ±127 になります。
- v4.5.5-nikque.13 で直っています。
- 古い版で `book correct` をかけて壊れた book は、v4.5.5-nikque.13 の `book correct` をかけると直りました（テスト用の小さい book での確認です）。

### book deepen：level の低い局面を、book の level で探索し直す

対象は、level が book の level より浅い局面です。その局面の level を book の level に上げて、Leaf をその level で探索し直します（v4.5.5-nikque.29 から）。

- 「浅い」とは、その局面の空きの数で、次のどちらかになることです。
  - 探索の深さが小さい
  - 深さが同じで、枝刈りが多い（selectivity が低い）
- book の level より深い局面は、そのままにします。
- 探索し直した局面は、level が book の level になります。
- 途中で止まったときは、途中経過のファイル（`.dep`）か、終了のときに保存された book を読みます。そして、もう一度 `book deepen` を実行すると、残りの局面だけを探索します（level そのものがマークになります）。
- book の level は、`book new` で決めた値か、`book import` で読んだ局面の level のいちばん高い値です。
- book の level を変えるコマンドはありません。そのため、使う場面は、level の違う局面が混ざった book に限られます（level の違う book を `book merge` したもの、取り込んだもの）。

**v4.5.5-nikque.28 までの版では、このコマンドは働きませんでした。**

- level が book の level と違う局面の Leaf を探索し直していました。しかし、探索は「その局面の今の level」で行い、level も書き換えませんでした。
- 何も深くなりませんでした。もう一度実行すると、同じ局面を全部、探索し直しました。
- これは元の Edax（4.3）からの実装です。

### book verbose：表示の量

| 値 | 表示 |
|---|---|
| `0` | book のコマンドの表示をなくす |
| `1`（ふつう） | 進み具合を表示する |
| `2` | 探索の表も表示する |

### book feed-hash

今の盤の局面から先の、book の評価値を、探索のハッシュ表に入れます。次の探索が、book の値を使えるようにするためです。何も表示されません。

## 8.8 book から取り出す

### book extract：最善の手順を棋譜にする

```
>book extract data/lines.txt
49 games extracted
```

初期局面から、各局面で「評価値がいちばん良い手」（同点なら全部）をたどった手順を、棋譜のファイルに書き出します。形式は拡張子で決まります（[10 章](10-files.md)）。

- **出力先のファイルがすでにあると、その後ろに追記されます。**
- 新しいファイルのときは、`WARNING: Cannot open file data/lines.txt` と表示されますが、ファイルは作られます。「前からあるファイルを読もうとして、なかった」という意味の表示です。

### book problem：局面を問題として取り出す

```
>book problem 52 3
Extracting 3 positions at 52 ...
-----------------OOOO----XOXXX----OOO--------------------------- X % bm E2:+1; ba C6:-2;
```

book の中から、空きが 52 で、最善の手が 1 つに決まっている局面を、3 つまで画面に表示します。「最善の手が 1 つに決まっている」とは、2 番目の手より評価値が良いことです。

- 数を省くと、空き 24・10 個です。
- 選ばれるのは、book の中の並びで先にあるものです。
- 行の中身は、盤、手番、`bm`（最善の手と評価値）、`ba`（2 番目の手と評価値）です。

## 8.9 この章のコマンド

| コマンド | 意味 |
|---|---|
| `book negamax` | 評価値を計算し直して、手を並べ替える |
| `book fix` | 点検、Link の張り直し、negamax、並べ替え |
| `book merge ファイル` | 別の book を取り込む |
| `book depth n` | 深さの設定を変える |
| `book subtree` | 今の盤の局面から先だけを残す |
| `book prune` | 片方が最善を打つ変化だけを残す |
| `book reduce X Y` | 今の盤の局面から `book deviate3 X Y` でたどれる局面だけを残す |
| `book export ファイル`・`book import ファイル` | テキストファイルに書き出す・読み戻す |
| `book leaf recalculate`（`2`・`3`・`4`）`X Y` | Leaf の評価値を計算し直す |
| `book correct` | 完全読みの局面を確かめ直す |
| `book deepen` | level の低い局面を book の level で探索し直す |
| `book extract ファイル` | 最善の手順を棋譜のファイルに書き出す |
| `book problem 空き 個数` | 局面を問題の形で表示する |
| `book feed-hash` | book の値をハッシュ表に入れる |
| `book verbose n` | 表示の量 |

次：[9. 大きな book を扱う](09-large-books.md)
