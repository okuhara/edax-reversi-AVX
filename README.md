# Edax 4.5.5 nikque

A maintained fork of [Edax 4.5.5 (okuhara/edax-reversi-AVX)](https://github.com/okuhara/edax-reversi-AVX), the Othello engine. It fixes bugs in book learning and book handling, adds book analysis and maintenance commands, and makes book learning faster on many-core PCs. Books made with upstream Edax 4.5.5 can be used as they are.

[日本語](#日本語) · [Download](https://github.com/Nikque/edax-reversi-AVX/releases/latest) · [Manual](manual/en/README.md) · [Release notes](RELEASE-NOTES.md) · [Tests and measurements](docs/validation/)

## Download and run

1. Download the ZIP from the [latest release](https://github.com/Nikque/edax-reversi-AVX/releases/latest) and unpack it.
2. Open the `bin` folder and start the executable for your system from **inside that folder** (Edax looks for `data/eval.dat` in the current folder).

| System | Executable in `bin/` |
|---|---|
| Windows x64 | `wEdax-x86-64.exe` (any CPU), `-v3` (x86-64-v3), `-v4` (x86-64-v4) |
| Windows x86 / ARM64 | `wEdax-x86.exe`, `wEdax-x86-sse.exe` / `wEdax-arm64.exe` |
| Linux x64 / x86 | `lEdax-x86-64` (any CPU), `-v3` (x86-64-v3), `-v4` (x86-64-v4) / `lEdax-x86` |
| macOS | `mEdax-arm64` (Apple silicon), `mEdax-x64-modern` (Intel) |
| Android | `aEdax-arm64-v8a`, `aEdax-armeabi-v7a` |

`-v3` needs a CPU of the x86-64-v3 level (AVX2, BMI1, BMI2, FMA, LZCNT, MOVBE: Intel Haswell or AMD Zen and later), `-v4` one of the x86-64-v4 level (AVX-512 F, BW, CD, DQ and VL as well: Intel Skylake-SP or Ice Lake, AMD Zen 4 and later). If you are not sure which CPU features you have, use the version without `-v3`/`-v4`. The ZIP also contains the libedax libraries (`.dll`, `.so`, `.dylib`) for GUIs and tools such as [nikque GUI](https://github.com/Nikque/nikque-gui) and [edax_runner](https://github.com/Nikque/edax_runner).

Getting started: [Manual, chapter 2](manual/en/02-quick-start.md).

## Versions

There are three lines. The nikque and eval2 lines share the same commands, settings and book format. Heath shares most of them; the differences follow the table.

| Line | What it is | Where |
|---|---|---|
| **v4.5.5-nikque.N** | The main line: bug fixes and new book commands, with the upstream evaluation (`eval.dat`) | Branch `edax-4.5.5-fixes` (default). Latest release |
| **v4.5.5-eval2.N** | nikque + a new evaluation function (adds mobility; about +17 to +22 Elo against upstream `eval.dat`). Scores differ from the nikque line | Branch `eval2`, releases `v4.5.5-eval2.N` |
| **Heath** (formerly newedax) | The eval2 line ported to C++23; a new 64-slot evaluation since v4.5.5-newedax.13 | [Nikque/Heath](https://github.com/Nikque/Heath) |

Each eval2 and Heath release includes all changes of the nikque release it names (for example, eval2.10 = nikque.22 + eval2).

What differs in Heath:

- Settings only in Heath: `book-format` (the form of the saved books: `1`, `2` or `auto`) and `keep-hash`.
- Settings only in the C lines: `probcut-model`, `pc-slope`, `pc-dev` and `pc-mob`. Heath has the probcut model `refit` and the values of `pc-slope 1.3,12`, `pc-dev 2,0.04,0.75` and `pc-mob 2.219,-0.184,-0.350` built in (since v4.5.5-heath.20).
- The default of `lazy`: `0` in the C lines, `auto` in Heath.
- Book files: Heath reads and writes form 1 (the books of the C lines) and form 2 (`NEDXBOOK`); the C lines read and write form 1 only.
- `eval.dat`: each line has its own. Heath needs an `eval.dat` of Heath (the one of its release ZIP): it reads that of the C lines with a warning, and its evaluation is then wrong.

## Main changes from upstream

- **Book learning fixed**: `book deviate` could crash or lose positions (a dangling pointer); `book merge`, `book fix` and book saving were made safe (temporary file, damaged records rejected, a book file held by another program at the start never replaced).
- **Book commands go on from a saved book** (v4.5.5-nikque.29): `book correct`, `book leaf recalculate` and `book learn` continue from a book saved before they ended (marks removed when they end); `book deepen` works.
- **Exact search fixed** (v4.5.5-nikque.28): at level 60 from 17 empties on, the exact score (and move) could rarely be wrong (a score cut by probcut after a pass was stored as exact); the same error is in Edax 4.5.5 and 4.5.6 of Toshihiko Okuhara.
- **New learning commands**: `book deviate2` / `book deviate3` (limits on loss per move and in total).
- **Faster learning on many cores**: several book positions are expanded at the same time (`book-expand-tasks`, `book-store-tasks`); with 32 threads at level 18, up to about 4.7 times faster.
- **Less memory**: about 50 bytes per book position (58 before).
- **Book analysis and maintenance**: `book moveinfo`, `book missing`, `book bestlines`, `book reduce`, the `export` of the `book deviate` family (the lines they walk, written without expanding the book) and others (used by nikque GUI).
- **17 other fixes** in file I/O, time control, PGN/GGF and protocols (XBoard, GTP, NBoard).

The complete list is in the [release notes](RELEASE-NOTES.md).

## Build

- **Windows** (Visual Studio 2022, Developer Command Prompt): in `src`, run `nmake -f NMakefile vc-x64-v4` (other targets are in `src/NMakefile`; `build-win-v4.cmd` builds the v4 executable). For a local PGO build, use the PGO targets (`vc-pgo-x64-v4`, `vc-pgo-x64-v3`, `vc-pgo-x64`, and `vc-pgo-x86-sse`, `vc-pgo-x86` from an x86 prompt).
- **Libraries**: Windows `nmake -f NMakefile vc-lib-x64` (`-v3`, `-v4`); Linux and others `make libbuild ARCH=<x86-64|x86-64-v3|x86-64-v4> COMP=gcc OS=linux`. See [manual 12.2](manual/en/12-integration.md).
- **Other platforms**: the [release-binaries workflow](.github/workflows/release-binaries.yaml) builds Linux, macOS and Android; `package-release.py` assembles the ZIP.
- The 32-bit Linux build (`lEdax-x86`) needs an i486 or later CPU. The upstream 32-bit macOS `mEdax-x86` is not included: current Xcode SDKs cannot link a corrected i386 binary.

## License

GPL-3.0 ([LICENSE](LICENSE)), as upstream. `bin/data/eval.dat` and `bin/data/book.dat` are taken unchanged from the upstream v4.5.5 release. If you redistribute the executables, keep the source and the license available.

This fork was developed with ChatGPT-6 Astra / Sol and Claude Opus 5.5.

---

## 日本語

オセロのエンジン [Edax 4.5.5（okuhara/edax-reversi-AVX）](https://github.com/okuhara/edax-reversi-AVX) の修正版です。book の学習と扱いの不具合を直し、book の解析・整理のコマンドを足し、多コアの PC で book の学習を速くしました。元の Edax 4.5.5 で作った book をそのまま使えます。

[ダウンロード](https://github.com/Nikque/edax-reversi-AVX/releases/latest) · [マニュアル](manual/ja/README.md) · [修正一覧](RELEASE-NOTES.ja.md) · [テストと実測の記録](docs/validation/)

### 使い始め方

1. [最新の Release](https://github.com/Nikque/edax-reversi-AVX/releases/latest) の ZIP をダウンロードして展開します。
2. `bin` フォルダーを開き、**そのフォルダーの中から**実行ファイルを起動します（Edax は今いるフォルダーの `data/eval.dat` を読みます）。

Windows なら、まず `wEdax-x86-64.exe` を使ってください。CPU が対応していれば `-v3`・`-v4` が速くなります。`-v3` は x86-64-v3 の水準の CPU（AVX2・BMI1・BMI2・FMA・LZCNT・MOVBE。Intel は Haswell、AMD は Zen 以降）、`-v4` は x86-64-v4 の水準の CPU（さらに AVX-512 の F・BW・CD・DQ・VL。Intel は Skylake-SP・Ice Lake、AMD は Zen 4 以降）が要ります。ほかの OS の実行ファイルは上の英語の表のとおりです。

詳しくは [マニュアル 2 章「起動と最初の対局」](manual/ja/02-quick-start.md) を見てください。

### 3 つの系列

nikque と eval2 は、コマンド・設定・book の形式が同じです。Heath もほとんど同じですが、違いを表の後に書きます。

| 系列 | 内容 | 置き場所 |
|---|---|---|
| **v4.5.5-nikque.N** | 本線。不具合の修正と book のコマンドの追加。評価関数（`eval.dat`）は元のまま | ブランチ `edax-4.5.5-fixes`（既定）、最新の Release |
| **v4.5.5-eval2.N** | nikque に新しい評価関数を足したもの（着手可能数を追加。元の `eval.dat` に対して約 +17〜22 Elo）。評価値は nikque と異なる | ブランチ `eval2` |
| **Heath**（旧名 newedax） | eval2 を C++23 に移したもの。v4.5.5-newedax.13 から 64 枠の新しい評価関数 | [Nikque/Heath](https://github.com/Nikque/Heath) |

eval2・Heath の各版は、対応する nikque の版の変更をすべて含みます（例：eval2.10 = nikque.22 + eval2）。

Heath で違うこと：

- Heath だけにある設定：`book-format`（保存する book の形式。`1`・`2`・`auto`）と `keep-hash`。
- C 版の系列だけにある設定：`probcut-model`・`pc-slope`・`pc-dev`・`pc-mob`。Heath は v4.5.5-heath.20 から、probcut の型 `refit` と、`pc-slope 1.3,12`・`pc-dev 2,0.04,0.75`・`pc-mob 2.219,-0.184,-0.350` の値を作り付けで使います。
- `lazy` の既定：C 版の系列は `0`、Heath は `auto`。
- book のファイル：Heath は形式 1（C 版の系列の book）と形式 2（`NEDXBOOK`）を読み書きします。C 版の系列は形式 1 だけです。
- `eval.dat`：系列ごとに別のものです。Heath には Heath の `eval.dat`（その Release の ZIP のもの）が要ります。C 版の系列の `eval.dat` は警告を出して読みますが、評価が正しくなりません。

### 元の Edax からの主な変更

- **book の学習の不具合を修正**：`book deviate` が解放済みのメモリを読み、学習が止まったり局面が抜けたりすることがありました。`book merge`・`book fix`・book の保存も安全にしました（一時ファイル経由、壊れたレコードを拒否、起動のときにほかのプログラムが開いていた book は置き換えない）。
- **book のコマンドを途中から続けられるように**（v4.5.5-nikque.29）：`book correct`・`book leaf recalculate`・`book learn` が、途中で保存された book から続けられます（終わると印は外れます）。`book deepen` が働くようになりました。
- **完全読みの不具合を修正**（v4.5.5-nikque.28）：空き 17 マス以上の level 60 で、まれに評価値（と手）を誤っていました（パスの後に probcut で打ち切った値を正確な値として残していた）。同じ誤りは奥原俊彦氏の Edax 4.5.5・4.5.6 にもあります。
- **学習のコマンドを追加**：`book deviate2`・`book deviate3`（1 手ごと・合計の損失で範囲を指定）。
- **多コアでの学習を高速化**：複数の局面を同時に展開します（`book-expand-tasks`、`book-store-tasks`）。32 スレッド・level 18 で最大約 4.7 倍。
- **メモリの削減**：book の 1 局面あたり約 50 バイト（以前は約 58 バイト）。
- **book の解析と整理**：`book moveinfo`・`book missing`・`book bestlines`・`book reduce`、`book deviate` の仲間の `export`（たどる進行を、book を広げずに書き出す）など（nikque GUI が使います）。
- **そのほか 17 件の修正**：ファイルの入出力、持ち時間、PGN・GGF、通信（XBoard・GTP・NBoard）。

すべての変更は [修正一覧](RELEASE-NOTES.ja.md) にあります。

### ライセンス

元と同じ GPL-3.0（[LICENSE](LICENSE)）です。`bin/data/eval.dat` と `bin/data/book.dat` は元の v4.5.5 の配布物から変えずに取り出したものです。実行ファイルを再配布するときは、ソースとライセンスを入手できるようにしてください。

この fork の作業には ChatGPT-6 Astra・Sol と Claude Opus 5.5 を使いました。

---

## Upstream README (okuhara/edax-reversi-AVX)

The text below is the original README of the upstream project, kept unchanged.

Automatically exported from code.google.com/p/okuharaandroid-edax-reversi

Edax is a strong othello program. Its main features are:

    fast bitboard based & multithreaded engine.
    accurate midgame-evaluation function.
    opening book learning capability.
    text based rich interface.
    multi-protocol support to connect to graphical interfaces or play on Internet (GGS).
    multi-OS support to run under MS-Windows, Linux and Mac OS X.

This is SSE/AVX optimized version of Edax 4.4.0. Functionally equivalent to the parent project, provided no bugs are introduced.

Thanks to AVX2, x64-modern build solves fforum-40-59.obf 60% faster than official edax-4.4 on Haswell, and runs level 30 autoplay 80% faster.

See http://www.amy.hi-ho.ne.jp/okuhara/bitboard.htm and http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm for optimization details in Japanese.

## 1. Mobility (board_sse.c, board_mmx.c)

### 1.1 new SSE2 version of get_moves
Diagonals are SIMD'd using vertical mirroring by bswap.

    Athlon -get_moves_sse
    problem\fforum-20-39.obf: 111349635 nodes in 0:07.998 (13922185 nodes/s).
    mobility: 81.10 < 81.28 +/- 0.17 < 82.03
    Athlon +get_moves_sse
    problem\fforum-20-39.obf: 111349635 nodes in 0:07.889 (14114544 nodes/s).
    mobility: 71.08 < 71.72 +/- 0.34 < 73.53
    Core2 -get_moves_sse
    problem/fforum-20-39.obf: 111349635 nodes in 0:10.180 (10938078 nodes/s).
    mobility: 78.06 < 78.18 +/- 0.08 < 78.41
    Core2 +get_moves_sse
    problem/fforum-20-39.obf: 111349635 nodes in 0:09.978 (11159514 nodes/s).
    mobility: 60.84 < 61.19 +/- 0.13 < 61.47

### 1.2 can_move
Now calls SIMD'd get_moves for x86/x64 build.

## 2. Stability (board.c, board_sse.c, board_mmx.c)

### 2.1 get_full_lines_h, get_full_lines_v
get_full_lines for horizontal and vertical are simplified. The latter is compiled into rotation instrunction.

### 2.2 rearranged loop
The last while loop is rearranged not to call bit_count in case stable == 0.

### 2.3 new SSE2 version with bswap and pcmpeqb
    Athlon -get_stability_sse
    stability: 90.10 < 90.28 +/- 0.24 < 91.20
    Athlon +get_stability_sse
    stability: 81.59 < 81.93 +/- 0.73 < 86.25
    Core2 -get_stability_sse
    stability: 79.24 < 79.39 +/- 0.15 < 79.93
    Core2 +get_stability_sse
    stability: 71.80 < 71.85 +/- 0.06 < 72.07

### 2.4 get_corner_stability
Kindergarten version eliminates bit_count call.

### 2.5 find_edge_stable
Loop optimization and flip using carry propagation. One time execution but affect total solving time.

## 3. eval.c (4.4.5)
Eval feature calculation using SSE2 / AVX2 (now in eval_sse.c) improves midgame by 15-30% and endgame by 8-12%.
Restoring eval from backup instead of rewinding.
eval_open (one time execution) is also optimized.

## 4. hash.c
I think hash->data.move[0] on line 677 should be hash->data.move[1].

## 5. board_symetry, board_unique (board.c, board_sse.c)
SSE optimization and mirroring reduction. (Not used in solving game)

## 6. endgame_sse.c (4.4.7)
Keep more variables in SSE registers.  SSE optimized count_last_flip.  Parity sort by shuffle.

## 7. board_get_hash_code (4.5.0)
Changed to use CRC32c.  This enables hardware acceleration on modern build.

## 8. AVX2 versions (x64-modern build only)
In many cases AVX2 version is simplest, thanks to variable shift instructions (although they are 3 micro-op instructions).

Benchmarks are on Core i5-4260U (Haswell) 1.4GHz (TB 2.7GHz) single thread.

    4.4.0 original x64-modern clang
    problem/fforum-20-39.obf: 111349635 nodes in 0:05.726 (19446321 nodes/s).
    +optimizations 1-5 above, no-avx2
    problem/fforum-20-39.obf: 111349635 nodes in 0:05.342 (20844185 nodes/s).
    +get_moves (board_sse.c)
    problem/fforum-20-39.obf: 111349635 nodes in 0:05.142 (21654927 nodes/s).
    +flip_avx.c
    problem/fforum-20-39.obf: 111349635 nodes in 0:04.946 (22513068 nodes/s).
    +count_last_flip_sse.c
    problem/fforum-20-39.obf: 111349635 nodes in 0:04.906 (22696624 nodes/s).

## 9. makefile
gcc-old, x86 build should be -m32, not -m64. Some flags and defines added for optimization.
