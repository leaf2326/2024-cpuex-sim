# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルへのパスを与えるとそのバイナリファイルを実行するシミュレータ。

アセンブリファイルを`6asm`で機械語にしてから使用する。

6asmのリンク: https://github.com/windows-server-2003/2024-cpuex-asm

使い方は、`make`して`$ ./simulator <filepath>`

実行例：
```
make
./simulator program.bin 1> simulator.log 2> simulator.err
```

実行結果の内シミュレートされたio命令のOutputが標準出力に、デバッグの出力が標準エラー出力に出力される。

何も指定せずに実行すると、`sld/contest.sld`が入力ファイルとして読み込まれる。

入力ファイルのパスを指定したい場合、以下のように実行する。

```
./simulator program.bin -i <filepath>
```

| オプション | 説明 |
| --- | --- |
| `-onlystdio` | (deprecated)実行時のOutputのみ表示する |
| `-i <filepath>` | 入力ファイルのパスを`<filepath>`に指定 |
| `-gdb` | ステップ実行とかできる。`-onlystdio`との併用は不可 |
| `-reg <RegNum>` | 特定のレジスタの値を標準出力に出力。RegNumは0-31がx0-x31、32-63がfp0-fp31に対応。 |
| `-limit <maxClock>` | 実行するクロックの最大値を`maxClock`に変更する |

`gdb`モードの時のコマンド一覧
| コマンド | 説明 |
| --- | --- |
| `s` | ステップ実行 + 付近のアセンブリコードを表示 |
| `c` | `ebreak`まで飛ぶ |
| `l` | アセンブリコードを全て表示する |
| `quit` | 終了する |


`make debug`をすると、シミュレーターのデバッグ用にログが多めに流れるが、普通は使わない。

`make testFPU`をすると、`main.cpp`ではなく`testFPU.cpp`が実行される。FPUのテストはこっちでやる予定。

`CLK`が`maxClock(デフォルトは100000)`に達するか`ebreak`が呼ばれると停止する
