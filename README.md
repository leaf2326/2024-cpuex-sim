# 2024-cpuex-sim
2024年CPU実験6班のCPUシミュレータのリポジトリ。

## 概要
2024年CPU実験6班のISAに従うバイナリファイルを読み込み、動作をシミュレートするプログラム。アセンブリコードは [`6asm`](https://github.com/windows-server-2003/2024-cpuex-asm) を使って機械語に変換してから利用する。ISAは基本RISC-Vをベースとして改変した独自のものであり、RISC-Vのプログラムが動くことは想定していない。

## 必要な環境
- コンパイラ: `g++` (C++23対応)
- ライブラリ: `libssl-dev`

インストールコマンド:
```bash
sudo apt-get install libssl-dev
```

## ビルドと実行
1. プロジェクトをクローンする。
   ```bash
   git clone https://github.com/your-repo/2024-cpuex-sim.git
   cd 2024-cpuex-sim
   ```

2. シミュレータをビルドする。
   ```bash
   make
   ```

3. シミュレータを実行する。
   ```bash
   ./simulator example/minrt_inline400_32
   ```

### 実行例
標準的なシミュレーション実行例:
minrt(256*256)
```bash
./simulator example/minrt_latest_256_p6.sim
```
mandelbrot
```bash
./simulator example/mandelbrot.sim -i example/mandelbrot.input
```
- `example/mandelbrot.sim`: 実行するRISC-Vバイナリ
- `-i example/mandelbrot.input`: 入力ファイルパス（指定がない場合、`sld/contest.sld` が使用される）

実行結果:
- `--output`に指定されたファイル（デフォルト: `output.ppm`）にプログラムのIO命令の出力が表示される。
- 標準エラー出力にシミュレーションの情報が表示される。

---

## コマンドラインオプション
| オプション            | 説明                                                                 |
|-----------------------|----------------------------------------------------------------------|
| `-h [ --help ]`      | ヘルプを表示                |
| `-i [ --input ] <filepath>`      | 入力ファイルを指定（デフォルト: `sld/contest.sld`）                |
| `-o [ --output ] <filepath>`      | 出力ファイルを指定（デフォルト: `output.ppm`）                |
| `--notify`            | 実行終了時にDiscord Webhookへ通知を送信する。Webhook URLは `discordWebhook.txt` に記載 |
| `-r [ --reg ] <RegNum>`      | 特定のレジスタ値を出力。`<RegNum>` は 0-31（`x0-x31`）、32-63（`fp0-fp31`） |
| `-l [ --limit ] <maxStep>`  | 最大実行命令数を指定（デフォルト: `UINT64_MAX`）                   |
| `-m [ --memory ] <size>`     | DRAMサイズを指定（単位: MiB、デフォルト: 4MiB）                                      |
| `--no-cache`          | キャッシュメモリを無効化する |
| `--icount`            | 命令メモリに存在する各命令とその実行回数を出力する                       |
| `--istats`            |  実行された命令の特殊な統計を出力する。`mv`と`mvi`の回数や`lw`/`sw`のオフセット分布など                       |
| `-d [ --debug ]`             | 詳細なログを表示（大量の出力が発生する可能性あり）                |
| `--stdout`            | 標準出力への出力を有効化 |
| `-p [ --pbar ] <imageSize>`             | プログレスバーをターミナルに表示。標準出力がターミナル上の時かつP3のバイナリの時のみ動作。 例：`/simulator example/minrt_inline400_32 -p 32`            |
| `-onlystdio`         | **(廃止)** 実行時のOutputのみを表示                     |
| `-g [ --gdb ]`               | GDBのような機能を有効化    |
| `--no-pipeline`             | パイプライン処理を無効化する。この時の時間予測はv2.2core向けのものであり最新のコアには対応していない。 |
| `--l1-lines <N>`           | L1キャッシュのライン数を指定 (デフォルト: 1024) |
| `--l2-lines <N>`           | L2キャッシュの総ライン数を指定 (デフォルト: 5120) 、L2キャッシュのウェイ数の倍数であるべき|
| `--l2-ways <N>`            | L2キャッシュのウェイ数を指定 (デフォルト: 5) |
| `--cache-line-size <N>`    | キャッシュラインのサイズをバイト単位で指定 (デフォルト: 64) |
| `--superscalar <mode>`    |  Set superscalar mode: `none` (disable, default), `restricted` (no b*/add/addi), or `full` |

---

## GDBモード
`--gdb` オプションを使うとデバッグモードでシミュレータを実行可能。以下のコマンドがサポートされる。

### コマンド一覧
| コマンド      | 説明                                                             |
|---------------|------------------------------------------------------------------|
| `s`           | ステップ実行 + 付近のアセンブリコードを表示                      |
| `c`           | 次の `ebreak` まで実行                                          |
| `l`           | 付近のアセンブリコードを表示                                    |
| `r`           | 直前の出力を再度表示                                            |
| `p` or `pipeline` | パイプラインの現在の状態を表示                                |
| `quit` or `q` | デバッグモードを終了                                            |
| `info` or `i` | 引数なしでレジスタ群をすべて表示                                 |
| `[空文字]`    | 直前のコマンドを再実行                                          |

### `info` コマンドの引数
| コマンド               | 説明                                                    |
|------------------------|---------------------------------------------------------|
| `reg` or `r` (+ `<Num>`)| 全レジスタを列挙。`<Num>` を指定するとその番号の値を表示。 |
| `fpreg` or `f` (+ `<Num>`)| 全FPレジスタを列挙。`<Num>` を指定するとその番号の値を表示。 |
| `pc` or `p`            | プログラムカウンタ（`PC`）を表示                         |

### コマンドの繰り返し
例:  
`100 s` のように `[数字] [コマンド]` 形式で指定すると、数字分だけコマンドを繰り返す。

---

## その他のMakefileターゲット(開発用)
| コマンド          | 説明                                                                            |
|-------------------|---------------------------------------------------------------------------------|
| `make debug`      |  **(非推奨)** デバッグ用ログを多めに表示。シミュレータ開発用なのでユーザーは使用しない。         |
| `make testFPU`    | `testFPU.cpp` を実行し、FPUのテストを行う。                                     |

---

## 注意点
- シミュレーションは命令実行数である`step`数が`maxStep`（デフォルト: `UINT64_MAX`）に達するか、`ebreak` で停止。
- 分岐予測には2^7エントリのBimodal Predictorを使用。
- Discord通知を利用する場合、Webhook URLを `discordWebhook.txt` に記載。

---

## 使用しているライブラリ
このプロジェクトでは以下のオープンソースライブラリを使用しています。

- [cpp-httplib](https://github.com/yhirose/cpp-httplib)  
  Lightweight C++ HTTP/HTTPS library.  
  Licensed under the [MIT License](https://github.com/yhirose/cpp-httplib/blob/master/LICENSE).

- [cxxopts](https://github.com/jarro2783/cxxopts)  
  Lightweight C++ option parser library. 
  Licensed under the [MIT License](https://github.com/jarro2783/cxxopts/blob/master/LICENSE).

- [pbar](https://github.com/estshorter/pbar)  
  C++ progress bar library inspired by tqdm(https://github.com/tqdm/tqdm).  
  Licensed under the [Apache-2.0 license](https://github.com/estshorter/pbar/blob/master/LICENSE).
