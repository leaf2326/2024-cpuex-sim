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
./simulator example.bin 1> simulator.log 2> simulator.err
```

実行結果の内シミュレートされたio命令のOutputが標準出力に、デバッグの出力が標準エラー出力に出力される。

何も指定せずに実行すると、`sld/contest.sld`が入力ファイルとして読み込まれる。

入力ファイルのパスを指定したい場合、以下のように実行する。

```
./simulator example.bin -i <filepath>
```

| オプション | 説明 |
| --- | --- |
| `-onlystdio` | 実行時のOutputのみ表示する |
| `-i <filepath>` | 入力ファイルのパスを`<filepath>`に指定 |
| `-gdb` | ステップ実行とかできる。`-onlystdio`との併用は不可 |
| `-reg <RegNum>` | 特定のレジスタの値を標準出力に出力。RegNumは0-31がx0-x31、32-63がfp0-fp31に対応 |
| `-limit <maxClock>` | 実行するクロックの最大値を`maxClock`に変更する。型はuint64_t |
| `-memory <dMemorySize>` | DRAMのサイズを`dMemorySize(MiB)`に変更する。|

`gdb`モードの時のコマンド一覧
| コマンド | 説明 |
| --- | --- |
| `s` | ステップ実行 + 付近のアセンブリコードを表示 |
| `c` | `ebreak`まで飛ぶ |
| `l` | 付近のアセンブリコードを表示する |
| `r` | 直前の出力を再度出力する |
| `quit` or `q` | 終了する |
| `info` or `i` | 引数なしで用いるとレジスタ群をすべて表示する |
| `空文字(改行)` | 直前のコマンドを再度実行する |

`info`の引数一覧
| コマンド | 説明 |
| --- | --- |
| `reg` or `r`(+ `<Num>`)| レジスタを列挙する。さらに後ろに引数`<Num>`をつけると`<Num>`番レジスタを表示 |
| `fpreg` or `f`(+ `<Num>`)| FPレジスタを列挙する。さらに後ろに引数`<Num>`をつけると`<Num>`番FPレジスタを表示 |
| `pc` or `p` | pcを表示 |

`100 s`のように数字+コマンドで、数字分だけコマンドを実行する

`make debug`をすると、シミュレーターのデバッグ用にログが多めに流れるが、普通は使わない。

`make testFPU`をすると、`main.cpp`ではなく`testFPU.cpp`が実行される。FPUのテストはこっちでやる予定。

`CLK`が`maxClock(デフォルトは100000)`に達するか`ebreak`が呼ばれると停止する
