# Edax 4.5.5 nikque 修正一覧

[English](RELEASE-NOTES.md) · [README](README.md) · [マニュアル](manual/ja/README.md) · [テストと実測の記録](docs/validation/)

版ごとの利用者向けの変更点です。各版の「互換性」に、探索の結果・`eval.dat`・book のファイル形式が前の版から変わったかどうかを書いています。テストと実測の詳しい記録は、各版の末尾のリンク先（`docs/validation/`）にあります。

- [nikque 系列](#nikque-系列)（本線。既定のブランチ `edax-4.5.5-fixes`）
- [eval2 系列](#eval2-系列)（新しい評価関数。ブランチ `eval2`）
- C++23 版の Heath（旧名 newedax）は [Heath の修正一覧](https://github.com/Nikque/Heath/blob/main/RELEASE-NOTES.ja.md)

## 未公開の変更

- libedax の `edax_stop` は、ヒントまたはベンチマークの次の探索が始まる直前の要求も引き継ぎます。停止時は `edax_hint` に完了済みのヒントだけが残り、`edax_hint_next` と `edax_hint_next_no_multipv_depth` は中断した探索について `LIBEDAX_NOMOVE` を返します。中断したベンチマーク局面は件数に含めません。

## 版の対応

eval2・Heath（旧名 newedax）の各版は、同じ行の nikque の版の変更をすべて含みます。

| nikque | eval2 | Heath (newedax) |
|---|---|---|
| nikque.33 | eval2.20 | heath.20 |
| nikque.33-pre1 | eval2.20-pre1 | heath.20-pre1 |
| nikque.32 | — | heath.19 |
| nikque.31 | eval2.19 | heath.18 |
| nikque.30 | eval2.18 | heath.17 |
| nikque.29 | eval2.17 | heath.16 |
| nikque.28 | eval2.16 | newedax.15 |
| nikque.27 | eval2.15 | newedax.12 |
| nikque.26 | eval2.14 | newedax.11 |
| nikque.25 | eval2.13 | newedax.10 |
| nikque.24 | eval2.12 | newedax.9 |
| nikque.23 | eval2.11 | newedax.8 |
| nikque.22 | eval2.10 | newedax.7 |
| nikque.21 | eval2.9 | newedax.6 |
| nikque.20 | eval2.8 | newedax.5 |
| nikque.19 | eval2.7 | newedax.4 |
| nikque.18 | eval2.6 | newedax.2・newedax.3 |
| nikque.16・17 | eval2.5 | newedax.1 |
| nikque.15 | eval2.4 | — |
| nikque.14 | eval2.3 | — |
| nikque.10〜13 | eval2.2 | — |
| nikque.9 | eval2.1（eval2 系列の最初） | — |
| nikque.2〜8 | — | — |

---

# nikque 系列

## v4.5.5-nikque.33

v4.5.5-nikque.33-pre1 に、Heath の探索の改良を入れた正式版です。持ち時間の配り方と手を読む順番が変わります。probcut の 3 つの改良は既定では無効で、`config.ini` で使えます。

**変更**

- v4.5.5-nikque.33-pre1 の変更をすべて含みます。
- 次の 2 つは、設定はなく、いつも有効です。時間と手を読む順番だけが変わります。
  - 持ち時間の配り方：1 手の時間は Edax と同じです。読みの値が下がった（fail low）ときだけ、最大 2 倍まで延ばします。
  - 手を読む順番：深さ 7 から評価値で並べます。評価の重みの割り方は 1,0,0 です。
- 次の 3 つの設定を足しました。既定は `off` です。`on` は Heath と同じ値で、数値でも書けます。

| 設定 | 内容 | `on` の値 |
|---|---|---|
| `pc-slope` | 最後まで読む読みの probcut。浅い読みの値を傾きの分だけ広げて読み、幅も 0.85 倍にする | `1.3,12` |
| `pc-dev` | 読み筋から外れた手の probcut の幅を狭める | `2,0.04,0.75` |
| `pc-mob` | 着手可能数で probcut の幅を変える | `2.219,-0.184,-0.350,0,0,0,0,0,0` |

- `lazy` に `auto` を足しました。4 スレッド以上の中盤の読みで、根の局面を深く読む補助のスレッドを 1 本使います。既定は `0` です。
- テスト用の設定を消しました。`-corr`・`-corr-end`・`-time-model`・`-time-ext-hard`・`-time-sep`・`-time-sep-depth`・`-time-close`・`-time-close-empties` です。
- 2 つを固定したので、その設定も消しました。`-time-ext`・`-sort-eval-shift`・`-sort-min-depth` です。

**互換性**

- 既定の設定でも、手を読む順番が変わるので、ノード数は変わります。評価値と選ぶ手はほとんど同じですが、probcut を使う読みでは、まれに評価値が変わります。それで学習した book も、まれに値が変わります。
- 持ち時間の対局では、読める深さが変わります。
- `pc-slope`・`pc-dev`・`pc-mob` を `on` にすると、評価値と選ぶ手が変わります。それで学習した book も変わります。
- `eval.dat` と book のファイル形式は前の版と同じです。

**注意**

- 消した設定をコマンド行に書くと、使い方を表示して終わります。それらを渡しているスクリプトは直してください。

**未確認**：3 つの設定の値は Heath の対局で決めました。この版での効きと強さは測っていません。

[検証記録](docs/validation/nikque.ja.md#v455-nikque33)

---

## v4.5.5-nikque.33-pre1

Edax 4.7 の book・base の機能、探索の新しい設定、不具合の修正を入れたプレリリースです。

**変更**

| コマンド | 内容 |
|---|---|
| `book play n` | 今の局面から、手順の先端を空き n まで伸ばす。途中から続けられる |
| `book extend` | 今の局面から、どの Link にも劣らない Leaf を広げる |
| `book exhaust` | 今の局面から book の深さまで、すべての手を足す |
| `book extract all`・`book extract draw` | すべての手順、または最善の手順を棋譜に書き出す |
| `base append` | 棋譜を別のファイルの後ろに追加する |

- `book play` は、`book-expand-tasks` に従って同時に広げ、`book-save-interval` の時間ごとに保存します。
- `base complete` は、`book-store-tasks` に従って複数の対局を同時に進め、途中から続けられます。
- libedax に、これらに当たる関数を 5 つ足しました。
- `symetry` を `symmetry` とも書けます。

- AVX-512 版の中盤の探索を速くしました。1 スレッド、level 21 のテストで、CPU 時間は 0.90 倍です。メモリの使用量は約 28 MB 増えます。
- book の読み込みを速くしました。テストの CPU 時間は、並びのそろった book で 0.94 倍、そろっていない book で 0.63 倍です。
- book と base の並べ替えを速くしました。結果は変わりません。

次の探索の設定を足しました。どれも既定は無効です。強さは未確認です。

| 設定 | 内容 |
|---|---|
| `-lazy n` | 空いているスレッド n 本が根の局面を先に読む |
| `-sort-eval-shift a,b,c`・`-sort-min-depth n` | 手の並べ方 |
| `-lmr 1`・`2` | 後ろの手を浅く読む |
| `-corr on` | ProbCut の判定に使う静的評価を補正する |
| `-pc-r`・`-pc-end`・`-pc-dev`・`-pc-mob`・`-pc-gate` | ProbCut の幅と判定 |
| `-time-model 1`・`-time-ext 1`・`-time-sep n`・`-time-close 1` | 持ち時間の対局での時間の配分 |
| `-split-probe off` | ProbCut の下読みの中では探索を分割しない |

**不具合の修正**

- 持ち時間で読む多スレッドの探索で、時間を延長した直後に探索全体が止まることがありました。延長した時間を分割した探索にも効かせます。
- パス（`pass`）のある `.pgn` を読むと、その対局の残りと次の対局が失われることがありました。パスを読めるようにしました。
- 標準の入力の `stop` と XBoard の `?` が、探索の開始の直前に届くと失われることがありました。届いた時点で止まります。GGS と DLL の `edax_stop` は、この修正の対象外です。
- 先読みの最中に新しい対局を始めると、先読みが終わるまで待たされました。先読みを止めてから始めます。
- `book learn` は、`.store` の保存の前後に止まると続きから学習できないことがありました（v4.5.5-nikque.29・eval2.17・heath.16 から）。どこで止まっても続けられます。前の版が書いた `.learn` も使えます。
- NBoard で `go` の直後の `ping` が探索を止めませんでした。探索を止めて手を返します。
- 並びのそろっていない book の読み込みで、局面のメモリが最大で約 2 倍になりました。そろった book と同じ量にしました。
- `eval.dat` の読み込みでメモリを確保できないと、説明なしに異常終了しました。エラーで終わります。
- eval2 用の `eval.dat` を読んでも警告しませんでした。警告を表示します。
- `base convert`・`base unique` は、次のときに何も書かないようにしました。
  - 入力や出力先の棋譜を読み切れないとき。
  - 入力と出力が同じファイルのとき。
- 棋譜の保存は、別のファイルに書き終えてから入れ替えます。途中で止まっても元のファイルが残ります。
- `WTHOR.JOU` の名前の後ろに不定のデータが書かれていました。0 で埋めます。
- `book extract` は、書き出しに失敗したら知らせて、元のファイルを残します。
- `book play` が設定にかかわらず 1 局面ずつ探索し、同じ局面を重ねて探索していました。どちらも直しました。
- `book deviate`・`deviate2`・`deviate3`・`book enhance`・`book leaf expand` が、同じ局面を重ねて探索していました。1 回だけ探索します。
- `book enhance` が book を 1 スレッドでたどり、設定にかかわらず毎周保存していました。`n-tasks` のスレッドでたどり、`book-deviate-save-rounds` に従って保存します。
- `book extract` が、手順の合流の先を合流のたびに書いていました。合流した所で手順を終えます。
- 壊れた盤（同じマスに両方の石、石が 4 つ未満）のある book を読み込めてしまいました。読み込みを断ります。
- Windows で `\` 区切りの名前に棋譜を書くと、`WTHOR.JOU` が同じフォルダーに置かれませんでした。同じフォルダーに置きます。

**互換性**

- 前の版の `eval.dat` と book を引き続き使えます。book のファイル形式は同じです。
- レベルや深さを指定した探索の評価値と最善手は、既定の設定では前の版と同じです。
- 持ち時間で読む多スレッドの探索は、不具合の修正により結果が変わることがあります。
- `book play` と `base complete` は、同時に進めると結果が前の版と変わることがあります。
- `book deviate` 系列・`book enhance`・`book leaf expand` でできる book は、重複した探索をやめたため、前の版と少し変わることがあります。
- `book extract` が書き出す手順は、合流した所で終わるため、前の版より短く、少なくなることがあります。
- `book play` では、初期局面からたどれない局面の評価値が前の版と変わることがあります。
- `book play` は毎周の保存をやめました。途中で止まると、最大で `book-save-interval`（既定 60 分）分を失います。
- 棋譜の保存中は、ファイル 2 つ分の空き容量が必要です。
- `book play n` は `.play-to`、`base complete` は `.completing` の付いた途中経過のファイルを作ります。続ける前に消さないでください。
- 数なしの `book play`、`book extract`、`base convert`・`base unique` は前の版と同じ意味です。Edax 4.7 とは意味が違います。

**注意**

- Linux 版の実行には glibc 2.38 以上が必要です。
- テスト用のプレリリースです。正式版までに設定名と既定値が変わることがあります。
- 大事な book は、コピーを取ってから使ってください。
- `book learn` を続けるときは、先に `.store` を `book load` で読んでください。読まずに実行すると `.learn` が上書きされ、その `.store` から続けられなくなることがあります。

**未確認**：時間配分の設定、`-pc-end` と `-split-probe` の単独、対局での強さ、32 スレッド、大きな実物の book、Android 実機、Linux・macOS 版の実行、GUI からの利用。

[検証記録](docs/validation/nikque.ja.md#v455-nikque33-pre1)

---

## v4.5.5-nikque.32

終盤の完全読みを速くしました。

**速さ**

- 空き 5〜6 マスの探索で、区画（盤の 4 分の 1）に 1 つだけ残った空きを先に打ち、スレッドごとの小さな置換表（2^14 件）を引くようにしました。考え方は KUROOBI（MIT）によります。
- 1 スレッドの完全読みで、fforum-40-59 が 0.978 倍の時間（431.4 秒 → 421.7 秒）、fforum-20-39 は探索する局面が 89.1% になりました。評価値と最善手はすべて前の版と同じです。
- 中盤の探索は変わりません（読み切りに入らない局面では、探索する局面の数が前の版と同じです）。

**互換性**

- 前の版の `eval.dat` と book を引き続き使えます。探索・学習の設定も前の版と同じです。読み切りの結果は同じで、探索する局面の数と時間が変わります。

**未確認**。32 スレッドなど多スレッドでの速さ、実 book、対局での強さ。

[検証記録](docs/validation/nikque.ja.md#v455-nikque32)

---

## v4.5.5-nikque.31

棋譜データベースの完了件数、手順数の見積もり、外部プロトコルとヘルプの不具合を修正しました。

**不具合の修正**

- `base complete` の最終表示で、棋譜の総件数を正しく表示します。
- `estimate` では、各試行の合計から全体の誤差を計算します。合計の誤差が `NaN` になることがありました。
- GTP の `genmove` と `reg_genmove` は、終局で `pass` を返します。履歴のない終局では、以前は `genmove` で終了することがありました。
- XBoard は解析中も `q` で終了します。GTP と NBoard は `q` を未認識と応答した後も入力を受け付けます。
- GGS は標準入力が閉じても接続を続けます。終了の通知をサーバーにコマンドとして送っていた問題を修正しました。
- `help test` に、OBF の問題を解く `solve <file> [hard-file]` を掲載しました。
- 棋譜の `load` と `save` のヘルプに、ファイル名が必須と明記しました。

**互換性**

- 前の版の `eval.dat` と book を引き続き使えます。探索・学習の設定も前の版と同じです。

**未確認**。実 book、対局での強さ、実際の GGS サーバーとの通信。

[検証記録](docs/validation/nikque.ja.md#v455-nikque31)

---

## v4.5.5-nikque.30

終盤の完全読みに使う置換表の不具合を修正しました。最終手の石返しと、探索開始時の確定石計算も見直しました。

**変更**

- BMI2 を使う版では、中央 16 マス以外の最終手で対角線のビット抽出を 2 回から 1 回に減らしました。
- AVX2 を使う版では、探索開始時に求めた埋まったラインを両色の確定石計算で共有します。
- SVE 対応ビルドでは、ベクトル長が 256 ビット以上のときに SVE で評価特徴を更新します。

**不具合の修正**

- 度外石の色をそろえた盤面を終盤の置換表のキーに使うと、勝者が変わる局面で空きマスの配点を誤り、完全読みの探索境界がずれることがありました。実際の盤面をキーに使うようにしました。

**互換性**

- 修正の影響を受ける局面では、探索結果や探索ノード数が変わることがあります。評価関数、`eval.dat`、book のファイル形式は前の版と同じです。

**未確認**。対局での強さ、実 book、SVE の実機での動作と速度。

[検証記録](docs/validation/nikque.ja.md#v455-nikque30)

---

## v4.5.5-nikque.29

`book correct`・`book leaf recalculate`・`book learn` を、途中で保存された book から続けられるようにしました。`book deepen` が働くようになりました。

**変更**

- `book correct` が、途中で保存された book から続けられるようになりました。
  - 確かめ直した完全読みの局面には、book の中にマークが付きます（book と一緒に保存されます）。
  - 途中で保存された book（`<book-file>.err` への時間ごとの保存、終了のときの保存）でもう一度実行すると、マークのない局面だけを探索します。
  - 全部の局面を確かめ終わると、マークはすべて外れます。
  - v4.5.5-nikque.28 までは、実行するたびに完全読みの局面を全部、最初から探索し直していました。大きな book では何日もかかるため、途中で止まると、それまでの探索が無駄になっていました。
- `book leaf recalculate`（2・3・4）も同じように続けられます。
  - 計算し直した Leaf にマークが付きます。もう一度実行すると、マークのある Leaf を飛ばします。コマンドが終わると、マークは外れます。
  - 同じ実行の中でも、前の回で計算し直した Leaf は、次の回で計算し直しません（`book-leaf-recalculate-rounds` が 2 以上のとき）。1 スレッドの探索なら同じ値でした。
- `book learn <ファイル>` が、学習済みの行から続けられるようになりました。
  - `<book-file>.store` に保存するたびに、小さなテキストファイル `<book-file>.learn` に、棋譜のファイルの何行目まで学習したかを書きます。その行の確かめの値と、book の局面の数・level・深さも書きます。
  - 同じファイルでこの book に `book learn` を実行すると、学習済みの行を飛ばします。
  - ファイルの最後まで学習すると、`.learn` は消えます。
  - v4.5.5-nikque.28 までは、もう一度実行すると全部の棋譜を打ち直していました（`book-randomness` があると別の対局になります）。

**不具合の修正**

- `book deepen` が、何も深くしていませんでした（元の Edax 4.3 からの実装）。level が book と違う局面を、その局面の今の level で探索し直し、level も書き換えていませんでした。もう一度実行すると、全部を探索し直していました。
- 今は、level が book の level より浅い局面に book の level を与え、その level で探索し直します。もう一度実行しても探索し直しません。深い level の局面はそのままです。

**互換性**

- マークは、book のファイルの各局面の level の 1 バイト（level は 0〜60）の上の 2 ビットです。ファイルの大きさと形式は変わりません。マークのない book は前と同じです。
- **`book correct`・`book leaf recalculate` が終わる前に保存された book にはマークがあります。v4.5.5-nikque.28 までの版は、この book を壊れた book とみなし、読み込めません。** 終わった book にはマークがありません。
- book のファイルを自分で読むほかのツールは、この 2 ビットを無視する必要があります（nikque GUI 0.4.6 は対応済み）。
- `eval.dat` と探索は変わりません。評価値・手・ノード数は v4.5.5-nikque.28 と同じです。
- ほかのコマンドが作る book は v4.5.5-nikque.28 と同じです。

**未確認**：実際の大きな book、強さ（探索は変えていません）。

[検証記録](docs/validation/nikque.ja.md#v455-nikque29)

---

## v4.5.5-nikque.28

空き 17 マス以上の level 60 の完全読みで、まれに評価値（と手）を誤る不具合を直しました。ほかは変わりません。

**不具合の修正**

- 空き 17 マスの完全読みの 10 万回に 1〜2 回ほど、根の評価値と手を誤っていました（誤りは数石）。1 スレッドでも 4 スレッドでも、置換表の大きさによらず、同じ局面で毎回起きていました。
- 原因：完全読み（深さが空きマス数で選択なしの level）は、まず選択あり（probcut）で読み、その後に完全に読みます。その間、中盤の探索で終盤の探索に移る深さ（`DEPTH_MIDGAME_TO_ENDGAME`、空き 15）の節を、正確な値として置換表に残していました。子が終盤の探索で読まれるためです。
- その節がパスのときは、相手の探索が中盤の探索の同じ深さのまま続き、probcut で打ち切られることがあります。その値が正確な値として残り、完全な読みの反復で使われていました。
- パスの節は、読みの選択の強さのまま残すようにしました（`midgame.c` の `NWS_midgame`）。

**互換性**

- `eval.dat`・book のファイル形式は変わりません。
- 正しかった完全読みの評価値は変わりません。ノード数は少し変わります（fforum-20-39 を 1 スレッドで 77,639,600、v4.5.5-nikque.27 は 77,639,567。評価値と手は同じ）。
- 終盤の深さに届かない中盤の探索は変わりません。

**注意**

- 前の版で完全読みした局面（たとえば level 18 の空き 20 マス以下の葉）を含む book には、まれにこの誤りの値が入っていることがあります。
- 同じ誤りは、この版の元の奥原俊彦氏の Edax 4.5.5・4.5.6（edax-reversi-AVX、2026-11-03 の master も）にもあります。Richard Delorme 氏の Edax 4.6・4.7 にはありません。

**未確認**：強さ、実 book。

[検証記録](docs/validation/nikque.ja.md#v455-nikque28)

---

## v4.5.5-nikque.27

起動のときにほかのプログラムが開いていた book のファイルを、壊れたものとして置き換えることがなくなりました。見直しで見つけたほかの不具合も直し、動きを変えずにコードを整理しました。

**変更**

- libedax：`edax_book_file_status(file)` を足しました。読み込むはずだった book のファイルがどうなったかを返します。
  - 0：置き換えてよい。1：読めなかった（壊れている）。2：開けなかった（置き換えない）。
  - 初期化の後に呼べば、空の book に学習を始めずに止められます。
- `book deviate` の仲間の `export`：ファイル名を `"…"` で囲めば、空白を含められます。
- `book reducecount` の数えた結果は、メモリを要るほかの解析に譲ります（その後の `book reduce` は数え直し、結果は同じです）。
- `book.c`・`book_analyze.c`・libedax・`play.c`・`options.c`・`base.c`・`util.c` とほかのいくつかのファイル、テストのコードを、動きを変えずに整理しました（何度も書いていた処理を共通の関数に）。
- マニュアル：4.1・6.6・7.3・8.4・8.5・12.2・12.4・13.1 に書きました。

**不具合の修正**

- 起動のときに開けない book のファイル（ほかのプログラムが共有なしで開いている、読む権限がない、など）を、nikque.26 までは壊れたものとして扱っていました。後の保存で `.damaged` に移し、新しい book で置き換えていました。
  - 今は、2 秒ほど待って開き直します。
  - それでも開けなければ、これまでどおり新しい book で始めますが、**そのファイルには保存しません**（`book save` でも、終了のときも）。`.damaged` も作りません。
  - 表示は `Cannot open opening book …; it is not loaded, and no save replaces it` です。ほかのプログラムが閉じた後に `book load` で読めます。
- 別の book のファイルの読み込みに失敗すると、最初に読めなかったファイルを、後の保存が `.damaged` に残さずに置き換えていました。今は、読めなかったファイルをそれぞれ、読み込むか退けるまで覚えています。同じファイルを別の書き方の名前（`./`、`/` の代わりの `\`、Windows での大文字と小文字、絶対の名前）で渡しても見つけます。
- libedax：日本語などの名前（ASCII でない名前）の book のファイルが起動のときに開けなかったとき、終わりに 1 局面の book で置き換えていました。
- libedax：`edax_book_load`・`edax_book_merge` の後に、`edax_book_deviate_export` で誤りなしで手順が抜けることがありました。今は、書き出すたびに今の book の局面に番号を付けます。
- libedax：`edax_hint` の 2 つの探索の間に来た `edax_stop` を忘れなくなりました。
- libedax：`edax_book_add_board` は、前の book の関数の失敗の印を消します。
- `edax_book_new` と `book new` は、深さを `book depth` と同じく 1〜61 に収めます。
- `book import`：Leaf の評価値が -64〜64 の外の行を、誤って読んでいました（300 を 44 として）。今は読めない行として飛ばします。
- `book merge`：ほかのツールが別の向きで書いた局面へ進む手が、merge の後にありませんでした。今は、その局面を探す所に入れ、そこへの手もつなぎます。
- `book merge`：同じ局面を 2 つの向きで持つファイルは断ります（`duplicated position`）。逆の順のときは前から断っていました。
- `book deviate` の仲間の `export`：`exprot` のような打ち間違いを読み飛ばして book を広げ、長い名前は黙って切っていました。今は、長すぎる名前、閉じていない引用符、数の後ろの `export` のほかの語は誤りとし、book を広げません。
- `book deviate` の仲間の `export`：合計の損失が 254 を超えたときの警告は、libedax でもコマンドと同じです。
- `book moveinfo`：メモリが足りないとき、調べられなかった手の行を出さないまま、誤りなしで終わっていました。今は `@error id=N memory=1` と `@end id=N error=1` で終わります。
- 2^31 個を超える項目をたどるときは、誤った親をたどらずに、誤りで止まります。
- 時間の `5:`（対局の持ち時間、xboard の level、GGS の時計）を 5 分 5 秒と読んでいました。今は 5 分と読みます。
- 実行ファイルのフォルダーの名前が長すぎて `config.ini` の場所を作れないとき、カレントフォルダーの `config.ini` を読み、誤った警告を出していました。今は `config.ini` を読まず、そう知らせます。
- 元の Edax の部分：
  - `weval` の表と統計（`histogram`）が、局面が 0 か 1 の行・列・表で 0 で割らなくなりました（`nan`・`inf`）。
  - `eval_builder` の使い方の文を、実際の読み方に合わせました。
  - GTP の `time_left` は、ほかのコマンドと同じく誤った色を知らせます。

**互換性**

- 探索の結果・`eval.dat`・book のファイル形式は変わりません。
- 答えが nikque.26 と違うのは、この版の **変更**・**不具合の修正** に書いた場合だけです。
- book のエラーの文に入るソースの行番号（`book.c : book_load : …`）は変わりました。
- libedax：設定の book のファイルを起動のときに開けなかったときは、どの保存もそのファイルを置き換えません（`edax_book_save_checked`・`edax_book_keep_unread_file` は 0）。

**注意**：libedax で book を自分で保存するプログラムは、初期化の後に `edax_book_file_status` を確かめてください。

**未確認**：実 book での時間とメモリ。

[検証記録](docs/validation/nikque.ja.md#v455-nikque27)

---

## v4.5.5-nikque.26

`book deviate`・`book deviate2`・`book deviate3` に、book を広げずに、たどる手順をテキストファイルに書き出す `export` を足しました。

**変更**

- 数の後ろに `export <ファイル> [leaf|ends|all]` を付けると、そのコマンドが今の局面からたどる手順を、book を広げずに書き出します（例 `book deviate2 2 4 export lines.txt ends`）。

| 指定 | 書き出す局面 |
|---|---|
| `leaf`（省いたとき） | 1 周目に広げる Leaf を持つ局面 |
| `ends` | 進行の端の局面 |
| `all` | 進行の局面すべて |

- 1 行に 1 局面で、初手からの手順を小文字で書きます（パスは書きません）。
- 合流した局面は 1 回だけ書きます。その手順は、`book deviate` では手数が、`book deviate2`・`book deviate3` では失った石数の合計が、いちばん少ないものです。
- libedax：`edax_book_deviate_export` を足しました。
- 解析の `stop id=N` で止めるものがないときの答えを、`@cancel id=N state=none export=1` にしました。`export=1` は、書き出しを持つ版であることの印です。
- マニュアル：7.3（書き出し）と 12.4（`export=1`）に書きました。`help book` にも足しました。

**互換性**

- `export` を付けないコマンドは nikque.25 と同じです。
- `@cancel id=N state=none` の答えの後ろに `export=1` が付きます。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**注意**：前の版は `export` 以降の語を読まずに book を広げます。GUI は、`export=1` を確かめてから `export` を送ってください。

**未確認**：実 book での時間とメモリ。

[検証記録](docs/validation/nikque.ja.md#v455-nikque26)

---

## v4.5.5-nikque.25

うみがめ数の `exact=0` が付く値が、ビルドした環境（C ライブラリ）によらず同じになるようにしました。

**変更**

- マニュアル：12.4 に書きました。

**不具合の修正**

- `exact=0` が付くうみがめ数の値が、Windows と Linux などで違うことがありました。並べ替え（`qsort`）が、同じ値の項の順を決めていなかったためです。
- うみがめ数で項（数え方の候補）を 128 より多く持てずに落とすとき、同じ大きさの項の並びを決めました。残る項が、どの環境でも同じになります。

**互換性**

- `exact=0` が付かないうみがめ数と、ほかの答えは nikque.24 と同じです。
- `exact=0` が付く値は、nikque.24 と変わることがあります（どちらも上限です）。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**未確認**：実 book での速さ、GUI での動き。

[検証記録](docs/validation/nikque.ja.md#v455-nikque25)

---

## v4.5.5-nikque.24

GUI の候補手の表のうみがめ数・局面の数と、`book levels`・`book missing` を、前の局面の答えから引き継ぎ、同じ計算をし直さないようにしました。

**変更**

- うみがめ数（`book moveinfo` の `part=umi`）：手の後の局面の網を数えるとき、その中の 1・2 手先の局面の値も取り出して覚えます。その局面が今の局面になったときに、網を作り直しません。
  - `umi=best` では、全部の局面を覚えます。
  - `umi=loss` では、損失 0 で着いた局面を覚えます。
  - `umi=range` では、手番が手の後の局面と同じ局面を覚えます（`lo=-hi` のときは全部）。
  - 項を落とした網（`exact=0`）からは覚えません。取り出しで項を落としたときも、その値は使いません。
  - 作る網のノードの数は、`umi=best` で約 38〜46%、`umi=loss x=2 y=4` で約 4〜53% 減りました（テスト用の book 3 つの棋譜で）。
- 局面の数（`part=count`）と `book levels`：局面ごとの答え（局面の数と level ごとの数）を、book が変わるまで覚えます。数えた手の後の局面が今の局面になって `book levels` を送ったときは、book をたどりません。
- `book missing`：覚えている精度の段階から、聞いた範囲（`x=`・`y=`）で何も見つからないとわかるときは、たどらずに 0 本と答えます。
  - GUI の既定（`base=deviate`、範囲 `1/1`〜`5/5`、`x=2 y=4`）では、範囲 2/4 がないので効きません。
  - `base=deviate2`・`deviate3` のときか、範囲に 2/4 があるときに効きます。
- マニュアル：12.4 にこの 3 つを書きました。

**互換性**

- `book moveinfo`・`book levels`・`book missing` の答えは nikque.23 と同じです。
- ただし、その局面だけから数えると `exact=0` が付くうみがめ数が、前の局面で覚えたちょうどの値（`exact=0` なし、より小さいか同じ値）になることがあります。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**未確認**：実 book での速さ、GUI での動き。

[検証記録](docs/validation/nikque.ja.md#v455-nikque24)

---

## v4.5.5-nikque.23

GUI の候補手の表の精度（`book moveinfo` の `stage=`）を、前後の局面の答えから引き継いで、たどる量を減らしました。book のコマンドを `book` なしで入力したときの案内を足し、マニュアルと文書を書き直しました。

**変更**

- `book moveinfo` の精度の段階：覚えている今の局面の段階と、手の後の局面の先の局面の段階からわかる所は、たどらずに決めます。
  - 今の局面がある範囲で完遂なら、損失 0 の手の後の局面も、その範囲で完遂です（`base=deviate` では、手の後の局面の評価値が、今の局面の評価値の符号を変えた値のとき）。
  - `base=deviate2`・`deviate3` では、Leaf までの損失の最小を局面ごとに覚えます。Leaf までの損失は、局面までの損失の合計と、Leaf の手の損失の和です。先の局面の値から求め、先の局面へも渡します。
  - 最善どうしの進行が完全読みの局面に届くかを、先の局面の答えから決めます。
  - 範囲のたどりが `deviate` で約 14%、`deviate2`・`deviate3` で約 29%、最善どうしの進行のたどりが約 99% 減りました（テスト用の book で。減り方は、局面を見る順で変わります）。
- 最上位で book のコマンドの名前（`leaf expand 30 30`・`fill2 2` など）を入力すると、警告に `(book command: type "book leaf expand 30 30" or "b leaf expand 30 30")` を添えます。
- 最上位のコマンド・設定・`base` のコマンドと同じ名前（`new`・`load`・`save`・`play`・`info`・`check` など）には添えません。
- マニュアル：
  - book のコマンドを、全部 `book …` の形で書きました（`book leaf expand 30 30` など。14.2 の表の各行も）。
  - 日英の言い回しを見直しました（事実は同じ）。
  - 英語の 12・14 章の誤り（`book leaf recalculate` の名前、`"…"` の決まりなど）を直しました。
  - 9.4 の保存の時間（約 30 秒 → 18〜28 秒）を直しました。
  - 12.4 と 13.2 に、精度の段階の引き継ぎと警告の案内を書きました。
- 文書：
  - README と修正一覧を書き直しました。
  - テストと実測の記録を `docs/validation/` へ移しました。
  - `README-NIKQUE.*` を消しました。配布版には `README-NIKQUE.*` の代わりに `README.md` が入ります。
  - `config.ini` の初めの説明の行は、マニュアルの 4 章を指します。

**互換性**

- `book moveinfo` の答え（`stage=`・`missing=`）は nikque.22 と同じです。
- 知らないコマンドの警告の行は、book のコマンドの名前のときだけ、後ろに案内が付きます。前の部分 `WARNING: Unknown command/Illegal move: "…"` は同じです。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**未確認**：実 book での速さ、GUI での動き。

[検証記録](docs/validation/nikque.ja.md#v455-nikque23)

---

## v4.5.5-nikque.22

大きな book で `book reduce`・`book reducecount` を速く、省メモリにしました。

**変更**

- `book reducecount` が約 2 倍速くなりました（29 GB・約 7 億局面の book で、8 スレッド 462 秒 → 237 秒）。
- `book reducecount` の直後に同じ条件で `book reduce` を実行すると、数えた結果を使い回して計算を省きます。GUI の「数えて確認してから削減」の流れで、同じ計算を 2 回しなくなります。
- `book reduce` で局面を大きく減らした後、使わなくなったメモリ（約 7 億局面の book で約 1 GB）をすぐに解放します。
- 新しい設定（どれも既定のままで使えます）：

| 設定 | 既定 | 内容 |
|---|---|---|
| `book-reduce-index` | `table` | `binary` で nikque.21 と同じ探し方 |
| `book-reduce-reuse` | `on` | 数えた結果を `book reduce` で使い回す |
| `book-reduce-shrink` | `on` | 削減後にメモリを解放する。`off` で nikque.21 と同じ並びで保存 |
| `book-reduce-walk` | `loss` | 局面をたどる順。`group` も選べるが、実測で 3〜9% 遅い |

**互換性**

- 残る局面とその中身は nikque.21 と同じです。
- `book-reduce-shrink = on` では、保存した book の中の局面の並び順が変わることがあります（中身は同じ）。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**未確認**：`book reduce` そのものの速さとメモリ（実 book では測っていません）。

[検証記録](docs/validation/nikque.ja.md#v455-nikque22)

---

## v4.5.5-nikque.21

`book reduce`・`book reducecount` の不具合を直し、処理を組み直しました。

**変更**

- `book reducecount` が `n-tasks` のスレッド数で並列に動くようになりました（以前は 1 スレッド）。結果は 1 スレッドのときと同じです。
- `book reduce` が局面を消す処理を、まとめて 1 回で行うようにしました。ほとんどの局面を消すときの無駄な処理がなくなります。

**不具合の修正**

- 壊れた book（同じ局面に戻る手のつながりなど）で `book reducecount` が終わらなかった（nikque.20）のを直しました。今は数えずにエラー（`@error id=N damaged=1`）を返します。
- `book reduce 0 0x`、`book reduce 0 0 junk` のような誤った入力でも、`book reduce 0 0` として局面を消していた（nikque.20）のを直しました。今は警告を出し、book を変えません。

**互換性**

- 正常な book では、残る局面と保存される book は nikque.20 と同じです。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**訂正**：nikque.20 の「残った局面の評価値は変わらない」は、`book reduce` が最初に行う評価値の計算し直しの後と比べた場合の話です。

**未確認**：速さとメモリ（測定時に CPU が混んでいたため未測定）、29 GB の実 book。

[検証記録](docs/validation/nikque.ja.md#v455-nikque21)

---

## v4.5.5-nikque.20

book を小さくするコマンド `book reduce` と、事前に数を確かめる `book reducecount` を追加しました。

**変更**

| コマンド | 働き |
|---|---|
| `book reduce X Y` | 現在の局面から `book deviate3 X Y` がたどる範囲（1 手の損失 X 以下、損失の合計 Y 以下）の局面だけを残し、ほかを消す。ふつうは初期局面で使う |
| `book reducecount id=N x=X y=Y` | `book reduce X Y` で残る数・消える数を数える（book は変えない）。GUI 向け |

- 削減した後に同じ局面から `book deviate3 X Y` を実行すると、削減前と同じ Leaf を展開します。
- 削減した book は、そのまま終了すると `book-file` に保存されます（`book subtree`・`book prune` と同じ）。
- 使い方：[マニュアル 8.4](manual/ja/08-book-maintenance.md)・[12.4](manual/ja/12-integration.md)

**互換性**：探索の結果・`eval.dat`・book のファイル形式は変わりません。

**未確認**：新しいコマンドの速さ、29 GB の実 book。

[検証記録](docs/validation/nikque.ja.md#v455-nikque20)

---

## v4.5.5-nikque.19

GUI のプログラム向けに、book を解析するコマンドを追加しました。探索の結果・`eval.dat`・book のファイル形式は nikque.18 と同じです。

**変更**

| コマンド | 答え |
|---|---|
| `book moveinfo id=N` | すべての合法手について、book の評価値・level、精度の段階（0〜7）、次の段階に足りない Leaf の数、うみがめ数、手の先の book の局面の数 |
| `book missing id=N` | 次の段階に足りない Leaf までの手順 |
| `book levels id=N` | 現在の局面から先の、level ごとの局面の数（`scope=all` で book 全体） |
| `book bestlines id=N` | 両方が最善の手（または `loss=` 石以内の手）だけを選ぶ進行の数。`file=` を指定すると、端までの手順をファイルに書き出す |
| `book findline id=N target=局面` | 現在の局面からその局面までの、book の局面だけを通る手順。評価値（黒から見た値）の変わり幅の合計が小さい順に `n=` 本（既定 1） |
| `stop id=N` | 解析 N だけを止める（ほかの解析と、待っているコマンドは残る） |

- 解析のコマンドはバックグラウンドのスレッドで動き、book を読むだけです。答えは `@` で始まる行で返します。
- 精度の段階 3〜7 は、その手の後の局面から `book deviate 1 1`〜`book deviate 5 5` を実行しても何も増えない、という意味です。0 は book にない、1 は最善の進行が完全読みの局面に届かない、2 は段階 3 の範囲が欠けている、です。
- 精度の段階の基準は、`base=deviate2`・`base=deviate3` と `stages=` で変えられます。
- うみがめ数は、その手の後の進行（既定は両方の最善の手）をすべて知るために覚える、いちばん少ない端の数です。黒・白それぞれに出します。違う手順で着く同じ局面は 1 回だけ数えます。
- うみがめ数の条件には `umi=loss`・`umi=range` も使えます。広い範囲で、いちばん少ないと言い切れない数には `exact=0` がつきます。
- 解析の間も、`hint`・手・ほかのコマンドを受け付けます。book を変えるコマンド（`book store` など）の前には、解析を止めます。
- 番号なしの `stop` は前と同じです（考えるのを止め、待っているコマンドを消します）。
- 標準出力の行（プロンプト、対局の手、book の手の行）の途中に、解析の行が入らないようにしました。
- `help book` に 5 つのコマンドを足しました。
- 解析のコマンドの語は空白で区切ります。`"…"` の中は空白も含めて 1 つの値です（`file="C:\a b\lines.txt"`）。
- `id=` が整数（int の範囲）でないときは、何もせず `@error word=id` とだけ返します。
- 使い方：[マニュアル 12.4](manual/ja/12-integration.md)

**互換性**

- 探索の結果（評価値・ノード数）・`eval.dat`・book のファイル形式・学習でできる book は nikque.18 と同じです。

**注意**

- ほかの表示（警告など）が書きかけのときは、解析の行がその行の途中に入ることがあります（GUI は行の途中の `@…` を読みます）。
- 解析で増えるメモリは、最大で book の 7% ほどです（1,914 万局面の book で約 86 MB）。

**未確認**：29 GB の実 book。

[検証記録](docs/validation/nikque.ja.md#v455-nikque19)

---

## v4.5.5-nikque.18

Windows の実行ファイルとライブラリに、ビルドしたときのフォルダーが入らないようにしました。

**変更**

- Windows の実行ファイルとライブラリは、エラーの文言のために、ビルドした PC のソースファイルの場所を含んでいました。Linux・macOS・Android と同じく、ファイル名だけにしました。
- エラーの文言は、ビルドしたフォルダーと `src\book.c` ではなく、`book.c` と表示します。
- `tests/check-build-paths.py` は、ビルドしたフォルダーが入った実行ファイルで失敗します。Release の配布版を作る workflow は、各ジョブで作った実行ファイルでこれを実行します。
- 説明の文書から、作業 PC のフォルダーと、指示や許可についての書き方をなくしました。
- 以前の Release も同じように直し、ZIP と `SHA256SUMS` を差し替えました。直したのは、Windows の実行ファイルに入っていたビルドしたフォルダー（ファイル名は残し、ほかは変えていません）と、文書の中の作業 PC のフォルダーです。
- edax_runner 5.3.0+nikque.10（別に公開）は、この版のライブラリを使います。読めなかった `book.dat` を `book.dat.damaged` として残し（`edax_book_keep_unread_file` を使います）、同じフォルダーで 2 つ目の edax_runner を起動すると止まるようにしました。

**互換性**

- 探索の結果（評価値・ノード数）・`eval.dat`・book のファイル形式・学習でできる book は nikque.17 と同じです。
- 速さと最大メモリも nikque.17 と変わりません。

[検証記録](docs/validation/nikque.ja.md#v455-nikque18)

---

## v4.5.5-nikque.17

book を毎回同じに作るための設定を 2 つ追加しました。設定しなければ、nikque.16 と同じに動きます。

**変更**

| 設定 | 値 | 既定 | 内容 |
|---|---|---|---|
| `book-seed` | 0〜2^64−1、`auto` | `auto`（時計） | book の乱数の種。数を指定すると、`book learn` が毎回同じ対局を学習する |
| `book-reproducible` | `on`／`off` | `off` | `on` で、book のコマンドの探索をすべて 1 スレッドにし、同時に広げた結果を選んだ順に book へ入れる。新しい局面の Link が探索したときから変わっていれば、それだけ探索し直す |

- `book-reproducible = on` では、`book deviate`・`deviate2`・`deviate3`・`enhance`・`fill` は、スレッドの数によらず `n-tasks 1` で作るのと同じ book になります（保存日時を除く）。
- `book-reproducible = on` で `book store`・`add`・`learn` を複数スレッドで行うと、毎回同じ book になりますが、`n-tasks 1` で作る book とは違います。
- `book learn` で毎回同じにするには、`book-seed` に数を指定してください。
- 複数の棋譜を同時に処理する `book learn`（`book-store-tasks` 2 以上）は、book の手を選ぶ乱数の種を、スレッドごとではなく棋譜ごとに付けます。種は時計から取るので、選ばれ方のでたらめさは変わりません。
- マニュアル 7.6 の「同時に広げている局面どうしは互いを見ない」は誤りだったので、「先に book に入ったものだけを見る」に直しました。
- 使い方：[マニュアル 7.6](manual/ja/07-book-learning.md)

**互換性**

- 設定しなければ、できる book・速さ・メモリは nikque.16 と同じです。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。
- `book-reproducible = on` では、32 スレッドで時間が条件により 0.84〜1.20 倍になり、最大メモリが 7〜82% 増えます。1 スレッドの探索がそれぞれ自分のハッシュ表（`hash-table-size = auto` で 57 MB）を持つためです。

**注意**

- 既定の設定では、同じコマンドでも実行するたびに book が少し変わることがあります（旧版からの動作）。
- その原因の 1 つ：book の同じ評価値の手から選ぶ乱数の種は、`book-randomness = 0` でも時計から取ります。このため `book learn` は実行するたびに別の対局を学習します。
- もう 1 つの原因：同時に広げるとき（`book-expand-tasks` 2 以上、`auto` を含む）は、ほかのスレッドが先に book へ入れた結果によって新しい局面の Link が決まります。どれが先に入るかは探索の終わる順で変わります。
- `book-reproducible` は、対局の手を探す探索（`play`・`go`）と、時間を決めた探索には効きません。
- `book-reproducible = on` の `book store` は、直前の対局の探索のハッシュ表を引き継ぎません（`n-tasks 1` の旧来の動作では引き継ぎます）。

**未確認**：29 GB の実 book、Android・Windows ARM64 の実機。

[検証記録](docs/validation/nikque.ja.md#v455-nikque17)

---

## v4.5.5-nikque.16

nikque.15 の公開後の 2 回目のソースの読み直し（クロスチェック）で見つかった不具合を直しました（多くは上流からのものです）。Android 版も追加で公開しました。

**変更**

- Android 版を追加で公開しました。Release に、完全版の ZIP と Android 用の ZIP を加えています。
- `base unique` が約 9.8 倍速くなりました（コマンド全体の実測）。`base save` と、勝敗の数が上限で止まった局面の数え直しも速くしました。
- libedax に関数 `edax_book_keep_unread_file` を追加しました。直前の `book load` で読めなかった book のファイルを `<ファイル名>.damaged` に移します（読めていたときは何もしません）。成功なら 1、移せなかったときは 0 を返します。読めなかった book を空の book で上書きしないよう、保存の前に呼びます。

**不具合の修正**

- 棋譜のファイル（txt・pgn）の最後の棋譜を読まないことがあったのを直しました（上流から）。
- `base correct`・`base complete` の途中で止まると、元の棋譜のファイルが壊れていました。別のファイルに書いてから入れ替えるようにしました。
- `base save` は、今あるファイルを読めないときは書かず、保存の失敗を返すようにしました。
- 途中で切れた edx・Wthor のファイルと読み取りのエラーを、失敗として扱うようにしました（これまでは読めた所まで使っていました）。壊れた棋譜のファイル・Wthor ファイルも読み込みません。
- 手の数が奇数の棋譜で、点数の符号が逆になっていたのを直しました。`base complete` が 2 回目の繰り返しから手番を誤ることがあったのも直しました（上流から）。
- `base unique` は、手順だけでなく、初期局面と手番も比べるようにしました。
- FEN・XBoard の `setboard`・GGF の `BO`・PGN の FEN・libedax（`edax_setboard_from_obj`）が、使えない盤面（石が 4 個未満、重なり、行・列の数の誤り）を受け付けていました。今は受け付けず、今の対局を残します。
- 複数スレッドでハッシュ表を読み書きするときの排他の誤りを直しました（上流から）。
- メモリ不足で Link を足せなかった局面を、足せたものとして扱っていたのを直しました。`book expand` 系では Leaf を残して失敗を記録します。
- 壊れた book で `book stats` が範囲の外を数えていたのを直しました（上流から）。範囲の外の局面は別に数えて表示します。
- `book depth` を 1〜61 に限りました。ヘッダーのない book での探索、`book check` などの合法手のない局面の扱いも直しました（上流から）。
- 時間の文字列（`1:30:00` など）、座標（`P@`・`@`）、`mode`（0〜3 に収める）、設定ファイルの深さの読み取りを直しました（上流から）。
- Cassio で、最後の入力行に改行がないまま入力が終わると、終了しなかったのを直しました。
- 入力のスレッドと終了処理の競合（Cassio・XBoard・コマンド行）と、XBoard・Cassio・GGS の不具合を直しました（上流から）。
- MSVC 19.44 でビルドした 32 ビット AVX2 版で、評価値の誤りとスタックオーバーフローが起きていたのを直しました。
- `MOVE_GENERATOR_NEON` を指定した aarch64 のビルドで、返す石を誤っていたのを直しました（上流から）。配布版の既定の構成では使っていません。
- 32 ビットのビルド・統計用のビルドでビルドできない所と、eval_builder などのツールを直しました（上流から）。

**互換性**

- 探索の結果（評価値・ノード数）・`eval.dat`・book のファイル形式は nikque.15 と同じです（配布版の場合。32 ビットの AVX2 構成を MSVC で自分でビルドしていた場合は、評価値の誤りを直したので結果が変わります）。
- 学習でできる book：1 スレッドでは、乱数を使う条件を除き、局面の中身は nikque.15 と同じです。32 スレッドでは nikque.15 との差が旧版どうしの差より大きい条件があり、修正による変化かどうかは確かめきれていません。

**注意**

- libedax の関数は、これまでどおり 1 つのスレッドから順に呼んでください。1 つの book のファイルに書くプロセスは 1 つにしてください。

**未確認**：実 book、Android・Windows ARM64 の実機。

[検証記録](docs/validation/nikque.ja.md#v455-nikque16)

---

## v4.5.5-nikque.15

nikque.14 の公開後にソースを読み直して見つかった不具合（上流からのものを含みます）を直しました。Linux・macOS でのメモリの解放と、大きな text の `book import` の速さも改善しました。

**変更**

- Linux・macOS でも、`book prune`・`book subtree`・`book load` の後に、局面をまとめて持つ領域（プール）の使わない部分を解放します（これまでは Windows だけ）。
- 学習で局面を足すと、プールに使われない場所が残っていました。book のコマンドや libedax の呼び出しの後、使われない場所がプールの 4 分の 1 を超えていたら、詰めて解放します。
- `book fill`・`fill2`・`leaf expand` が対象の一覧に使うメモリを減らしました。
- 大きな text の `book import` が速くなりました（400 万局面で 9.7 秒 → 7.9 秒）。

**不具合の修正**

- 正規形でない向きで書いた盤面がある text を `book import` すると、その後の `book prune`・`book subtree` が終わらなかった（上流から）のを直しました。`book import` は盤面を正規形にしてから入れます。同じ盤面の行が 2 つあれば、前の行を残します。`book prune`・`book subtree`・`book merge` も、終わらなくなる経路をなくしました。
- `book fix` は、正規形でない局面（別のツールが書いた book ファイル）を探索し直していました。今は Link と Leaf ごと正規形にして残します。同じ盤面の局面がすでにあれば、あった方を残します。
- 壊れた book ファイル（局面の手の数が多すぎる、Link の手がマスでない、など）で `book show`・`play` が異常終了した（上流から）のを直しました。`book load`・`book merge` は、こうした局面や level が 60 を超える局面があると「Truncated or damaged opening book」と表示して読み込みません（今の book はそのまま残ります）。
- 評価ファイルの印の検査が、8 バイトの片方だけで通っていた（上流から）のを直しました。両方そろったときだけ読みます。
- `book import` でメモリが足りないとき・ファイルを読めなかったときも、途中までの book に入れ替えていた（上流から）のを直しました。今は失敗にして、今の book を残します。
- ハッシュ表の大きさを変えるたびにメモリが漏れていた（上流から。XBoard の `memory`、nikque.13 からは `hash-table-size` の変更でも）のを直しました。確保できないときは、エラーで終了します。
- メモリが足りないときに、Link を減らす処理（`book prune` の後、`book fix`）で、その局面の Link が全部消えることがあった（nikque の途中の版から）のを直しました。今はメモリを確保せずに減らします。
- libedax：`libedax_terminate` の後に別の `eval-file` で `libedax_initialize` しても、前の評価データのままだったのを直しました。ファイル名が変わったら読み直します。
- 進み具合の表示に使う数を、スレッドの間で正しく読み書きするようにしました（表示だけに使う値です）。

**互換性**

- 正しい book では、`book deviate` 系・棋譜の学習・`book fill`・`fill2`・`leaf expand`・`book fix`・negamax・切り詰めでできる book は nikque.14 と同じです。
- 大きな text の `book import` は、局面が増えるにつれて区画の数を増やします。区画の順が変わるため、取り込んだのと同じ実行の中で続けて学習すると、できる book が変わることがあります。
- 正規形でない盤面を持つ book、壊れた book ファイル、印の片方だけが正しい評価ファイル、`book import` の失敗では、動作が変わります。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**注意**

- 範囲を付けない `book fill`・`fill2`・`leaf expand` の negamax は、これまでどおり初期局面から行います。

**未確認**：実 book での確認、macOS でのメモリの解放、スレッド間の競合がすべて解消したかどうか。

[検証記録](docs/validation/nikque.ja.md#v455-nikque15)

---

## v4.5.5-nikque.14

book の欠けた所を埋める `book fill2` と、Link より良い Leaf を広げる `book leaf expand` を追加し、`book fill` を複数スレッドで速くしました。

**変更**

| コマンド | 働き |
|---|---|
| `book fill2 n` | `book fill n` と同じく、book の局面から n 手先が book の局面になるとき、その間の局面を足す。1 手目は Leaf の手だけをたどる。n を省くと 2 |
| `book fill n X Y`・`book fill2 n X Y` | 現在の局面から `book deviate3 X Y` がたどる範囲だけを対象にする（X は 1 手の損失の上限、Y は合計の上限）。完全読みの局面の Leaf も対象 |
| `book leaf expand` | Leaf の評価値がその局面のどの Link よりも大きい局面の Leaf を、`book deviate` と同じように広げる。そういう局面がなくなるまで繰り返す。Link のない局面は対象にしない |
| `book leaf expand X Y` | `book deviate3 X Y` がたどる範囲だけで `book leaf expand` を行う |

- `book leaf expand` は、Egaroucid で学習するときに、Leaf の評価値が Link の最大より大きい局面があると学習がおかしくなるため、book の側で直せるように追加しました。
- `book fill`・`fill2` は、どの周も初めに、手の先の局面が book にあるのに Link になっていない手（欠けた Link）を、すべての手について張ります。
- `book leaf expand` は、Leaf の手の先が book にある局面の欠けた Link を全部張ります。それ以外の局面の欠けた Link は `book fix` で張ってください。
- 終わった後は、対象の範囲の中に欠けた Link は残りません。
- 局面を足した元の局面には、その場で Link を張ります。その Leaf が足した手だったときは、Leaf を探し直します。
- `book fill`・`fill2`・`leaf expand` は複数のスレッドで動き、同時に行う探索の数は `book-expand-tasks` に従います。`book fill 2` は 8 万 3 千局面の book（32 スレッド）で 94.8 秒 → 6.74 秒になりました。
- 途中経過の保存は `book-deviate-save-rounds` に従います（これまでの `book fill` は毎周保存）。
- `book fill` で足した局面は、次の周に埋める元になります（これまでは同じ周のうち）。
- `book leaf-recalculate`・`leaf-recalculate2`・`3`・`4` の名前を、`book leaf recalculate`・`leaf recalculate2`・`3`・`4` にしました（`book leaf recalculate4 5 5` のように、数字は続けて書きます）。表示も `Book leaf recalculate4 5 5...` になります。設定名 `book-leaf-recalculate-rounds` は変わりません。
- libedax：`edax_book_fill2`・`edax_book_fill_limited`・`edax_book_fill2_limited`・`edax_book_leaf_expand`・`edax_book_leaf_expand_limited` を追加しました。`edax_book_fill` は新しい `book fill` になります。

**互換性**

- `book fill n`（n が 2 以上）でできる book が変わります。同時に探索するため、1 つずつ探索したときとも少し違うことがあります（`book store` と同じ）。
- `book fill 1`（`book fill`）でできる book は nikque.13 と同じです。
- `book deviate` 系・棋譜の学習・`book fix`・negamax・切り詰めでできる book は nikque.13 と同じです。
- ハイフンの付いた `book leaf-recalculate` などは使えなくなりました（「Unknown book command」と表示されます）。
- 探索の結果・`eval.dat`・book のファイル形式は変わりません。

**注意**

- `book fill n` の n は「ちょうど n 手先」です。欠けている局面が 1 つだけの所は n = 2 で埋まります。`book fill2 1` と、範囲を付けた距離 1（`book fill 1 5 5` など）は何もしません（「nothing to fill」と表示します）。
- 範囲を付けない `book fill`・`fill2`・`leaf expand` は、negamax を初期局面から行います。初期局面のない book（`book subtree` で切り出したものなど）では、範囲を付けて切り出した局面から行ってください。
- `book fill`・`fill2` の後は、Link より良い Leaf を持つ局面が増えることがあります。両方を直したいときは、`fill2` → `leaf expand` の順に実行してください。
- 6 億 7 千万局面の実 book（level 18、32 スレッド）では、最初の周だけで、`book leaf expand 5 5` が約 55 分、`book fill2 2 5 5` が約 4 時間、`book leaf expand`（book 全体）が約 18 時間、`book fill2 2`（book 全体）が約 4 日かかる見込みです。2 周目からは対象が減りますが、合わせた時間は最初の周より長くなります。
- Edax 以外のプログラムが書いた、または壊れた book ファイルでは、`book fix` が局面を正しく引けなくなる、メモリを壊す、といったおそれがあります。写しで扱ってください（nikque.15 で直しました）。

**未確認**：最終のソースでのテスト一式と速さの計測、実 book での最後までの実行（実 book の所要時間は数分動かした速さからの見込みです）。

[検証記録](docs/validation/nikque.ja.md#v455-nikque14)

---

## v4.5.5-nikque.13

nikque.10〜12 の変更を点検し、book の評価値を壊す不具合や、book のファイルが黙って上書きされる不具合などを直しました。ユーザーマニュアルを同梱しました。

**変更**

- ユーザーマニュアル（日本語・英語、各 16 ページ）を `manual` フォルダーに入れました（[日本語](manual/ja/README.md)・[英語](manual/en/README.md)。Release の ZIP にも入っています）。
- 新しい設定 `book-leaf-recalculate-rounds = n`（既定 1）：`book leaf-recalculate` 系のコマンドが「たどる → 計算し直す → negamax」を最大 n 回繰り返し、Leaf が 1 つも変わらなかった回で終わります。Leaf を計算し直すと、たどる範囲も動くためです。既定の 1 は nikque.12 と同じ動作です。
- `book save` をファイル名なしで実行すると、`book-file` に保存します（以前はエラーで保存されませんでした）。
- 実行中に `hash-table-size`（`auto` のときは `n-tasks` も）を変えると、探索の表を作り直します（以前は表示だけが変わっていました）。

**不具合の修正**

- `book correct`・`book deepen` が、合法手がなく Link もない局面（終局の局面、パスの先が book にない局面）の Leaf を消し、book の評価値を壊していました（上流から。nikque.11 の同時探索も同じ）→ 探索し直すようにしました。以前の版で壊れた book は、この版の `book correct` で直ります。
- `book leaf-recalculate` 系がパスの局面の Leaf を計算し直していませんでした（nikque.12）→ 対象にしました。`book leaf-recalculate4` を book のいちばん深い層の局面で実行すると何もしなかった（nikque.12）のも、`3` と同じく計算し直すようにしました。
- `book new` の後、`book load` で別のファイルを読んで book を変えた後、`book import` の後に、ファイル名を書かずに保存（終了時・名前なしの `book save`）すると、`book-file` のファイルが上書きされていました（上流から）→ それまでのファイルを `<book-file>.old` として残します。`book-file` に拡張子を足したファイル（`.dev2`・`.store` など）を読んだときは、これまでどおり置き換えます。
- `book subtree`・`book prune` で切り詰めた book が、`book save` をしないと終了時に保存されませんでした（上流から）→ `book-file` に保存します。
- `book subtree` を book の深さより 2 つ以上深い局面で実行すると、book が空になっていました（上流から）→ 警告を表示して何もしません。
- book のコマンド・`base` のコマンドの実行中に `stop`（`quit`）を入力すると、打ち切った探索の結果が book に入っていました（上流から）→ 実行中は探索を止めません。`quit` はコマンドが終わってから終了します。libedax の `edax_base_complete`・`edax_base_correct` 実行中の `edax_stop` も同じように直しました。
- `base complete`（`edax_base_complete`）が、直前の探索の深さのまま動いていました（上流から）→ `level` の設定で探索します。
- 80 手（着手＋パス）を超える行を、複数の棋譜を同時に学習するとき（`book-store-tasks` が 2 以上か `auto`）に学習していませんでした（nikque.10 の直し残し）→ 学習します。edax_runner 5.3.0+nikque.7 では、1 局ずつ学習する場合も直しました。
- パスの局面を探索すると、まれに異常終了していました（上流から。`-solve` の 1 問目がパスの局面のときなど）→ 直しました。
- 棋譜の読み込み：パスのある棋譜を保存した SGF が読み込めなかった、石を 1 つも返さない手のある棋譜を `load` があり得ない盤のまま読み込んでいた（その盤で `book store` するとあり得ない局面が book に入った）のを直しました（どちらも上流から）。保存した `.edx` に値を入れていないメモリが書かれていたのも、0 で埋めるようにしました。
- `hash-table-size` を小さい数（`level` が 18 以下なら 19 以下）にしていると、`book-expand-tasks = auto` の対象の多い周でメモリが増えていました（nikque.10）→ nikque.9 と同じ量に戻しました。同梱の設定（`level = 18`、`hash-table-size = auto`）では変わりません。
- 表示の誤りを直しました（上流から）：`options` の `auto-store`・`auto-swap`・`auto-quit`、`analyze` の `-nan(ind)`、XBoard の終了時の合計、`help options` の既定値。`load` の失敗や `.txt` の棋譜の打てない手に警告を出し、`book info` は level の違う局面を 10 個までと個数で表示します。

**互換性**

- 探索の結果、`eval.dat`、book のファイル形式は変わりません。速さとメモリは、nikque.12 より遅くなった・増えたという結果は出ていません。
- `book deviate` 系・棋譜の学習・`book fix`・negamax・切り詰めでできる book は nikque.12 と同じです。例外は、`hash-table-size` を小さい数にしているときの `book-expand-tasks = auto` の対象の多い周で、結果が少し変わることがあります。
- `book export` のファイルの最後に `% depth n` の行が付き、`book import` はその深さにします。nikque.12 はこの行を警告を出して読み飛ばします。`% depth` の行がないファイルを取り込むと、深さはこれまでより 1 つ小さくなります。

**注意**

- 切り詰める前の book を残したいときは、`book subtree`・`book prune` の前にファイルの写しを取ってください。`.old` のファイルは Edax が消さないので、要らなくなったら消してください。
- 途中の局面から始まる対局は `.txt`・`.sgf` では読み戻せません。`.ggf`・`.pgn`・`.edx` で保存してください。
- Windows では、`book-file` のフルパスを 240 文字以内にしてください。長いと `.store` などの保存が行われないことがあります。
- 直していないもの：作ったばかりの Leaf でも計算し直すと値が変わることがある（探索する盤面の向きによる）、`book deepen` は局面の level を上げない、初期局面からたどれない局面は negamax されない。

**訂正**

- 80 手を超える対局は「`setboard` で作った局面から」（nikque.10）：通常の初期局面からでも超えます（パスが 21 回以上ある対局）。
- Leaf の再計算で「同じコマンドをもう一度行うと変わらない」（nikque.12）：book の全部を範囲にした場合の話です。
- `book subtree` の後処理は `book fix` と同じ（nikque.7）：nikque.12 から Link の張り直しを行いません。
- 「`book link`」というコマンド（nikque.10・12）：そういうコマンドはありません。Link の張り直しは `book fix` などの中で行われる処理です。

**未確認**：Linux・macOS・32 ビット版・ARM64 の速さ、level 19 以上の book での同時探索、実 book への `book correct`。

[検証記録](docs/validation/nikque.ja.md#v455-nikque13)

---

## v4.5.5-nikque.12

Leaf の評価値を計算し直すコマンド `book leaf-recalculate` を追加し、`book subtree`・`book prune` を速くしました。

**変更**

| コマンド | たどり方 | 計算し直す Leaf |
|---|---|---|
| `book leaf-recalculate X Y` | `book deviate X Y` と同じ | `book deviate X Y` が展開すると選ぶ局面の Leaf |
| `book leaf-recalculate2 X Y` | `book deviate2 X Y` と同じ | `book deviate2 X Y` が展開すると選ぶ局面の Leaf |
| `book leaf-recalculate3 X Y` | `book deviate X Y` と同じ | たどった局面の全部の Leaf（いちばん深い層も） |
| `book leaf-recalculate4 X Y` | `book deviate2 X Y` と同じ | たどった局面の全部の Leaf（いちばん深い層も） |

- `eval.dat` を替えた後で、book の古い評価値を新しい評価関数の値に置き換えるためのコマンドです。局面は足しません。1 回たどって終わります。
- 完全読みの局面（level 18 では空き 21 以下）の Leaf は計算し直しません。
- 1 スレッドの探索で行われる場合（`book-expand-tasks = auto`、level 18 以下、対象が `n-tasks` の 32 倍以上）は、スレッドの数によらず同じ結果になります。
- `book-save-interval` 分ごとと終わりに `book.dat.leaf`〜`.leaf4` に保存します。途中の保存を使うときは `book negamax` を実行してください。続きから再開する仕組みはありません。
- `eval.dat` を替えなくても book は変わります。複数スレッドの探索で作った Leaf や、`book subtree` の切り出しで入った値が変わります。
- libedax：`edax_book_leaf_recalculate`・`2`・`3`・`4` を追加しました。
- `book subtree`・`book prune` の、残す局面に印を付ける処理を `n-tasks` スレッドで行います。6 億 6162 万局面の book で深さを 40 から 39 に減らす切り出しが 603.6 秒 → 176.6 秒になりました。
- `book subtree` は、切り出しの後に Link を張り直さなくなりました。
- `book`・`base` の後ろの語を大文字で書いても通ります（`book Deviate 5 5`）。ファイル名などの引数は書いたとおりに使います。

**互換性**

- 切り出す前から Link が欠けていた book では、`book subtree` の後も欠けたままになります（nikque.11 までは張られていました）。必要なら `book fix` を実行してください。Link の欠けていない book では、できる book は同じです。`book prune` は変わりません。
- 探索の結果、`eval.dat`、book のファイル形式は変わりません。

**未確認**：level 19 以上、6 億局面の book での全部の実行（時間）、実 book での `book prune`。

[検証記録](docs/validation/nikque.ja.md#v455-nikque12)

---

## v4.5.5-nikque.11

book を切り詰めるコマンド `book subtree`・`book prune` と、`book correct`・`book enhance` を速くしました。新しい機能・設定はありません。

**変更**

- `book subtree`・`book prune` で、同じ局面を何度もたどり直すのをやめました（上流から）。649万局面の book・32 スレッドで、深さを 1 減らす切り出し（`book depth 19` の後に初期局面で `book subtree`）が 120.4 秒 → 5.5 秒になりました。
- 切り詰めの前の negamax と、消した局面への Link を外す処理を、複数スレッドで行います（`-n 1` では従来と同じ処理）。
- 切り詰めた後、消した局面のメモリを返します（Windows 版だけ）。6億6162万局面の実 book で深さを 1 減らした後、31.8 GB → 24.3 GB になりました。
- `book correct` は、`book-expand-tasks` の数だけ完全読みの局面を同時に探索します（`book deviate` の展開と同じ決め方）。120 局面の小さい book・32 スレッドで 2.0 秒 → 0.47 秒です。同時に行う探索のぶん、メモリが増えます。`book-expand-tasks = 1`・`-n 1`・`-cpu` のとき、メモリが足りないときは、これまでどおり 1 つずつです。
- `book enhance` の始めと各周の negamax を、複数スレッドで行います。
- 表示：`Book subtree N... done`・`Book prune N... done` の N は、たどり直しを数えなくなったので小さくなります。途中経過の `Book prune N to keep` は 10 万局面ごとに 1 回出ます。

**不具合の修正**

- 初期局面以外から、`book depth` で深さも減らして `book subtree` を行うと、negamax が何もせず、勝ち・引き分け・負け・手順の数と評価値の範囲が切り出す前の book のままでした（上流から）。初期局面が book にないときは、切り出した局面から negamax を行います。
- `book check` で、棋譜の手が book に 1 つもないとき、悪手の割合が 0÷0 になっていました。0% と表示します。

**互換性**

- 探索の結果・`eval.dat`・book のファイル形式は変わりません。
- `book subtree`・`book prune` で保存される book は、nikque.10 と同じです。ただし、初期局面以外から深さも減らして `book subtree` を行うと、勝ち・引き分け・負け・手順の数と評価値の範囲が変わります。手と評価値は同じで、これらの値を使うのは `book show` の表示だけです。
- `book correct` で同じ評価値の手が複数あるとき、どれを Leaf にするかは実行ごとに変わることがあります（nikque.10 でも同じ）。

**注意**

- `book correct` は、合法手がなく Link もない局面（終局の局面、パスの先が book にないパスの局面）の Leaf を消して、評価値を壊します（上流から。nikque.13 で修正）。
- 初期局面のない book では、`book negamax`・`book fix`・`book prune` などの negamax は、これまでどおり何もしません（`book deviate`・`book enhance` は今の盤面から行います）。

**未確認**：配布版（PGO）での速さ、実 book での `book prune`・`book correct`・`book enhance`、評価値の誤りが見つかる場合の `book correct`、Linux・macOS・32 ビット版・ARM64 の速さ、戻る Link のある壊れた book での `book subtree`・`book prune`。

[検証記録](docs/validation/nikque.ja.md#v455-nikque11)

---

## v4.5.5-nikque.10

nikque.9 の後に見つかった不具合を 3 件直し、大きな book の `book negamax`・`book fix`・`book merge` と、`book deviate`・`deviate2`・`deviate3` の展開を速くしました。新しい機能・設定はありません。

**変更**

- 複数スレッドの `book negamax` と、`book fix`・`book merge` の Link の張り直しを速くしました。6億6162万局面の実 book・32 スレッドで、`book fix` の張り直しが 153.7 秒 → 136.3 秒になりました。
- `book-expand-tasks = auto`（同梱の `config.ini` の値）で、level 18 以下、その周の展開の対象が `n-tasks` の 32 倍以上のときは、1 スレッドの探索を `n-tasks` 個同時に動かします（これまでは 2 スレッドの探索を `n-tasks / 2` 個）。649万局面の book・32 スレッドで、60 秒に展開できる件数が約 1.25 倍、最大メモリが 1,432 MB → 1,001 MB になりました。
- それより対象の少ない周、level 19 以上、`book-expand-tasks` を数で指定した場合は、nikque.9 と同じです。
- 以前の決め方に戻すには、`book-expand-tasks` に `n-tasks` の半分の数（32 スレッドなら `16`）を書きます。

**不具合の修正**

- 着手とパスを合わせて 80 手を超える対局で、対局の記録の外に書き込み、手数と持ち時間が壊れていました（上流から。パスの多い対局で起きます）。記録を 128 手ぶんにしました。`force` の手順、棋譜の保存、libedax（`edax_play`・`edax_move` など）も直しました。
- 文字列の設定（`book-file`・`eval-file`・`game-file`・`name`・ログのファイル名など）を設定し直すたびに、前の文字列のメモリが解放されませんでした（上流から）。
- libedax：`edax_bench` の途中で `edax_stop` を呼んでも、残りの問題が続き、結果の時間が異常な値になっていました（元の libedax から）。`edax_stop` でベンチマークを終えます。打ち切られた問題は、局面数・ノード数・時間に数えません。

**互換性**

- `eval.dat`、book のファイル形式、1 スレッドの探索の結果とノード数は変わりません。
- `book negamax`・`book fix`・`book merge` の結果は nikque.9 と同じです。
- `book-expand-tasks = auto` で対象の多い周を展開すると、できる book が nikque.9 と少し違います（1 周で全局面の 0.065% の、Leaf の手と評価値。評価値の差はほとんどが 1）。

**注意**

- `edax_get_moves` は、これまでどおり 80 手までを返します。
- 複数の棋譜を同時に学習する処理は、80 手ぶんの記録のままです（nikque.13 で修正）。
- 同時に展開している局面どうしは、お互いを参照しません（同時に動く数が 16 から 32 に増えます）。

**未確認**：`auto` の新しい決め方で、毎回同じ book になること（続けて動かした回は同じでした）。6億6162万局面の book を配布版で、32 ビット版・Linux・macOS・ARM64 の速さ。

[検証記録](docs/validation/nikque.ja.md#v455-nikque10)

---

## v4.5.5-nikque.9

nikque.8 と edax_runner v5.3.0-nikque.2 の総点検で見つかった不具合を直し、棋譜の学習（`book learn`）を少し速くしました。

**変更**

- `book learn`（libedax の `edax_book_store_games`）で、早く終わった対局のスレッドを、まだ対局している探索に次の手から足します（1 局あたり 7 スレッド以下のときだけ）。level 18 の棋譜 128 本・32 スレッドで 33.6 秒 → 30.3 秒です。
- 新しい設定 `book-store-auto-save`（`on`／`off`、既定は `on`＝これまでと同じ）：`off` にすると、`book store`・`book learn` の後の `<book ファイル>.store` への保存を行いません。学習のたびに自分で book を保存するプログラム（edax_runner）のためのものです。
- 数値でない値・on/off でない値の設定（`level = abc` など）は、警告を出して無視します。
- 実行中に `n-tasks` を下げても、`book-store-tasks`・`book-expand-tasks` の数は保たれます。
- PATH から名前だけで起動して、作業フォルダに `config.ini` がないときは警告します。
- Linux の `-cpu`：book の処理のスレッドも CPU ごとに固定します。`-cpu` を付けたときは、同時に行う探索を使いません。
- Windows：強制終了で残った `book.dat.tmp.<番号>` を、次の保存のときに消します。
- libedax：関数 `edax_book_save_checked`（保存できたら 1、できなかったら 0 を返す）と `edax_book_failed`（直前の book の処理が、メモリ不足で局面を追加できなかったら 1）を追加しました。`edax_book_store_games` の状態の文字に `'2'`（失敗。その行はまだ学習されていない）を足しました。
- Linux のライブラリが約 8% 速くなりました。Linux と Android のライブラリは、使うプログラムに同じ名前の関数があっても、ライブラリの中では自分のものを使います。

**不具合の修正**

- 複数スレッドの探索で、手が探索されないまま置換表に書かれるノードができ、探索を止める要求が失われることもありました（上流から）。nikque.8 の「止めて、スレッドを足して続ける」処理では、book の Leaf が最善でない手・評価値になり得ました。
- `book merge` を同じ実行の中で 2 回以上行うと「duplicated position」で失敗し、前の merge で入った局面まで消えることがありました（nikque.3 から）。
- 壊れた book（自分や上の局面に戻る Link）で、複数スレッドの `book negamax` が異常終了していました（nikque.3 から）。
- `book import` に失敗すると今の book が消え、終了時に book ファイルへ上書きされていました（上流から）。今の book を保ちます。
- 起動時に読めなかった book ファイルが、学習の後で上書きされていました。保存の前に `<名前>.damaged` として残します。
- `base complete`・`base correct` が、読み込めないファイルを削除していました（上流から）。
- `book new` の範囲外の level、長すぎる book ファイル名、level が 60 を超える局面を持つ壊れた book で、異常終了・戻らないことがありました。
- 学習：255 バイトを超える行が切られる、持ち時間つきの学習で残り時間が減らない、の 2 件を直しました（nikque.7 から）。行の先頭の randomness の上限（127）もなくしました。
- メモリが足りない・スレッドが作れないときに終了する・止まったままになることがありました。探索を減らして続けます。
- `edax < file` で、`quit` の前のコマンドが実行されないことがありました。
- `book store` で Link が増えただけのときに保存されない、同時に行う探索で探索ログが途中で閉じる、を直しました。
- libedax：book を変える処理の実行中は `edax_stop` が探索を止めません（途中の結果が book に入っていました）。思考中（ponder）の book の処理で止まる、NULL の引数で異常終了する、なども直しました。

**互換性**

- `eval.dat`、book のファイル形式、1 スレッドの探索の結果とノード数は変わりません。
- `book learn`（`book-store-tasks` が 1 以外）は、組の終わりのほうの対局の手が実行ごとに変わることがあります。以前の版と同じ book を作るには `book-store-tasks = 1` を指定します。

**注意**

- 32 ビット版では、同時に行う探索を使う処理の最大メモリが約 110〜120 MB 増えます。8947万局面以上の book は読めません。

**未確認**：macOS 版・Android 版・Windows ARM64 版の動作と速さ、level 30 を超える level。

[検証記録](docs/validation/nikque.ja.md#v455-nikque9)

---

## v4.5.5-nikque.8

nikque.7 で既定にした `book-store-tasks = auto` が、探索の数が少ないときや level が高いときに nikque.6 より遅くなる問題を直しました。変更はこれだけです。

**変更**

- 探索の数がスレッドの数より少ないときは、最初から `n-tasks ÷ 探索の数` スレッドずつ使います。
- 始める探索がなくなったら、まだ動いている探索に、終わった探索のスレッドを足して続けます。それまでの探索は無駄になりません。
- 探索が 1 つだけのときは、`book-store-tasks = 1` とまったく同じ探索をします。追加のメモリも使いません。
- 探索用の領域は、始める探索が残っている間だけ作ります。短い探索が少しあるだけのときに、スレッドの数だけ作ることがなくなりました。
- 測ったすべての条件で、`auto` は `1` と同じか、それより速くなりました（level 24 で 1 局を対局して `book store`：`1` で 13.4 秒、nikque.7 の `auto` で 26.0 秒、nikque.8 の `auto` で 5.8 秒・10.6 秒）。

**不具合の修正**

- nikque.7 の `auto` では、棋譜の学習（`book store`・`book add`・`book learn`）と Link の張り直し（`book fix`・`book merge`・`book import` など）で、いちばん長い探索が 1 スレッドのまま終わるまで、ほかのスレッドが何もせずに待っていました。探索の数がスレッドの数より少ないときや、短い探索に長い探索が混じるときに起きます。level が高いほど影響が大きくなります。→ 同時に行う探索へのスレッドの配り方を直しました。

**互換性**

- `eval.dat`、book のファイル形式、探索結果、`book-store-tasks = 1` のときの動作は変わりません。
- book の内容が壊れる種類の問題ではありません。nikque.7 で作った book は、そのまま使えます。
- 1 スレッドのまま終わった探索の結果は、nikque.7 と同じです。
- 途中でスレッドを足した探索と、最初から複数スレッドで行った探索は、実行のたびに結果がわずかに変わることがあります（複数スレッドの探索と同じです）。nikque.7 の `auto` は、何度実行しても同じ book でした。

**注意**

- `auto` は `1` より多くメモリを使います。32 スレッドでは、同時に行う探索のハッシュ表の合計が、level 18 以下で約 450 MB、level 21 以下で約 0.9 GB、それより上で約 1.8 GB までです（スレッドの数に比例します）。
- メモリを増やしたくない場合は、`book-store-tasks = 1` を指定してください。

[検証記録](docs/validation/nikque.ja.md#v455-nikque8)

---

## v4.5.5-nikque.7

Edax をほかのプログラムから使えるライブラリ libedax を追加し、棋譜の学習を複数のスレッドで行うようにしました。`book fix` も速くなりました。

**変更**

- libedax を追加しました。lavox 氏・sensuikan1973 氏の libedax と同じ 93 個の関数で、やり取りするデータの並びも同じです。libedax 用のプログラム（libedax4dart、edax_runner など）が、ライブラリのファイルを差し替えるだけで動きます。

| ファイル | 対象 |
|---|---|
| `libedax-x64.dll`（Linux：`libedax-x86-64.so`） | x86-64 のどの CPU でも |
| `libedax-x64-v3.dll`（`libedax-x86-64-v3.so`） | AVX2 対応の CPU |
| `libedax-x64-v4.dll`（`libedax-x86-64-v4.so`） | AVX-512 対応の CPU |
| `libedax.universal.dylib` | macOS（Apple silicon と Intel の両用） |
| `libedax-arm64-v8a.so`・`libedax-armeabi-v7a.so` | Android（ARM64・32 ビット ARMv7） |

- libedax の設定は、作業フォルダの `edax.ini`、`config.ini`、`libedax_initialize` の引数の順に読みます（後のものが優先）。
- libedax に追加した関数：`edax_book_deviate2`、`edax_book_deviate3`、`libedax_cpu_level`、`edax_book_store_games`（複数の棋譜をまとめて対局・学習）、`edax_book_store_tasks`。
- libedax の 1 スレッドの終盤探索は、元の libedax（Edax 4.4）の 1.3〜1.7 倍の速さで、最大メモリは約半分です。
- 新しい設定 `book-store-tasks` で、棋譜の学習（`book store`・`book add`・`book learn`、libedax の `edax_book_store_games`）を複数のスレッドで行います。

| `book-store-tasks` | 動作 |
|---|---|
| `auto`（既定。同梱の `config.ini` も、`config.ini` がない場合も） | `n-tasks` 本の棋譜を同時に学習する（1 本につき 1 スレッド） |
| 数値 n（2 以上） | n 本の棋譜を同時に学習する（1 本につき `n-tasks / n` スレッド） |
| `1` | nikque.6 と同じ動作で、同じ book になる |

- 棋譜 128 本の学習（論理 CPU 32）が、`1` の 112.9 秒から `auto` で 39.4〜41.6 秒になりました。edax_runner を何本も同時に起動して後で merge するやり方とほぼ同じ速さを、1 本・1 つの book で出せます。
- 新しいコマンド `book learn <ファイル>`：ファイルの各行の手順を打ち、終局まで Edax どうしで対局して `book store` します（edax_runner の「Edax 対 Edax」の行と同じ書き方）。`f5d6c3`、または `2,f5d6c3` のように `book-randomness` の値と手順を書きます。空行、`#` で始まる行、`//` を含む行は読み飛ばします。
- `book fix`（`book import`・`correct`・`prune`・`subtree`、`book store` の後処理でも使います）の局面の確認・Link の張り直し・並べ替えを、複数スレッドで行います。649 万局面の book で、32 スレッドでは 22.9 秒 → 2.3 秒です。
- Link の張り直しで必要になる Leaf の探索（`book fix`・`book merge`・`book import` など）も、`book-store-tasks` が 1 以外なら同時に行います。
- macOS 用に、Apple silicon 用の `mEdax-arm64` と `libedax.universal.dylib` を追加しました。

**不具合の修正**

- 学習コマンドが途中経過を別のファイル（`.store`、`.dev` など）に保存すると「保存済み」の扱いになり、`book save` をせずに終了すると `data/book.dat` に保存されませんでした（nikque.2 から）。→ 途中経過の保存では「保存済み」にしないようにしました。
- `book fix` の「Fixing book...」が、直す局面がないと終わるまで何も表示しませんでした。→ 確認した局面数を 1 秒ごとに表示します（例：`Fixing book...3137536/6491163 positions checked`）。

**互換性**

- 探索の結果・`eval.dat`・book のファイル形式は変わりません。
- **棋譜の学習でできる book は、nikque.6 とわずかに違います。** 違いの大きさは、従来の学習でスレッド数を変えたときと同じ程度です。`auto` は何度実行しても同じ book になります。nikque.6 と同じ book が必要なら、`book-store-tasks = 1` を指定してください。
- `book fix` の確認と Link の張り直しは、1 つずつ処理した場合と同じ book になります。Leaf の探索し直しを同時に行った場合は、Leaf がわずかに違うことがあります。
- `book deviate`・`deviate2`・`deviate3`・`enhance`・`play`・`book fill` は変わりません。
- edax 本体には影響しません。libedax のコードは、ライブラリをビルドするときだけ取り込まれます。

**注意**

- `auto` は多くメモリを使います（棋譜 128 本の学習で、最大メモリが 290 MB → 722 MB）。同時に行う探索は、それぞれハッシュ表を持ちます（level 18 以下で 1 個 14 MB、level 21 以下で 28 MB、それより上で 57 MB）。
- 1 局だけの `book store` や、`book-store-tasks` が 2・4 のような小さい値では、効果は小さくなります（nikque.8 で、探索の数が少ないときの遅さを直しました）。
- `book learn` では、同時に対局する組の中の棋譜の学習結果は、その組の対局には反映されません。
- libedax の既定の level は 18 です（元の libedax は 21）。`edax_book_merge` は、Link の張り直し・修正・negamax まで行います（元の libedax は局面を足すだけ）。
- libedax は 1 つのプロセスにつき 1 つです。関数は 1 つのスレッドから順に呼んでください。1 つの book のファイルに書くプロセスは 1 つにしてください（後に保存した方だけが残ります）。

**未確認**：Android 用 libedax の実機での動作（ビルドの確認のみ）。

[検証記録](docs/validation/nikque.ja.md#v455-nikque7)

---

## v4.5.5-nikque.6

起動時の設定を `config.ini` で決められるようにし、`config.ini` を手で書きやすくしました。既定の level を 21 から 18 に変えました。

**変更**

- `config.ini`（またはコマンドライン）で、起動時の次の値を決められます。同梱の `config.ini` の値と、`config.ini` がない場合の既定値は同じです。

| 設定 | 既定 | 内容 |
|---|---|---|
| `level` | `18` | 起動時の探索レベル（0〜60） |
| `n-tasks`（`n`） | `auto` | 探索のスレッド数。1〜論理 CPU の数、`auto` は論理 CPU の数 |
| `book-depth`（新しい設定） | `auto` | 起動時の book の深さ。`auto` は book ファイルの深さのまま。1〜60 ならその深さにする |
| `book-usage` | `on` | book から打つかどうか |

- コマンドラインでは `-n auto`、`-book-depth <n|auto>` と書けます。
- 設定の値は、プロンプトの `options` で確かめられます（「book depth at startup」の行を追加しました）。
- `config.ini` と `edax.ini` の書き方を緩くしました。
  - `level = 18`、`level=18`、`level 18`、`set level 18` はどれも同じです。タブ、全角の空白と「＝」も使えます。
  - 名前と、`on`/`off`/`auto` などの値は、大文字・小文字を区別しません。名前の中の空白・`_`・`-` は同じに扱います（`book depth` と `book-depth` は同じ）。
  - `=` を書いた場合は、行の残りが値になります（空白を含むファイル名も書けます）。
  - `#` から行末まではコメントです。UTF-8 の BOM と CRLF の改行も使えます。
  - 知らない名前は、起動時にファイル名と行番号つきで警告します（例：`WARNING: config.ini:7: unknown or incomplete setting "levle" ignored`）。
- 1 局面の合法手の数の上限を 33 から 34 にしました。初期局面から到達できない局面には 34 通りのものがあり、`-solve` や `setboard` で入力されても扱えるようにしました。

**不具合の修正**

- `book merge` と `book fix` の「Linking book...」「Fixing book...」が、途中経過を表示していませんでした（nikque.3 から）。→ 1 秒ごとに進み具合（局面数）を表示します。`book merge` でファイルを読む間も、`Checking book ...`・`Merging book ...` に読んだ局面数を表示します。

**互換性**

- **組み込みの既定の level を 21 から 18 に変えました。** `-l` を指定しない `-solve` や対局の探索は、以前と結果が変わります。`-l` を指定した場合は同じです。
- 同じオプションでの探索結果、`eval.dat`、book のファイル形式は変わりません。
- 合法手の上限の変更で、book のメモリと速さは変わりません。
- 途中経過の表示の修正で、book の内容は変わりません。

**注意**

- `config.ini` と `edax.ini` の `level` は、起動時の level だけを決めます。持ち時間制の対局（`-t`、`-move-time`）で読みの上限にはなりません。上限にしたいときは、コマンドラインの `-l` か、プロンプトの `level` で指定してください（`-o` で読ませる設定ファイルの `level` も上限になります）。

[検証記録](docs/validation/nikque.ja.md#v455-nikque6)

---

## v4.5.5-nikque.5

持ち時間制の対局で持ち時間を使い切るようにして強くし、book のメモリを約 14% 減らしました。

**変更**

- 持ち時間制の対局（`-t`、`-move-time`）で、level を指定しない場合は読みの上限を 60 にしました。以前は既定の level 21 で読みをやめ、1 局 16 秒では持ち時間の約半分、1 局 64 秒では 2 割未満しか使っていませんでした。
- `-l` や `level` で level を指定した場合は、従来どおりその level が上限です。xboard の `sd`、NBoard の `depth` も同じです。
- 持ち時間の配分を、実際に測った探索速度（の 2 倍）で見積もるようにしました。`-speed n` を指定した場合は従来どおりその値を使います（`-speed auto` で自動に戻ります）。
- この 2 つで、1 局 16 秒・1 スレッドの対局が +90 Elo 強くなりました。
- `-nps` を指定したときは、1 局の残り時間もノード数で減らします（対局を再現できる比較のため）。
- メモリ上の book の 1 局面を 56 バイトから 48 バイトにしました。6.57 億局面の book で、読み込み後のメモリが 35.55 GiB → 30.64 GiB です。
- `book deviate`・`deviate2`・`deviate3` は、局面を選ぶ間だけ 1 局面 1 バイトの表を使います（6.57 億局面で約 0.9 GB）。
- 新しい設定：

| 設定 | 既定 | 内容 |
|---|---|---|
| `book-expand-tasks` | 同梱の `config.ini` は `auto`。`config.ini` がない場合は `1` | `auto` は book の level から同時に展開する局面の数を決める。1 局面の探索に level 18 以下は 2 スレッド、24 以下は 4、それより上は 8 スレッド（32 スレッド・level 18 なら 16 局面） |
| `probcut-model` | `standard` | `refit` は実験用。実測で当てはめ直した ProbCut の誤差モデル |

**互換性**

- 固定 level の探索（book の学習、`-solve`、level 指定の対局）の結果は変わりません。
- book のファイル形式と保存される内容は変わりません。以前の版と book を互いに読み書きできます。
- `book enhance` の誤差を 63 より大きくすると、結果が以前と変わることがあります。評価値の上限・下限が ±127 を超える局面では、±127 に丸められます。誤差が 63 以下なら変わりません。
- `probcut-model = refit` では探索結果が変わり、できる book も `standard` とは変わります。`standard` の book と混ぜる前によく確かめてください。
- `book-expand-tasks = auto` でできる book は、1 局面ずつ展開した場合と同じにはなりません（nikque.4 の `book-expand-tasks` の注意を読んでください）。
- `eval.dat` は変わりません。

**注意**

- 範囲外の上限・下限を持つ book（以前の版で大きな誤差の `book enhance` で作ったもの）を読み込むと、その値は ±127 に丸められます。
- `book deviate2`・`deviate3` の累積損失（2 つめの引数）の上限は 254 です。それより大きい値は 254 として扱い、警告を表示します。
- `book deviate3` などの局面の選択が少し遅くなりました（実 book の `book deviate3 2 5` の 1 周目で 12.6 秒 → 15.5 秒）。選ばれる局面は同じです。
- `refit` を book の学習に使うと、序中盤の学習は遅くなります。

**未確認**：`book-expand-tasks = auto` の速さは level 18 でしか測っていません。

[検証記録](docs/validation/nikque.ja.md#v455-nikque5)

---

## v4.5.5-nikque.4

探索と book の学習を速くし、book に登録できる局面数の上限を約 43 億に引き上げました。複数の局面を同時に展開する学習の設定を追加しました。

**変更**

- book の学習で、局面を探索するたびにハッシュ表を消す処理を省きました。大きなハッシュ表ほど効果があります（level 18・1 スレッド、`-h 26` で 180 秒 → 108 秒）。
- x86-64-v4 版（AVX-512）の評価関数を速くしました。1 スレッドの中盤の探索が 7〜13% 速くなりました。
- Windows 版（ARM64 を除く）の実行ファイルは、PGO（実行の傾向を使ったコンパイラ最適化）でビルドします。
- book に登録できる局面数の上限を、2,147,483,647 から 4,294,967,295 にしました。
- 新しい設定：

| 設定 | 既定 | 内容 |
|---|---|---|
| `book-expand-tasks` | `1` | `book deviate`・`deviate2`・`deviate3`・`enhance`・`play` で n 局面を同時に展開する。それぞれ `n-tasks / n` スレッドと専用のハッシュ表を使う。コマンドラインは `-book-expand-tasks n` |
| `hash-table-size` | 同梱の `config.ini` は `auto`。`config.ini` がない場合は 21 | `auto` はスレッド数から選ぶ（1〜3 で 21、4〜15 で 22、16〜63 で 23。上限 25、メモリの 1/32 まで）。コマンドラインは `-h auto` |

- `book-expand-tasks = 16` で、6.57 億局面の book の `book deviate3 2 5`（1 万件の展開、32 スレッド）が 832 秒 → 174 秒になりました。

**不具合の修正**

- 探索用のスレッドを作った直後に解放すると、停止の合図を取りこぼして永久に待つ、またはスレッドが走り出す前のタスクを解放することがありました。→ 確実に止めるようにしました。

**互換性**

- 同じオプションなら探索結果は変わりません。book のファイル形式も変わりません。
- `hash-table-size = auto` でハッシュ表の大きさが変わると、`-h` を変えたときと同じく探索結果もわずかに変わります。
- `book-expand-tasks` を 2 以上にすると、できる book は 1 局面ずつ展開した場合と同じではありません。展開される局面と Link の手は同じでしたが、一部の Leaf と評価値が 1 か 2 違いました。
- 2,147,483,647 局面以下の book は、これまでとバイト単位で同じに保存されます。それを超える book は、以前の版や上流の Edax では読めません。

**注意**

- `book-expand-tasks` が 2 以上のとき、同時に展開している局面どうしはお互いを参照しません。同じ周の局面が同じ新しい局面に行き着く場合などに、その時点では Link が張られません（後の周で張られることがあります）。
- 展開対象が n より少ない周では、同時に動く探索も減ります。
- level が高いほど、`book-expand-tasks` は小さめにしてください（32 スレッドなら 4 や 8 から始めて、速さを比べてください）。
- `book-expand-tasks` の n 個の探索は、それぞれハッシュ表を持ちます。`hash-table-size` を数値で指定した場合は、その n 倍のメモリを使います。
- `book-expand-tasks` は `n-tasks` を超えられません。学習中に対局や解析もするときは、`n-tasks`（と `book-expand-tasks`）を減らしてください。
- 43 億局面の book には約 240 GB のメモリが必要です。

**未確認**：`book-expand-tasks` は level 18 でしか測っていません。ARM64 の Windows 版と Android 版はビルドだけで、実行していません。

[検証記録](docs/validation/nikque.ja.md#v455-nikque4)

---

## v4.5.5-nikque.3

数億局面の大きな book で、読み込み・保存・`book deviate` 系・`book merge` を大きく速くし、メモリを減らしました。11 件の不具合を直し、`book merge` の後に自動で保存する設定を追加しました。

**変更**

- 6.57 億局面・28.95 GB の book で、読み込みが 445.6 秒 → 48.9 秒、読み込み後のメモリが 58.4 GiB → 35.5 GiB、保存が 657.2 秒 → 28.3 秒になりました。
- `book deviate 2 4` の 1 周（展開対象 0 件）が 2,909.6 秒 → 142.0 秒になりました。
- negamax、`book deviate`・`deviate2`・`deviate3` の展開対象の選択、`book merge` の Link の再構築・検査・並べ替えを、`n-tasks` のスレッドで並列に行います。結果は同じです。
- `book merge` は統合元の book を読み込まずに、ファイルを 2 回読みます。統合元が壊れている場合は、統合先を変えません。
- `book merge` は、統合元が同じ level ですでに探索した Leaf を使い、探索し直しを省きます。6.6 億局面どうしの merge が、4 時間 33 分・117.2 GiB から 263 秒・37.0 GiB になりました。
- 新しい設定 `book-merge-auto-save`（既定 `on`。コマンドラインは `-book-merge-auto-save on/off`）：`book merge` が成功すると、book を `<book ファイル名>.mrg`（既定では `data/book.dat.mrg`）に保存し、`Merged book saved to data/book.dat.mrg` と表示します。
- book の学習と同時に対局や解析をするときは、`n-tasks`（`-n`）でスレッド数を指定できます。

**不具合の修正**

- book にない局面を指す Link で、negamax などが異常終了していました。→ 読み飛ばし、`book fix` がその Link を取り除きます。
- パイプで入力したとき、book の読み込み中に `quit` が届くと異常終了していました。→ 直しました。
- `book fill` が局面の追加中に解放済みのメモリへ書き込み、異常終了することがありました。→ 直しました。
- メモリが足りないとき、局面を黙って失っていました。→ 報告して学習を止めます。
- book の保存で、ファイルの内容をディスクに確定させる前に、以前のファイルと置き換えていました。→ 確定させてから置き換えます。
- `book merge` で追加した局面のうち、初期局面からたどれない局面に ±127 の評価値が残っていました。→ 直しました。
- 空・途中切れ・不正な手を含む `.edx` を読むと、現在の棋譜が消えていました。→ 読み込みを中止し、棋譜を残します。
- Windows で、OS の起動から約 49.7 日で時計の値が巻き戻っていました。→ 直しました。
- book のヘッダーの日時に、初期化していない 1 バイトを書き出していました。→ 0 を書きます。
- 約 21 億局面を超えると、局面数が桁あふれしていました。→ 追加を拒否します。
- バケット数の上限が 2^26 で、10 億局面を超えると検索が遅くなっていました。→ 局面数に応じて決めます。

**互換性**

- book のファイル形式は変わりません。以前の book をそのまま読み書きできます。
- `book merge` で統合元の Leaf を使った局面と、その親局面の評価値が、わずかに変わることがあります（差はほとんどが 1〜2）。局面の集合、Link の手、探索レベルは同じです。nikque.2 でも、探索し直す局面の評価値は実行のたびに同じ程度に揺れます。
- `book merge` で追加した局面のうち、初期局面からたどれない局面の評価値が変わります。
- 統合後の局面数が統合先のバケット数の 32 倍を超える `book merge` では、保存される局面の順序が変わります。局面の内容は同じです。
- `book-merge-auto-save = off` では、`book merge` は以前とまったく同じ動作です。
- 上流 v4.5.5 の `book merge` は Link を張り直さず、`book fix` の実行を求めます。この版は merge の中で Link の再構築・修復・negamax・並べ替えまで行います。

**注意**

- 自動保存は元の book（`data/book.dat`）を上書きしません。`.mrg` を使うには、`book load data/book.dat.mrg` で読み込むか、ファイル名を変えてください。merge が失敗した場合は保存しません。終了時の保存は以前と同じです。
- `book load` で別の book を読み込む間は、失敗したときに現在の book を残すため、一時的に 2 冊分のメモリを使います。
- 展開対象が多い `deviate2`・`deviate3` の 1 周は、大半が 1 件ずつの探索なので、あまり速くなりません。
- 32 ビット Linux 版は i486 以降の CPU が必要です。32 ビット macOS 版は引き続き配布していません。

[検証記録](docs/validation/nikque.ja.md#v455-nikque3)

---

## v4.5.5-nikque.2 まで（最初の修正版）

上流の Edax v4.5.5 をもとに、`book deviate2`・`book deviate3` と book の自動保存の設定を追加し、18 件の不具合を直した最初の版です。

**変更**

- この fork は、上流のタグ `v4.5.5`（`4cde6ff588f0eade07fcba0c7f02d5cd0cacd4ee`）をもとにしています。
- book の学習のコマンドを 2 つ追加しました。どれも現在の局面から Link をたどり、条件に合う Leaf を展開します。

| コマンド | 対象とする条件 |
|---|---|
| `book deviate 2 4` | 従来の相対誤差と、開始局面からの絶対誤差（従来どおり） |
| `book deviate2 5 5` | 1 手の損失が最大 5、黒白の合計も最大 5。book の学習レベルで完全読みした Leaf を除く |
| `book deviate3 5 5` | 条件は `deviate2` と同じで、完全読みした Leaf も含む |

- 新しい設定（`config.ini`）：

| 設定 | 内容 |
|---|---|
| `book-save-interval` | 展開中に時間の間隔で保存する（単位は分）。`0` で無効 |
| `book-deviate-save-rounds` | 増分があった周の何周ごとに保存するか（3 種類の deviate に共通）。`1`（既定）は毎周、`0` は学習の完了時だけ、`N` は N 周ごとと完了時 |

- 配布版（Release の ZIP）には、再ビルドした Windows・Linux・macOS x64・Android 用の実行ファイル、`bin/data/eval.dat`、初期の `bin/data/book.dat`、問題集が入っています。`eval.dat` は上流 v4.5.5 の配布物のままです。

**不具合の修正**

| 分野 | 修正の内容 |
|---|---|
| book の学習 | 従来の `book deviate` で、最初の展開の後に開始局面を取り直します。以前は解放済みのメモリを読み、学習が止まったり局面が抜けたりすることがありました |
| book の学習 | `book enhance` を、展開が収まるまで続け、増分があった周ごとに保存します |
| book の merge・保存 | book は一時ファイルに書いて確かめ、書き終えてから保存先と置き換えます |
| book の merge・保存 | 途中で切れた・余分なレコードを含む book を拒否します。`book load`・`book merge` が失敗したときは現在の book を残し、壊れた起動時の book を上書きしません |
| ファイル | Windows で `.edx` をバイナリモードで読み書きします |
| ファイル | 対応していない棋譜の保存の拡張子は、ファイルを開く前に拒否します |
| ファイル | 4 文字より短いファイル名を正しく扱います |
| 持ち時間 | XBoard の `level 0` でゼロ除算しません。着手の後に Fischer の加算時間を加えます |
| 持ち時間 | 棋譜解析で履歴をさかのぼる範囲を制限し、パスの履歴を初期化します |
| PGN・GGF | PGN の出力で、密な FEN に十分な領域を取ります |
| PGN・GGF | PGN の FEN タグを読み、時刻タグを読み書きできるようにします |
| PGN・GGF | 最大長の GGF のフィールドに終端の領域を取ります |
| PGN・GGF | PGN の着手の読み込みを 60 手までにします |
| コマンドと通信 | 文字列のフィールドとコマンドの解析で、書き込み先の大きさを守ります |
| コマンドと通信 | GTP の `reg_genmove` で実際の結果を返します |
| コマンドと通信 | GTP の `time_left` で、不正な色を拒否します |
| コマンドと通信 | GTP の応答 ID をコマンドごとに初期化し、エラーのときも正しい順序で返します |
| コマンドと通信 | NBoard のノード数と経過秒数を、正しい形式で表示します |

**互換性**

- 従来の `book deviate` の選択条件は変わりません。
- 上流 v4.5.5 と違い、`book merge` は Link の再構築、不整合な局面（古い `nomove` Leaf を含む）の修復、評価値の計算し直し、着手の並べ替えまで行います。統合先にある局面は上書きしません。
- 上流 v4.5.5 と違い、`book merge` の統合元がない・壊れている場合は、merge を中止して現在の book を残します。
- 上流 v4.5.5 と違い、`book save` で保存した後に変更がなければ、終了時に保存しません。

**注意**

- 従来の `book deviate` の修正は、すでにある book で抜けた局面を自動で補うものではありません。
- book の保存中は、一時ファイルのために book とほぼ同じ大きさの空き容量が必要です。保存に失敗しても、以前の book は残ります。
- 失敗や中断のときは、最後の保存の後の学習内容が失われることがあります。`book-deviate-save-rounds = 0` と時間の間隔での保存の無効を組み合わせるときは注意してください。
- 従来の `book deviate` は、黒白両方の展開で 1 周と数えます。`deviate2`・`deviate3` は 1 回の展開で 1 周です。
- `deviate2`・`deviate3` の `todo` は、その回に選んだ Leaf の局面の数です（棋譜の本数ではありません）。途中経過の book には `.dev`・`.dev2`・`.dev3` が付きます。
- 旧 32 ビット macOS 用の `mEdax-x86` は配布していません。今の Xcode の SDK では、修正版の i386 用の実行ファイルをリンクできないためです。
- 実行ファイルは `bin/` から起動するか、`-eval-file` を指定してください。AVX-512 版には x86-64-v4 対応の CPU が必要です。
- 実行ファイルを再配布するときは、ソース、この改変の告知、`LICENSE` を添えてください。

[検証記録](docs/validation/nikque.ja.md)

---

# eval2 系列

nikque 系列に新しい評価関数（着手可能数の重み）を足した系列です。ブランチ `eval2` から公開しています。各版は、上の「版の対応」の表のとおり、nikque の版の変更をすべて含みます。

## v4.5.5-eval2.19

[v4.5.5-nikque.31](#v455-nikque31) の変更を取り込みました。

**互換性**

- 前の版の `eval.dat` と book を引き続き使えます。探索・学習の設定も前の版と同じです。

**未確認**。実 book、対局での強さ、実際の GGS サーバーとの通信。

[検証記録](docs/validation/eval2.ja.md#v455-eval219)

---

## v4.5.5-eval2.18

[v4.5.5-nikque.30](#v455-nikque30) の変更を取り込みました。

**互換性**

- 修正の影響を受ける局面では、探索結果や探索ノード数が変わることがあります。評価関数、`eval.dat`、book のファイル形式は前の版と同じです。

**未確認**。対局での強さ、実 book、SVE の実機での動作と速度。

[検証記録](docs/validation/eval2.ja.md#v455-eval218)

---

## v4.5.5-eval2.17

v4.5.5-nikque.29 の変更を取り込みました（[nikque.29](#v455-nikque29)）。`book correct`・`book leaf recalculate`・`book learn` が、途中で保存された book から続けられるようになりました。`book deepen` が働くようになりました。

**変更**

- `book correct`・`book leaf recalculate`（2・3・4）が、途中で保存された book から続けられるようになりました。終わった局面・Leaf に book の中のマークが付き、もう一度実行するとマークの付いたものを飛ばします。コマンドが終わるとマークは外れます（内容は nikque.29 と同じです）。
- `book learn <ファイル>` が、学習済みの行から続けられるようになりました（内容は nikque.29 と同じです）。
- `book deepen` が働くようになりました。level が book の level より浅い局面に book の level を与え、その level で探索し直します（内容は nikque.29 と同じです）。
- eval2 系列だけの新しい変更はありません。

**互換性**

- マークは、book のファイルの各局面の level の 1 バイト（level は 0〜60）の上の 2 ビットです。ファイルの大きさと形式は変わりません。マークのない book は前と同じです。
- **`book correct`・`book leaf recalculate` が終わる前に保存された book にはマークがあり、v4.5.5-eval2.16 までの版では壊れた book とみなされ、読み込めません。** 終わった book にはマークがありません。
- book のファイルを自分で読むほかのツールは、この 2 ビットを無視する必要があります（nikque GUI 0.4.6 は対応済み）。
- `eval.dat` と探索は変わりません。評価値・手・ノード数は v4.5.5-eval2.16 と同じです（fforum-20-39、1 スレッド：78,733,311 ノード）。
- ほかのコマンドが作る book は v4.5.5-eval2.16 と同じです。

**未確認**：実際の大きな book、強さ（探索は変えていません）。

[検証記録](docs/validation/eval2.ja.md#v455-eval217)

---

## v4.5.5-eval2.16

v4.5.5-nikque.28 の変更を取り込みました（[nikque.28](#v455-nikque28)）。空き 17 マス以上の level 60 の完全読みで、まれに評価値（と手）を誤っていたのを直しました。ほかは変わりません。

**不具合の修正**

- 完全読みで、まれに根の評価値と手を誤っていました（誤りは数石）。直し方は nikque.28 と同じです。
- 同じ誤りは、この版の元の奥原俊彦氏の Edax 4.5.5・4.5.6 にもあります。

**互換性**

- `eval.dat`・book のファイル形式は変わりません。
- 正しかった完全読みの評価値は変わりません。ノード数は少し変わります（fforum-20-39 を 1 スレッドで 78,733,311、v4.5.5-eval2.15 は 78,733,223。評価値と手は同じ）。
- 終盤の深さに届かない中盤の探索は変わりません。
- 前の版で完全読みした局面（たとえば level 18 の空き 20 マス以下の葉）を含む book には、まれにこの誤りの値が入っていることがあります。

**未確認**：強さ、実 book。

[検証記録](docs/validation/eval2.ja.md#v455-eval216)

---

## v4.5.5-eval2.15

v4.5.5-nikque.27 の変更を取り込みました（[nikque.27](#v455-nikque27)）。

**変更**

- 起動のときにほかのプログラムが開いていた book を置き換えない直しを取り込みました。
- libedax の `edax_book_file_status` を取り込みました。
- `book import`・`book merge`・`book deviate` の仲間の `export`・メモリが足りないときの `book moveinfo` などの直しを取り込みました。
- 整理したコードを取り込みました。
- どれも内容は nikque.27 と同じです。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.14 と同じです。
- 答えが eval2.14 と違うのは、nikque.27 の場合だけです。

[検証記録](docs/validation/eval2.ja.md#v455-eval215)

---

## v4.5.5-eval2.14

v4.5.5-nikque.26 の変更を取り込みました（[nikque.26](#v455-nikque26)）。

**変更**

- `book deviate`・`book deviate2`・`book deviate3` の `export` を取り込みました。book を広げずに、たどる手順を書き出します。
- libedax の `edax_book_deviate_export` を取り込みました。
- `@cancel id=N state=none export=1` の印を取り込みました。
- どれも内容は nikque.26 と同じです。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.13 と同じです。
- `export` を付けないコマンドは eval2.13 と同じです。

[検証記録](docs/validation/eval2.ja.md#v455-eval214)

---

## v4.5.5-eval2.13

v4.5.5-nikque.25 の変更を取り込みました（[nikque.25](#v455-nikque25)）。

**変更**

- うみがめ数の `exact=0` が付く値が、ビルドした環境によらず同じになる直しを取り込みました（内容は nikque.25 と同じです）。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.12 と同じです。
- `exact=0` が付く値のほかは、答えは eval2.12 と同じです。

[検証記録](docs/validation/eval2.ja.md#v455-eval213)

---

## v4.5.5-eval2.12

v4.5.5-nikque.24 の変更を取り込みました（[nikque.24](#v455-nikque24)）。

**変更**

- うみがめ数・局面の数・`book levels`・`book missing` の答えの引き継ぎを取り込みました（内容は nikque.24 と同じです）。
- マニュアル：12.4 に、nikque のマニュアルと同じ文を書きました。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.11 と同じです。
- `book moveinfo`・`book levels`・`book missing` の答えは eval2.11 と同じです。
- ただし、うみがめ数の `exact=0` には nikque.24 と同じ例外があります。

[検証記録](docs/validation/eval2.ja.md#v455-eval212)

---

## v4.5.5-eval2.11

v4.5.5-nikque.23 の変更を取り込みました（[nikque.23](#v455-nikque23)）。

**変更**

- `book moveinfo` の精度の段階の引き継ぎを取り込みました（内容は nikque.23 と同じです）。
- book のコマンドを `book` なしで入力したときの案内を取り込みました（内容は nikque.23 と同じです）。
- マニュアル：book のコマンドを全部 `book …` の形で書きました。
- マニュアル：nikque のマニュアルの言い回しの見直しを、eval2 のマニュアルにも当てました。対象の版・版の名前・`eval.dat` の版・記録の置き場を eval2 に合わせました。nikque の版で測った数には、そう書きました。
- nikque.23 の文書の書き直し（README・修正一覧・記録の置き場・`README-NIKQUE.*` を消す）は、eval2 には入っていません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.10 と同じです。
- `book moveinfo` の答えは eval2.10 と同じです。

[検証記録](docs/validation/eval2.ja.md#v455-eval211)

---

## v4.5.5-eval2.10

v4.5.5-nikque.22 の変更を取り込みました（[nikque.22](#v455-nikque22)）。

**変更**

- `book reduce`・`book reducecount` の時間とメモリが減り、新しい設定 `book-reduce-index`・`book-reduce-reuse`・`book-reduce-shrink`・`book-reduce-walk` が使えます（内容は nikque.22 と同じです）。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.9 と同じです。
- book のファイル形式は変わりません。`book reduce` の結果は nikque.22 と同じです。

[検証記録](docs/validation/eval2.ja.md#v455-eval210)

---

## v4.5.5-eval2.9

v4.5.5-nikque.21 の変更を取り込みました（[nikque.21](#v455-nikque21)）。

**変更**

- `book reducecount` の並列化、`book reduce` の処理の組み直しを取り込みました（内容は nikque.21 と同じです）。
- eval2 系列だけの新しい変更はありません。

**不具合の修正**

- 壊れた book で `book reducecount` が終わらない不具合と、誤った入力でも `book reduce` が局面を消す不具合を直しました（eval2.8 で入ったもの。直し方は nikque.21 と同じです）。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.8 と同じです。
- 正常な book で残る局面は eval2.8 と同じです。book のファイル形式は変わりません。

[検証記録](docs/validation/eval2.ja.md#v455-eval29)

---

## v4.5.5-eval2.8

v4.5.5-nikque.20 の変更を取り込みました（[nikque.20](#v455-nikque20)）。

**変更**

- book を小さくするコマンド `book reduce` と、事前に数を確かめる `book reducecount` が使えます（内容は nikque.20 と同じです）。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.7 と同じです。
- book のファイル形式は変わりません。

[検証記録](docs/validation/eval2.ja.md#v455-eval28)

---

## v4.5.5-eval2.7

v4.5.5-nikque.19 の変更を取り込みました（[nikque.19](#v455-nikque19)）。

**変更**

- GUI のための book の解析コマンド `book moveinfo`・`book missing`・`book levels`・`book bestlines`・`book findline` と、`stop id=N` が使えます（内容は nikque.19 と同じです）。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.6 と同じです。
- book のファイル形式は変わりません。

[検証記録](docs/validation/eval2.ja.md#v455-eval27)

---

## v4.5.5-eval2.6

v4.5.5-nikque.18 の変更を取り込みました（[nikque.18](#v455-nikque18)）。

**変更**

- 実行ファイルに、ビルドしたときのフォルダーが入らなくなりました（nikque.18 と同じです）。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.5 と同じです。
- book のファイル形式と、`book-reproducible` で作る book は eval2.5 と同じです。

**注意**

- 32 ビット版 `wEdax-x86-sse.exe` は、eval2.5 より少し遅い向きです（32 スレッドの `book learn` 1.007 倍・`book leaf expand` 1.013 倍、1 スレッドの `book leaf expand` 1.058 倍）。PGO のビルドの違いと見ています。ほかの実行ファイルの速さと、最大メモリは変わりません。

**未確認**：`wEdax-x86-sse.exe` が遅い向きになった理由。

[検証記録](docs/validation/eval2.ja.md#v455-eval26)

---

## v4.5.5-eval2.5

v4.5.5-nikque.16・17 の変更を取り込みました（[nikque.17](#v455-nikque17)・[nikque.16](#v455-nikque16)）。

**変更**

- nikque.16：2 回目のクロスチェックで見つかった不具合の修正、`base unique` などの高速化、libedax の関数 `edax_book_keep_unread_file`、Cassio の EOF、MSVC の 32 ビット AVX2 構成のインライン化。
- nikque.17：book を毎回同じに作るための設定 `book-seed`・`book-reproducible`（既定の動作は変わりません）。
- eval2 系列だけの新しい変更はありません。評価関数の 4 ファイル以外のソースは nikque.17 と同じです。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.4 と同じです。
- book 関係の変更は nikque.16・17 と同じです。

**未確認**：この版の速さ・強さ（評価関数は eval2.1 と同じ）、eval2 独自の book のテスト一式、29 GB の実 book、Android・Windows ARM64・macOS の実機での実行。

[検証記録](docs/validation/eval2.ja.md#v455-eval25)

---

## v4.5.5-eval2.4

v4.5.5-nikque.15 の変更を取り込みました（[nikque.15](#v455-nikque15)）。

**変更**

- クロスチェックで見つかった不具合の修正と、速さ・メモリの改善を取り込みました（内容は nikque.15 と同じです）。

**不具合の修正**

- eval2 系列の評価関数のファイルにも、nikque.15 と同じ 2 つの修正をしました。
- 評価ファイルの印の検査は、4 バイトの語が2つとも同じバイト順で一致したときだけ通すようにしました。
- libedax を別の `eval-file` で初期化し直したとき、そのファイルを読むようにしました。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.3 と同じです。
- book のコードは nikque.15 と同じです。

**注意**

- この版は、ビルドと起動の確認だけで公開しました。テストは公開の後に行いました。

**未確認**：この版の速さ・強さ、実 book での動作、eval2 独自の book のテスト一式。

[検証記録](docs/validation/eval2.ja.md#v455-eval24)

---

## v4.5.5-eval2.3

v4.5.5-nikque.14 の変更を取り込みました（[nikque.14](#v455-nikque14)）。

**変更**

- 新しいコマンド `book fill2`・`book leaf expand`、`book fill` の範囲の指定と高速化、libedax の関数の追加を取り込みました（内容は nikque.14 と同じです）。
- `book leaf-recalculate` の名前が `book leaf recalculate` になりました（nikque.14 の変更）。eval2.2 の説明にある `book leaf-recalculate` は、この版から `book leaf recalculate` と書きます。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.2 と同じです。
- 上流の `eval.dat` で使うと、nikque.9 までと同じ結果・同じノード数です。
- book のコードは nikque.14 と同じです。

**未確認**：この版の速さと強さ、新しいコマンドを eval2 の `eval.dat` で使ったときの時間、eval2 独自の book のテスト一式。

[検証記録](docs/validation/eval2.ja.md#v455-eval23)

---

## v4.5.5-eval2.2

v4.5.5-nikque.10〜13 の変更を取り込みました（[nikque.13](#v455-nikque13)・[nikque.12](#v455-nikque12)・[nikque.11](#v455-nikque11)・[nikque.10](#v455-nikque10)）。

**変更**

- book の不具合の修正、`book leaf-recalculate`、`book subtree`・`book prune` の高速化、`book-expand-tasks = auto` の変更、libedax の関数の追加などを取り込みました（内容は nikque.10〜13 と同じです）。
- `eval.dat` を替えた後の book の評価値は、`book leaf-recalculate` 系のコマンド（nikque.12 で追加）で計算し直せます。上流の `eval.dat` で作った book を eval2 の `eval.dat` で計算し直すと、評価値や手が変わる Leaf があります。上流の `eval.dat` に戻して計算し直すと、元の book に戻ります。完全読みの局面は対象外です。
- 32 スレッドの `book deviate2 5 5`（6 億 6162 万局面の book）で、5 分間に広げる数が eval2.1 より約 13% 増えました（11,673 → 13,240）。差の約 8 割は `book-expand-tasks = auto` の決め方の変更によるものです。
- eval2 系列だけの新しい変更はありません。

**互換性**

- 評価関数と `eval.dat` は eval2.1 のままです（同じファイル）。
- 探索の結果（評価値・ノード数）は eval2.1 と同じです。
- 上流の `eval.dat` で使うと、nikque.13 と同じ結果・同じノード数です。

**未確認**：この版の強さと、対局・探索の速さ（eval2.1 の数字は nikque.8・9 のソースでのもの）、book のコマンドの速さを eval2 の `eval.dat` で測ったもの（nikque.10〜12 の数字は上流の `eval.dat` でのもの）、大きな book での `book leaf-recalculate` の時間。

[検証記録](docs/validation/eval2.ja.md#v455-eval22)

---

## v4.5.5-eval2.1

評価関数（`eval.dat`）を変えた別の系列 eval2 の最初の版です。v4.5.5-nikque.9 のソースに、評価関数の変更だけを足しました。強くなり、level 18 以上では速くなりますが、評価値と探索の結果が変わります。

**変更**

- 評価関数に、両者の着手可能数（打てる場所の数）を足しました。46 個のパターンはそのままです。
- 重みは、上流（v4.5.5）の重みを出発点にして、全部を学習し直しました。
- 学習データは、Edax 自身の自己対局（level 8、約 84 万局）、この fork で学習した book、[Egaroucid](https://www.egaroucid.nyanyan.dev/) のサイトで公開されている学習データです。Egaroucid のパターンや学習ずみの重みは使っていません。学習データは配布版に入っていません。
- ブランチ `eval2` から公開しています。既定のブランチと最新の Release は v4.5.5-nikque.9 のままです。これまでと同じ評価値が必要なときは、nikque.9 を使ってください。
- 強さ（上流の `eval.dat` との対局。同じ実行ファイル、1 スレッド、book なし、各約 2200 局。かっこは 95% 信頼区間）：

| level | Elo |
|---|---|
| 6 | +22.1 [+8.5, +35.7] |
| 10 | +18.8 [+6.1, +31.5] |
| 18 | +17.0 [+6.6, +27.5]（別の開始局面で +20.0 [+9.5, +30.5]） |
| 21 | +17.4 [+7.9, +26.8] |

- level 18 の評価値の、完全読みの結果との平均の誤差が 1.089 石 → 1.067 石になりました。
- 速さ（nikque.9 の配布版＋上流の `eval.dat` に対する時間の比。1 より小さいと速い）：

| 実行ファイル（1 スレッド） | level 10 | level 18 | level 21 | 完全読み |
|---|---|---|---|---|
| `wEdax-x86-64-v4.exe`（AVX-512） | **1.055** | 0.945 | 0.877 | **1.026** |
| `wEdax-x86-64-v3.exe`（AVX2） | **1.045** | 0.943 | 0.878 | **1.036** |
| `wEdax-x86-64.exe`（標準） | **1.135** | 0.994 | 0.916 | **1.029** |
| `wEdax-x86-sse.exe`（32 ビット SSE2） | **1.204** | **1.037** | 0.949 | 0.97（±0.05） |
| `wEdax-x86.exe`（32 ビット） | **1.239** | **1.070** | 0.973 | **1.034** |

- 速くなる条件：AVX2・AVX-512 版の level 18〜24（1〜32 スレッドで 2〜15% 短い。AVX-512 版の level 24 は 1 スレッド 0.847 倍・32 スレッド 0.902 倍）。読む局面の数が減るためです。
- 遅くなる条件：level 10 などの浅い探索、32 ビット版の level 18、完全読み（1 スレッドで 3〜4%、fforum 40〜59 で 279.6 秒 → 290.1 秒。32 スレッドで約 6%、22.5 秒 → 23.8 秒）。評価関数を呼ぶたびに着手可能数を数えるためです。
- 完全読みを主に使う場合、浅い level で大量に対局・学習する場合、32 ビット版を使う場合は、nikque.9 のほうが速いです。

**互換性**

- `eval.dat` は版 3.3.0 の新しい形式です（13,960,248 バイト、SHA-256 `1870f8fa5eecb6df972a4d83224832f9a620745cdc67c2d9b17bcd0ba09ea172`）。これまでの内容の後ろに、着手可能数の表を足しました。
- `eval.dat` と実行ファイル（ライブラリ）は一組で使ってください：

| 実行ファイル（ライブラリ） | `eval.dat` | 動作 |
|---|---|---|
| この版 | この版（3.3.0） | 新しい評価関数 |
| この版 | 上流のもの（nikque.9 までの配布版に入っているもの） | nikque.9 と同じ結果・同じノード数。時間は 0.98〜1.013 倍 |
| nikque.9 以前、上流 | この版（3.3.0） | **使わないでください。** 読み込めますが、着手可能数の表が読まれず、弱くなります |

- book のファイル形式は同じで、これまでの book をそのまま読めます。ただし評価値が変わるので、この版で学習を続けると、1 つの book に新旧の評価値が混ざります。混ぜたくないときは、book を分けてください。
- 設定、コマンド、libedax の API、最大メモリは nikque.9 と同じです。

**注意**

- libedax を使うプログラムでは、ライブラリ（この版の `libedax-*`）と `data/eval.dat` の両方を差し替えてください。
- edax_runner v5.3.0-nikque.3 に入っているのは、nikque.9 のライブラリと上流の `eval.dat` です。
- `probcut-model = refit`（nikque.5 の実験的な設定）の係数は上流の重みに合わせたままで、新しい重みでは調整していません。既定の `standard` は、新しい重みで測り直したうえで変えていません。

**未確認**：この版のライブラリに差し替えた edax_runner、book の学習の時間、level 10 より浅い level の速さ、level 24 以上・複数スレッド・ほかのプログラムとの対局、新しい `eval.dat` での book のテスト、macOS・Android・Windows ARM64 版の動作。

[検証記録](docs/validation/eval2.ja.md#v455-eval21)
