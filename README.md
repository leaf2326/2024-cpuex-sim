# 2024-cpuex-sim
2024年CPU実験6班のCPUシミュレータのリポジトリ。

## 概要
2024年CPU実験6班のISAに従うバイナリファイルを読み込み、動作をシミュレートするプログラム。アセンブリコードは [`6asm`](https://github.com/windows-server-2003/2024-cpuex-asm) を使って機械語に変換してから利用する。ISAは基本RISC-Vをベースとして改変したものであり、RISC-Vのプログラムがこのシミュレータで動くことは保証していない。

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
```bash
./simulator example/mandelbrot.bin -i example/mandelbrot.input 1> simulator.log 2> simulator.err
```

- `example/mandelbrot.bin`: 実行するRISC-Vバイナリ
- `-i example/mandelbrot.input.sld`: 入力ファイルパス（指定がない場合、`sld/contest.sld` が使用される）

実行結果:
- 標準出力にシミュレーション結果のIO命令の出力が表示される。
- 標準エラー出力にシミュレーションの情報が表示される。

---

## コマンドラインオプション
| オプション            | 説明                                                                 |
|-----------------------|----------------------------------------------------------------------|
| `-h [ --help ]`      | ヘルプを表示                |
| `-i [ --input ] <filepath>`      | 入力ファイルを指定（デフォルト: `sld/contest.sld`）                |
| `--notify`            | 実行終了時にDiscord Webhookへ通知を送信する。Webhook URLは `discordWebhook.txt` に記載 |
| `--reg <RegNum>`      | 特定のレジスタ値を出力。`<RegNum>` は 0-31（`x0-x31`）、32-63（`fp0-fp31`） |
| `--limit <maxStep>`  | 最大実行命令数を指定（デフォルト: `UINT64_MAX`）                   |
| `--memory <size>`     | DRAMサイズを指定（単位: MiB、デフォルト: 4MiB）                                      |
| `--cache`             | キャッシュメモリを有効化（実行速度は低下する）                    |
| `--icount`            | 命令メモリに存在する各命令とその実行回数を出力する                       |
| `--debug`             | 詳細なログを表示（大量の出力が発生する可能性あり）                |
| `-onlystdio`         | **(廃止)** 実行時のOutputのみを表示                     |
| `--gdb`               | GDBのような機能を有効化    |

---

## GDBモード
`-gdb` オプションを使うとデバッグモードでシミュレータを実行可能。以下のコマンドがサポートされる。

### コマンド一覧
| コマンド      | 説明                                                             |
|---------------|------------------------------------------------------------------|
| `s`           | ステップ実行 + 付近のアセンブリコードを表示                      |
| `c`           | 次の `ebreak` まで実行                                          |
| `l`           | 付近のアセンブリコードを表示                                    |
| `r`           | 直前の出力を再度表示                                            |
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

- [Boost.ProgramOptions](https://www.boost.org/)  
  A library for program option parsing and handling.  
  Licensed under the [Boost Software License 1.0](https://www.boost.org/users/license.html).
