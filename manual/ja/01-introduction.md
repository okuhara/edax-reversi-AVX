# 1. はじめに

[目次](README.md) ｜ 次：[2. 起動と最初の対局](02-quick-start.md)

## 1.1 Edax とは

Edax は、オセロ（リバーシ）を打つプログラムです。作者は Richard Delorme 氏です。

Toshihiko Okuhara 氏が、新しい CPU の命令セット（AVX2・AVX-512 など）を使って高速化した版を公開しています。この版は Edax 4.5.x（`okuhara/edax-reversi-AVX`）です。

この**修正版**（`Nikque/edax-reversi-AVX`）は、その v4.5.5 を元にしています。版の名前は `v4.5.5-nikque.N` です。主な変更は次のとおりです。

- 不具合の修正（book の処理、複数スレッドの探索、棋譜の読み書きなど）
- 大きな book（数億局面）を扱うための、速度とメモリ使用量の改善
- book を育てる・保守するコマンドの追加（`book deviate2`・`book deviate3`・`book learn`・`book leaf recalculate`・`book leaf expand`・`book fill2` など）
- ほかのプログラムから Edax を呼び出すためのライブラリ（libedax）

評価データ（`eval.dat`）と book のファイル形式は、元の Edax 4.5.5 と同じです。元の Edax の book は、そのまま読めます。

版ごとの変更点は、配布版の `RELEASE-NOTES.ja.md` にあります。このマニュアルは使い方の説明です。

## 1.2 Edax でできること

| したいこと | 章 |
|---|---|
| Edax と対局する。打った手の良し悪しを見る | [2](02-quick-start.md)・[3](03-playing.md) |
| ある局面の最善手と評価値を調べる | [3](03-playing.md)・[5](05-search.md) |
| 終盤の局面を最後まで読み切る（完全読み） | [5](05-search.md)・[11](11-solve-bench.md) |
| 定石のデータ（book）を作る・育てる・保守する | [6](06-book-basics.md)〜[9](09-large-books.md) |
| 棋譜のファイルを変換する・調べる | [10](10-files.md) |
| 画面つきのソフト（GUI）や自作のプログラムから使う | [12](12-integration.md) |

Edax そのものには、マウスで石を置くような画面はありません。**文字を入力して操作するプログラム**です。

- 盤はコンソールに文字で表示されます。
- 手は `f5` のように入力します。

盤を絵で見ながら使いたい場合は、Edax に対応した GUI のソフトと接続します（[12 章](12-integration.md)）。

## 1.3 配布版の中身

Release のページからダウンロードした ZIP を展開すると、次のものが入っています。

| 場所 | 中身 |
|---|---|
| `bin/` | 実行ファイル（下の表）、ライブラリ（libedax）、設定ファイル `config.ini`、元の Edax に付いていた Windows 向けの説明 `README.MS-Windows.txt`（英語） |
| `bin/data/eval.dat` | 評価データ。**これがないと Edax は起動しません** |
| `bin/data/book.dat` | 最初の book（初期局面が 1 つ入っているだけの、ほぼ空の book） |
| `problem/` | 終盤の問題集（`fforum-1-19.obf` など 4 ファイル。[11 章](11-solve-bench.md)） |
| `README.md` | この版の概要（英語・日本語）と、元の Edax の説明 |
| `RELEASE-NOTES.ja.md`・`RELEASE-NOTES.md` | 版ごとの変更点（テストと実測の記録は、ソースの `docs/validation/`） |
| `manual/ja/`・`manual/en/` | このマニュアル（日本語・英語。v4.5.5-nikque.13 から） |
| `LICENSE` | ライセンス（GPL-3.0） |

## 1.4 どの実行ファイルを使うか

`bin/` には、OS と CPU ごとの実行ファイルが入っています。機能と探索の結果はどれも同じで、速さだけが違います。

| お使いの環境 | 実行ファイル |
|---|---|
| Windows（64 ビット）、ふつうの PC | `wEdax-x86-64.exe` |
| Windows、AVX2 に対応した CPU | `wEdax-x86-64-v3.exe`（上より速い） |
| Windows、AVX-512 に対応した CPU | `wEdax-x86-64-v4.exe`（さらに速い） |
| Windows（32 ビット） | `wEdax-x86-sse.exe`（SSE2 対応の CPU）、`wEdax-x86.exe` |
| Windows（ARM64） | `wEdax-arm64.exe` |
| Linux（64 ビット） | `lEdax-x86-64`、`lEdax-x86-64-v3`（AVX2）、`lEdax-x86-64-v4`（AVX-512） |
| Linux（32 ビット） | `lEdax-x86` |
| macOS | `mEdax-arm64`（Apple silicon）、`mEdax-x64-modern`（Intel） |
| Android | `aEdax-arm64-v8a`、`aEdax-armeabi-v7a` |

- **迷ったら、名前に `v3`・`v4` の付かないもの**を使ってください（Windows なら `wEdax-x86-64.exe`）。どの 64 ビットの CPU でも動きます。
- `v3` の付いた実行ファイルには AVX2 に対応した CPU が、`v4` の付いたものには AVX-512 に対応した CPU が必要です。対応していない CPU では動きません。
- 対応しているかどうか分からないときは、CPU の型番で調べるか、`v3`・`v4` の付いていないものを使ってください。
- 速さの違いの目安は次のとおりです。標準 6,900 万ノード/秒、`v3` 8,100 万、`v4` 9,200 万。
  - 条件：Ryzen 9 9950X、1 スレッド、終盤の完全読みの 20 局面。
  - 出典：テストと実測の記録（`docs/validation/`） の実測。
- テストと実測の記録（`docs/validation/`） によると、Windows の ARM64 用と Android 用は、ビルドできることを確認しただけです。実機では動かしていません。
- macOS 用は、公開用の自動ビルドで作ったものです。
- このマニュアルの内容は、Windows の 64 ビット版で確認しました。

## 1.5 このマニュアルで使う用語（最初に知っておくもの）

くわしくは[用語集](15-glossary.md)にあります。

| 用語 | 意味 |
|---|---|
| 局面 | 盤の上の石の並びと、どちらの手番か |
| 手 | 石を置くマス。`f5` のように書く |
| パス | 打てるマスがないので、相手に手番を渡すこと。`ps` と書く |
| 空き | 空いているマスの数。対局の最初は 60、終局で 0（途中で両者とも打てなくなれば、空きが残ったまま終局） |
| 評価値 | 「最後まで最善を尽くすと、何石差になりそうか」を、**手番の側から見て**表した数。`+4` は手番の側が 4 石勝ちそう、`-2` は 2 石負けそう、`+0` は引き分けそう、という意味 |
| level | 探索の強さを決める数（0〜60）。大きいほど深く読み、時間がかかる（[5 章](05-search.md)） |
| 完全読み | 終局まで全部読んで、正確な石差を求めること |
| book | 序盤〜中盤の局面と、その評価値をためておくデータ。「定石ファイル」（[6 章](06-book-basics.md)） |
| スレッド | CPU が同時に進められる仕事の数。多く使うほど探索が速くなる |

次：[2. 起動と最初の対局](02-quick-start.md)
