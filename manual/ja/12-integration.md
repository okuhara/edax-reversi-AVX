# 12. ほかのプログラムから使う

[目次](README.md) ｜ 前：[11. 問題を解く・速さを測る](11-solve-bench.md) ｜ 次：[13. 困ったときは](13-troubleshooting.md)

Edax は、文字で操作するプログラムですが、ほかのプログラムの思考エンジンとしても使えます。つなぎ方は 3 つあります（12.1〜12.3）。12.4 では、GUI のプログラムが使う book の解析のコマンドを説明します。

| 方法 | 向いている用途 |
|---|---|
| **GUI のソフトとつなぐ**（12.1） | 盤を絵で見ながら、マウスで対局・解析したい |
| **ライブラリ（libedax）を呼ぶ**（12.2） | 自分でプログラムを書いて、Edax の機能を使いたい |
| **edax_runner を使う**（12.3） | 学習させたい手順をリストに書いて、book を自動で育てたい |
| **book の解析のコマンド**（12.4） | GUI のソフトから、book の精度の段階・うみがめ数・局面の数などを数えさせたい |

## 12.1 GUI のソフトとつなぐ

GUI のソフトは、Edax をバックグラウンドで起動し、決まった形式のメッセージ（**プロトコル**）で「この局面で打て」「この手を打った」とやり取りします。Edax は、次のプロトコルに対応しています。

| 起動のときのオプション | プロトコル | 相手 |
|---|---|---|
| （なし） | Edax 自身のコマンド（このマニュアルで説明しているもの） | 人、または Edax のコマンドを直接送るプログラム |
| `-gtp` | GTP（Go Text Protocol） | GTP に対応した GUI |
| `-nboard` | NBoard | NBoard |
| `-xboard` | XBoard（WinBoard） | リバーシに対応した XBoard・WinBoard |
| `-cassio` | Cassio のエンジン用プロトコル | Cassio（macOS） |
| `-ggs` | GGS（インターネット対局サーバー）に、Edax 自身が接続する | GGS |

オプションを付けなくても、最初に受け取ったコマンドが次のどれかなら、Edax はそのプロトコルに切り替わります。

| 最初のコマンド | プロトコル |
|---|---|
| `nboard 1` | NBoard |
| `xboard` | XBoard |
| `protocol_version` | GTP |
| `engine-protocol init` | Cassio |

**接続のしかたの基本**（GUI の側の設定）

1. GUI の「エンジンを登録する」画面で、Edax の実行ファイル（`bin` の中の `wEdax-x86-64.exe` など）を指定します。
2. **作業フォルダー（実行するときのフォルダー）を `bin` にします。** Edax は、そこから `data/eval.dat` を探します（[2.1](02-quick-start.md)）。
   - 作業フォルダーを指定できない GUI では、起動のオプションに `-eval-file` と `-book-file` を、フォルダーを含めた名前で書きます。
3. 必要なら、起動のオプションに、プロトコル（`-gtp` など）・強さ（`-l 18`）・スレッドの数（`-n 4`）を書きます。
   - `config.ini` の設定は、GUI から起動したときにも読まれます。

**このマニュアルの作成で確かめたこと**：GUI のソフトそのものとの接続は、試していません。確かめたのは、3 つのプロトコル（GTP・NBoard・XBoard）で起動して、基本のコマンドに答えることだけです。

GTP の例（`>>>` の行が送ったコマンド、その下が Edax の答え）：

```
>>> protocol_version
= 2

>>> name
= Edax

>>> boardsize 8
=

>>> play black f5
=

>>> genmove white
= d6
```

GTP で使えるコマンドは、`list_commands` で表示されます：`protocol_version`、`name`、`version`、`known_command`、`list_commands`、`quit`、`boardsize`、`clear_board`、`komi`、`play`、`genmove`、`undo`、`time_settings`、`time_left`、`set_game`、`list_games`、`loadsgf`、`reg_genmove`、`showboard`。

NBoard の例：

```
>>> nboard 1
>>> set depth 4
set myname Edax4
>>> go
status Edax is thinking
nodestats 257 0.00
=== d6 2.00 0.0
status Edax is waiting
```

XBoard の例（`protover 2` に対して、対応している機能を答えます）：

```
>>> protover 2
feature setboard=1 playother=1 ping=1 draw=0 sigint=0 sigterm=0 analyze=1 myname="Edax 4.5.5" variants="reversi" colors=0 nps=1 memory=1 smp=1 done=1
```

- プロトコルで動いているときも、対局の強さは `level` と持ち時間で決まり、book も使われます。`level` は、NBoard の `set depth`、XBoard の `sd` でも指定できます。
- 配布版の `bin/README.MS-Windows.txt`（元の Edax の説明。英語）に、NBoard と WinBoard との接続のしかたが書いてあります。古い説明なので、ファイル名などは今の配布版と違います。
- GGS（`-ggs`）は、設定 `ggs-host`・`ggs-port`・`ggs-login`・`ggs-password` で接続先を決めます（このマニュアルの作成では試していません）。

## 12.2 ライブラリ（libedax）

**libedax** は、Edax の機能を、ほかのプログラムから関数として呼べるようにしたものです。

- lavox 氏が Edax 4.4 向けに作ったライブラリと、同じ関数を持ちます。
- libedax 向けに書かれたプログラムは、ライブラリのファイルを差し替えるだけで、この修正版の Edax を使えます。

配布版の `bin/` に入っているライブラリ：

| ファイル | 環境 |
|---|---|
| `libedax-x64.dll` | Windows（64 ビット）。どの CPU でも動く |
| `libedax-x64-v3.dll`・`libedax-x64-v4.dll` | Windows。AVX2・AVX-512 に対応した CPU 用 |
| `libedax-x86-64.so`・`-v3.so`・`-v4.so` | Linux |
| `libedax.universal.dylib` | macOS（Apple silicon と Intel の両用） |
| `libedax-arm64-v8a.so`・`libedax-armeabi-v7a.so` | Android（ビルドの確認だけで、実機では動かしていません） |

使い方の流れ：

1. ライブラリと `data/eval.dat`（必須）を、自分のプログラムと同じフォルダーに置きます。`data/book.dat` と `config.ini` は、必要なら置きます。
2. C・C++ からは、ソースの `src/libedax.h` をインクルードします。ほかの言語からは、その言語の外部関数インターフェース（FFI）でライブラリを読み込みます。Dart には libedax4dart というパッケージがあります。
3. 最初に `libedax_initialize` を呼びます（ここで設定が読まれます）。次に `edax_init` で対局を始め、最後に `libedax_terminate` を呼びます。
4. 関数は、Edax のコマンドに対応しています（`edax_play`・`edax_go`・`edax_hint`・`edax_book_deviate` など）。

短い例（ソースの `tests/libedax_example.c`。説明の文だけ、日本語にしてあります）：

```c
#include <stdio.h>
#include "libedax.h"

int main(void)
{
	char *args[] = {"", "-eval-file", "data/eval.dat", "-book-file", "data/book.dat", "-level", "12", "-n-tasks", "2"};
	char moves[] = "f5d6c3";
	static LibedaxHintList hints;
	LibedaxMove last;
	int i;

	libedax_initialize(9, args);  /* edax.ini、config.ini、この引数の順に設定を読む */
	edax_init();                  /* 新しい対局 */
	edax_play(moves);
	edax_hint(2, &hints);         /* 良い手を 2 つ：hint[1] 〜 hint[n_hints] */
	for (i = 1; i <= hints.n_hints; ++i)
		printf("%c%c %+d\n", 'a' + hints.hint[i].move % 8, '1' + hints.hint[i].move / 8, hints.hint[i].score);
	edax_go();                    /* Edax が 1 手打つ */
	edax_get_last_move(&last);    /* マスの番号：A1 = 0、B1 = 1、…、H8 = 63 */
	libedax_terminate();
	return 0;
}
```

知っておくこと：

- 1 つのプロセスで使える Edax は、1 つです。
- 関数は、1 つのスレッドから順に呼びます。実行中の処理を止める `edax_stop` と `edax_book_stop_count_bestpath` だけは、別のスレッドから呼べます。
- book は、メモリに読み込んで使い、保存のときにファイルを置き換えます。同じ book のファイルを 2 つのプロセスで学習して保存すると、後に保存した方だけが残ります。1 つの book のファイルに書くプロセスは、1 つにします。
- **設定**は、作業フォルダーの `edax.ini`、`config.ini`、`libedax_initialize` の引数の順に読みます（後のものが優先）。設定の名前と書き方は、Edax 本体と同じです。
- **book を変える関数と、棋譜のファイルを書く関数（`edax_base_complete`・`edax_base_correct`）の実行中は、`edax_stop` が探索を止めません。** 途中で打ち切った探索の結果が、book やファイルに入るのを防ぐためです。
- `edax_get_moves` が返す手順は、80 手（着手とパスの合計）までです。
- Windows では、ファイル名を UTF-8 として読みます。UTF-8 として正しくないときは、その PC の文字コード（ANSI）として読みます。
  - 日本語の名前を、その PC の文字コード（CP932）で渡すと、まれに別の名前として読まれます。そのバイトの並びが、たまたま UTF-8 としても正しいときです。たとえば、半角カタカナ 2 文字の「ﾐｽ」は「н」と読まれます。
  - このとき、ファイルを作ると、UTF-8 として読んだ名前（この例では「н」）になります。読み込みでは、その名前のファイルがなければ、ANSI の名前のファイルを読みます。
  - これは v4.5.5-nikque.13 の点検で確かめました。呼び出す側の文字コードは分からないので、この動作は変えていません。
  - ファイル名は、半角の英数字にしておくのが安全です。

### くわしい説明（元の libedax との違い・追加した関数・ビルド）

**互換性**：関数と、呼び出し側とやり取りするデータの並びは、元の libedax と同じです（`src/libedax.h`）。Edax 4.5 で内部の構造が変わっているので、内部のデータはそのまま渡しません。元の並びのデータに詰め替えて渡します。

**追加した関数**

- `edax_book_deviate2`・`edax_book_deviate3`：`book deviate2`・`book deviate3`。
- `libedax_cpu_level`：CPU が動かせる版を返します。`libedax-x64.dll` に問い合わせてから、v3・v4 を読み込む使い方ができます。
- `edax_book_store_games`：複数の棋譜をまとめて対局・学習します。`book learn` と同じ処理です。棋譜は、1 行に 1 局の文字列で渡します。
- `edax_book_store_tasks`：同時に学習する棋譜の数を返します。`book-store-tasks` が `auto` なら、`n-tasks` の値です。
- v4.5.5-nikque.9 で追加：
  - `edax_book_save_checked`：book を保存し、保存できたかどうかを返します。
  - `edax_book_failed`：直前の book の処理が、局面を追加できなかったかどうかを返します。
- v4.5.5-nikque.12 で追加：`edax_book_leaf_recalculate`・`edax_book_leaf_recalculate2`・`3`・`4`（`book leaf recalculate` 系列のコマンド。v4.5.5-nikque.13 までの名前は `book leaf-recalculate`）。
- `edax_book_keep_unread_file(file)`（v4.5.5-nikque.16 で追加）：読めなかった book のファイルを、別のファイルに保存した book で置き換える前に、`<file>.damaged` として残します。
  - 置き換えてよければ 1、置き換えてはいけなければ 0 を返します。
- v4.5.5-nikque.27 で追加：`edax_book_file_status(file)`。読み込むはずだった book のファイルがどうなったかを返します。
  - 0：置き換えてよい（読めた、または読み込みに失敗していない）。
  - 1：book として読めなかった（壊れている）。保存の前に `.damaged` として残す。
  - 2：開けなかった（ほかのプログラムが共有なしで開いていた、など）。2 秒ほど待っても開けなかった。どの保存もこのファイルを置き換えず、`edax_book_keep_unread_file` は 0 を返す。
  - 初期化の前は −1 です。
  - 初期化の後に呼べば、開けなかった book のファイルに気づけます。空の book に学習を始めずに、止められます。
- v4.5.5-nikque.26 で追加：`edax_book_deviate_export(base, x, y, file, which)`。`book deviate`・`book deviate2`・`book deviate3` の `export` です（[7.3](07-book-learning.md)）。
  - book を広げずに、今の局面からたどる手順を、ファイル `file` に書き出します。
  - `base` は、1 が `book deviate`、2 が `book deviate2`、3 が `book deviate3` です。
  - `which` は、0 が `leaf`、1 が `ends`、2 が `all` です。
  - `x`・`y` の範囲は、コマンドと同じです。`book deviate2`・`book deviate3` の `y` は 254 までです。超えるときは、コマンドと同じ警告を出して 254 にします。
  - 書いた行の数を返します。今の局面が book にないときは 0 を返し、空のファイルを作ります。
  - 引数が正しくないとき、ファイルに書けないとき、メモリが足りないときは −1 を返します。

**元の libedax（Edax 4.4）との違い**

- 既定の level は 18 です（元は 21）。同じ level でも、探索の評価値や手が Edax 4.4 と違うことがあります。
- `edax_book_merge` は、この版の `book merge` と同じく、Link の再構築・修正・negamax まで行います。元は、局面を足すだけでした。
- 最善進行の数（`edax_book_count_bestpath`・`edax_book_count_board_bestpath`）：
  - 元の libedax では、book の全局面に、常に記録欄を持たせていました。
  - この版では、数えるときだけ別に確保します。book の局面は 48 バイトのままです。
  - 数え方（下限の値）を変えたときと、book を変更したときは、自動で数え直します。
- `Position` の `link`（手の配列）は、次の 63 局面を取得するまで有効です（v4.5.5-nikque.8 までは 8 局面）。
- `edax_bench` の結果の後ろにあったロックは使いません。
- 元の libedax では異常終了した次の呼び方は、何もしないか、空の結果を返すように変えました。
  - 初期化や終了の二重呼び出し
  - 1 手も打っていないときの `edax_get_last_move`
  - book にない局面の `edax_book_show`
  - 書き換えできない文字列を渡した `edax_get_bookmove_with_position_by_moves`

**Edax 本体には影響しません。** libedax のコードは、ライブラリをビルドするときだけ取り込まれます。libedax を追加した時点のソースからビルドした Edax は、v4.5.5-nikque.6 のソースからビルドしたものとバイト単位で同じでした（ビルド時刻とソースのフォルダー名の文字を除く）。

**速さとメモリ**（1 スレッド、`bench` の終盤 20 局面、Ryzen 9 9950X）

| | 速さ | 最大メモリ |
|---|---|---|
| 元のlibedax（Edax 4.4、x86-64） | 53 百万ノード/秒 | 159 MB |
| `libedax-x64.dll` | 69 百万ノード/秒 | 82 MB |
| `libedax-x64-v3.dll` | 81 百万ノード/秒 | 82 MB |
| `libedax-x64-v4.dll` | 92 百万ノード/秒 | 82 MB |

ライブラリと Edax 本体（同じ CPU 向け）は、ノード数が一致し、速さも同じでした。

**テスト**

- `tests/libedax_test.c` が全関数を呼び出します。
  - 項目の数：279 項目（v4.5.5-nikque.27 から）。
  - これまでの数：最初は 147 項目。v4.5.5-nikque.9 では 191 項目、v4.5.5-nikque.10 では 193 項目、v4.5.5-nikque.12 では 197 項目、v4.5.5-nikque.13 では 198 項目、v4.5.5-nikque.26 では 218 項目（書き出しの 14 項目を含む）。
  - v4.5.5-nikque.27 で足した 61 項目（うち 28 項目は Windows だけ）：
    - book が変わった後の書き出し：9 項目
    - 日本語の名前のフォルダーにある、開けない book：8 項目
    - 読めなかった book のマーク・別の書き方の名前・`edax_book_file_status`：21 項目
    - 起動のときに、ほかのプログラムが開いていた book：17 項目
    - `edax_book_new` の深さ：4 項目
    - `edax_hint` の止め：2 項目
  - Windows 3 種と Linux 3 種で合格しました。
  - 元の libedax でも、対応する項目は同じ結果です。
- libedax4dart 7.67.0 のテスト 29 件のうち、28 件が合格しました。
  - 残る 1 件は、探索の評価値の比較です。既定の level の違いと、直前の探索の状態によるものです。
  - 同じ level で単独に探索すると、元の libedax と同じ評価値になります。
- Edax 4.4 の libedax が保存した book をこの版で読めること、この版が保存した book を Edax 4.4 の libedax で読めることも確かめました（27 万局面の book で全局面一致）。

**ビルド**

- Windows：`nmake -f NMakefile vc-pgo-lib`（`vc-pgo-lib-x64`・`vc-pgo-lib-x64-v3`・`vc-pgo-lib-x64-v4`）。配布版と同じ PGO のビルドです。`bench 20` と `tests/libedax_test.c` の API のテストで学習します。
- Windows（PGO なし）：`nmake -f NMakefile vc-lib`（`vc-lib-x64`・`vc-lib-x64-v3`・`vc-lib-x64-v4`）。
- Linux など：`make libbuild ARCH=<x86-64|x86-64-v3|x86-64-v4> COMP=gcc OS=linux`。
- Android：`ndk-build -C src NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application-lib.mk NDK_OUT=./obj-lib NDK_LIBS_OUT=./libs-lib`。`src/libs-lib/<ABI>/libedax.so` ができます（NDK r27d で確認）。
- テスト：`tests\build-libedax-test.cmd`。
- macOS：release-binaries ワークフローが arm64 と x86-64 のライブラリを作り、1 つのファイル `libedax.universal.dylib` にまとめます。これは、libedax 用のプログラムが読み込む名前です。

## 12.3 edax_runner

**edax_runner**（`Nikque/edax_runner`）は、**学習させたい手順をリストに書いておくと、上から順に book に学習させていく**小さなプログラムです。

- sensuikan1973 氏の edax_runner を、この修正版の libedax で動くようにしたものです。
- Edax の画面でコマンドを入力する代わりに、テキストファイルを書くだけで済みます。

使い方（edax_runner の README より）：

1. edax_runner の Release から、配布版をダウンロードして展開する。
2. `learning_list.txt` に、学習させたい内容を書く。
3. `config.ini` を、好みに合わせて書き換える（書き方は Edax と同じ。[4 章](04-settings.md)）。
4. 育てたい book があれば、`data/book.dat` に置く。
5. edax_runner を起動する。
6. 学習が済んだ行は、`learned_log.txt` に記録される。

`learning_list.txt` の書き方：

| 目的 | 書き方 | 例 |
|---|---|---|
| その手順から、Edax どうしで 1 局打って学習する（`book learn` の 1 行と同じ） | `book-randomness の値,手順` | `2,F5F6F7F8` |
| その手順の局面から `book deviate` を行う | `[相対誤差 絶対誤差] 手順` | `[1 1] F5F6F7F8` |
| `book fix` を行う | `fix` | `fix` |
| 説明 | `// 説明` | `// ここから虎定石` |
| edax_runner を終わらせる | `exit` | `exit` |

- `2,` を省いて手順だけを書くと、`0,手順` と同じです。
- edax_runner は、1 行が終わるたびに、book を `data/book.dat` に保存し、リストを読み直します（同時に学習する場合は、その 1 組が終わるたび）。
- 実行中にリストを書き換えてもかまいません。
- 同じフォルダーで 2 つ目の edax_runner を起動すると、何もせずに止まります（5.3.0+nikque.10 から）。
- 設定 `book-store-tasks` が 1 以外のとき（既定の `auto` を含む）は、「Edax どうしで 1 局」の行をその数ずつまとめて、同時に学習します。
- この節のこれ以外の内容と、edax_runner の動作そのものは、このマニュアルの作成では確かめ直していません（edax_runner のテストは、別に行っています）。
- くわしくは、edax_runner の `README.ja.md` を見てください。

## 12.4 GUI のための book の解析のコマンド

GUI のソフトが、book をたどって数える計算を Edax に任せるためのコマンドです（v4.5.5-nikque.19 から）。

- どれも、ほかのコマンドと並行してバックグラウンドのスレッドで動きます。解析の間も、`hint` や手を打つコマンドなどを受け付けます。
- book を読むだけで、変えません。book を変えるコマンド（`book store` など）を送ると、Edax は先に解析を止めます。
- 番号 `id=…` を付けて送ります。
- 答えは `@` で始まる行です。1 行ずつ、行の終わりまで一度に書きます。
  - ほかの表示（警告など）が書きかけのときは、その行の途中に入ることがあります。たとえば `WARNING: @end id=5 ms=0` のようになり、警告の残りは次の行に出ます。
- 値は、今の局面で手番の側から見た石差です。

| コマンド | 答え |
|---|---|
| `book moveinfo id=N` | 全部の合法手について、book の評価値と level、精度の段階、次の段階に足りない Leaf の数、うみがめ数、手の後に book でたどれる局面の数 |
| `book missing id=N` ［`sq=手`］ | 次の段階に足りない Leaf までの手順の一覧（`sq=` を書くと、その手の後から） |
| `book levels id=N` | 今の局面から先の局面の、level ごとの数（`scope=all` は book 全体） |
| `book bestlines id=N` | 両方が最善の手（`loss=` の損失までなら次善の手も）だけを打つ進行の数。`file=` を書くと、進行の端までの手順を書き出す |
| `book findline id=N target=局面` ［`n=`］ | 今の局面から `target` の局面までの、book の局面だけを通る手順（評価値の変わり方がいちばん小さいものから `n` 本、既定は 1 本） |
| `book reducecount id=N` ［`x=`・`y=`］ | `book reduce X Y`（[8.4](08-book-maintenance.md)）を今の局面で実行したときに残る局面と消える局面の数（book は変えない。v4.5.5-nikque.20 から） |
| `stop id=N` | 解析 N を止める（ほかの解析と、待っているコマンドは残る） |

**精度の段階**（`stage=`）

| 段階 | 意味 |
|---|---|
| 0 | 手の後の局面が book にない |
| 1 | book にあるが、両方が最善の手だけを打っても、完全読みの局面に届かない |
| 2 | 届くが、段階 3 の範囲が終わっていない |
| 3〜7 | その手の後の局面で `book deviate 1 1`〜`book deviate 5 5` を実行しても、何も広げない |

- 基準のコマンドは `base=deviate2`・`base=deviate3` に、範囲は `stages=1/1,2/2,3/3,4/4,5/5` の形で変えられます。
- 求めた精度の段階は、book が変わるまで覚えています。
- 手の後の局面の段階を求めるときは、覚えている段階からわかる所を、book をたどらずに決めます。使うのは、今の局面で覚えている段階と、その先の局面で覚えている段階です（v4.5.5-nikque.23 から。答えは同じです）。
- `book missing` は、聞いた範囲（`x=`・`y=`）で何も見つからないことが、覚えている精度の段階からわかる局面では、book をたどらずに 0 本と答えます（v4.5.5-nikque.24 から。答えは同じです）。

**うみがめ数**（`umi=黒:白`）

- その手の後で、条件を満たす全部の進行を覚えるには、最小で何通りの進行の端を覚えればよいか。これを黒と白それぞれについて数えた数です。
- 覚える側の番では 1 手を選べばよく、相手の番では全部の手に備えます。
- 別の手順から同じ局面に来たときは、その先を 1 回だけ数えます。
- 条件は、既定では両方の最善の手だけです。次の条件でも数えられます。
  - `umi=loss x=… y=…`：1 手の損失と合計の損失
  - `umi=range lo=… hi=…`：評価値の範囲
- 範囲が広いと、最小とは限らない値に `exact=0` が付きます。
  - `exact=0` が付く値は、ビルドした環境（Windows・Linux など）によらず同じです（v4.5.5-nikque.25 から）。
- 求めたうみがめ数は、book が変わるまで覚えています（v4.5.5-nikque.24 から）。
  - 手の後の局面を数えるときは、その先の 1 手目・2 手目の局面の値も覚えます。その局面が今の局面になったときは、数え直しません。
  - 覚えるのは、次の局面です。`exact=0` が付く数え方の途中の値は、覚えません。
    - `umi=best`：全部の局面
    - `umi=loss`：損失 0 で着いた局面
    - `umi=range`：手番が手の後の局面と同じ局面（`lo=-hi` のときは全部）
  - 答えは同じです。ただし、その局面だけから数えると `exact=0` が付く手が、前の局面で覚えたちょうどの値になることがあります。この値には `exact=0` が付かず、`exact=0` の付いた値より小さいか同じです。

例（テスト用の book）：

```
>book moveinfo id=1
@begin id=1 ply=0 turn=black moves=4 book=1
@move id=1 sq=d3 book=1 value=-2 level=4 lines=505 stage=1 missing=1
...
@move id=1 sq=d3 part=umi umi=1:1
...
@move id=1 sq=d3 part=count positions=1546
...
@end id=1 ms=0
```

### 各コマンドの答え

**`book moveinfo`**

- book にない手、精度の段階、うみがめ数（`part=umi`）、局面の数（`part=count`）の順に答えます。
- 大きな book の序盤では、局面の数に時間がかかります。`parts=stage,umi` のように、一部だけを求めることもできます。
- `lines=` は、book に書かれた終わっていない進行の数です。すぐに出ます。合流は進行ごとに数えます。
- 局面の数（`part=count`）と、`book levels`（`scope=all` を除く）の数は、局面ごとに、book が変わるまで覚えています（v4.5.5-nikque.24 から）。
  - 次のときは、book をたどり直しません。答えは同じです。
    - 数えた局面が今の局面になって、`book levels` を送ったとき
    - 同じ手をもう一度数えるとき

**`book bestlines`**

- 答えは `@end id=N lines=… ends=… incomplete=… incomplete_ends=… positions=… written=…` の 1 行です。
  - `lines`：進行ごとの数
  - `ends`：端の局面ごとの数
  - `incomplete`：完全読みに届かない端の数
- `file=` のファイルには、端の局面ごとに 1 行、今の局面までの手順から端までの手順を書きます。`which=incomplete` を付けると、完全読みに届かない端だけを書きます。

**`book findline`**

- `target=` は、a1〜h8 の 64 マスの後に手番（`X` か `O`）を続けた 65 文字です。マスは `X` が黒、`O` が白、`-` が空きで、`setboard` と同じです。
- 今の局面と `target`、その間の局面が、すべて book にある手順を探します。
  - `target` で石のあるマスにだけ打ちます。
  - 打てる手がなく、相手が打てるときは、パス `pa` です。
- 手順は、各手で book の評価値（黒から見た値）が変わった幅の合計が小さい順に並びます。
- 答えは、手順ごとに `@line id=N moves=f5d6c3… cost=…`、最後に `@end id=N count=… paths=… positions=…` です。
  - `paths`：手順の全部の数
  - `positions`：たどった局面の数
- `target` が book にないときは `notinbook=1`、今の局面が book にないときは `startnotinbook=1` が付き、`count=0` です。
- `n=` は 1〜100 です。

**`book reducecount`**

- 答えは `@end id=N kept=… removed=… positions=… ms=…` の 1 行です。
  - `kept`：残る局面の数
  - `removed`：消える局面の数
  - `positions`：book 全体の局面の数
- `x=`・`y=` は、`book reduce X Y` の X（0〜129、既定 2）と Y（0〜254、既定 4）です。
- 今の局面から先の評価値を、book を変えずに計算し直してから数えます（`book reduce` と同じ）。そのため、`book reduce` の後の局面の数と同じになります。
- 次のときは、`kept=` が book 全体、`removed=0` になります（`book reduce` は book を変えません）。
  - 今の局面が book にないとき：`notinbook=1` が付きます。
  - 今の局面が book の深さより 2 つ以上先のとき：`deep=1` が付きます。
- 今の局面から先の book が壊れているときは、数えずに `@error id=N damaged=1` と `@end id=N error=1` を返します（v4.5.5-nikque.21 から）。壊れているとは、すでに石のあるマスへの手や、同じ局面に戻る手のつながりがあることです。
- GUI のソフトは、これで数を見せて確かめてから、`book reduce` を送れます。
- 数えた結果は、その後の `book reduce` が使います（[8.4](08-book-maintenance.md)）。
- ほかの解析がマークのためのメモリを必要とするときは、数えた結果は捨てられます。このときの `book reduce` は、計算し直します（結果は同じです）。

### 誤り・取り消し・書き方の決まり

- 引数の誤りには、`@error id=N word=…` と `@end id=N error=1` と答えます。
- メモリが足りないときは、`@error id=N memory=1` と `@end id=N error=1` と答えます。
  - それより前に出た `@move` などの行は、その解析の一部だけです。
  - v4.5.5-nikque.26 までは、`book moveinfo` だけが違いました。メモリが足りなかった手の行を出さないまま、誤りのない `@end` を答えていました。
- 止めた解析には、`@cancel id=N` と答えます。
- 番号 `id=` が整数（int の範囲）でないときは、解析も `stop` も行わず、`@error word=id` とだけ答えます。
- コマンドの語は空白で区切ります。`"…"` の中は、空白も含めて 1 つの値です。その中の `id=` は番号ではありません。
- 名前（`id=` など）の大文字・小文字は区別しません。
- `stop id=N` が、終わった後の解析や、まだ受け付けていない解析に届いたときは、`@cancel id=N state=none` と答えます。
  - 終わった解析は、その前に `@end id=N` を答えています。
  - 受け付ける前の解析は、その後で最後まで行い、`@end id=N` を答えます。
  - v4.5.5-nikque.26 から、この答えに `export=1` が付きます（`@cancel id=N state=none export=1`）。
    - `export=1` は、`book deviate`・`deviate2`・`deviate3` の `export`（[7.3](07-book-learning.md)）を持つことを表します。
    - `export` を知らない前の版は、その語を読まずに book を広げます。
    - そのため、GUI のソフトは、起動のときに送った `stop id=N` の答えで `export=1` を確かめてから、`export` を送ってください。

### このマニュアルの作成で確かめたこと


- 小さなテスト用の book（自分で作ったもの、最大 1,914 万局面）で、答えを、別に書いた計算と照らし合わせました。
- 精度の段階は、実際に `book deviate` を実行して確かめました。
- `book reduce`・`book reducecount` は、別に書いた計算（`book deviate3` のたどり方と、評価値の計算し直し）と照らし合わせました（4 つのテスト用の book、546 通り）。照らし合わせたのは次のものです。
  - 残る局面と消える局面
  - 残った局面の中身
  - その後の `book deviate3` が最初に選ぶ Leaf（別の計算で求めたもの）
- 壊れた book 2 つと、誤った語の `book reduce` も試しました（v4.5.5-nikque.21 から）。
- GUI のソフトそのものとの接続は、試していません。

次：[13. 困ったときは](13-troubleshooting.md)
