# v4.5.5-nikque.16：Android追加公開

初回公開ではAndroid CIがggs.cのIPPROTO_TCP未定義で失敗した。その後、エンジンソースの修正とAndroidの公開を追加で行った。最初の失敗ログ・Windows比較値は履歴として保持する。

## 確認した事実

- 修正はsrc/ggs.cの非Windows側ヘッダーをnetinet/tcp.hからnetinet/in.hへ置き換える1行だけ。修正コミットは`7c7d51a9f66f6577b7bf8bc67e71154a31aacacb`。
- [再ビルドCI](https://github.com/Nikque/edax-reversi-AVX/actions/runs/38791358433)はAndroidを含む全7ジョブ成功。NDK27.3.13750724、armeabi-v7a/arm64-v8a、Android API21の実行ファイル2本・共有ライブラリー2本を再作成。
- ELFヘッダーはARM32（machine40）/AArch64（183）、全4本がET_DYN。両共有ライブラリーに、Windows DLLと同じ公開API105関数の定義を確認した。
- Windows v4のプリプロセス出力は、ビルド時刻文字列だけを正規化すると修正前後で同一。詳細とハッシュはAndroid修正Windowsプリプロセス比較.json。Windows配布PGO5種とDLL3種は元の検証済みファイルを維持し、計測v4と配布v4のSHA-256も不変。速度計測のやり直しはしていない。
- Linux・macOS・Windows ARM64も今回のCIで再作成したファイルを採用。完全版ZIPは14実行ファイル・9ライブラリー。
- 公開済みのv4.5.5-nikque.16タグ（df909948...）は変更しない。追加後の対応ソースは新タグ`v4.5.5-nikque.16-android`と上記修正コミットで取得できる。Windowsの比較対象は従来どおり19e99cb。追加パッチも添付する。

## できなかったこと・推測の範囲

Android実機はないため、実機での起動・探索・DLL/API実行は確認できなかった。ELF形式・リンク時API定義の検査を実機テストとは呼ばない。ヘッダー宣言の修正でコンパイルが通ったことは確認済みだが、実機の全環境で動くことまで保証する試験はしていない。

## 配布物

`edax-4.5.5-nikque.16.zip`はAndroidを含む完全版。`edax-4.5.5-nikque.16-android.zip`はAndroid用4ファイル・設定・eval.dat・空book・ライセンス・本報告。初回のwithout-android.zipと比較資料は当初の記録として残す。

各ファイルのSHA・ABI検査はAndroid追加検証.json、全23ファイルのビルド元はAndroid追加配布物由来.json。SHA256SUMSには初回公開物と追加物を含める。
