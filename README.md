# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルへのパスを与えるとそのバイナリファイルを実行する

アセンブリファイルを`6asm`で機械語にしてから用いてください

6asmのリンク: https://github.com/windows-server-2003/2024-cpuex-asm

使い方は、`make`して`$ ./simulator <filepath>`

実行例：
```
make
./simulator program.bin > simulator.log
```

出力のみ表示するコマンドも追加

```
./simulator program.bin -onlystdio
```

何も指定せずに実行すると、`sld/contest.sld`が入力ファイルとして読み込まれる。

入力ファイルのパスを指定したい場合、以下のように実行する。

```
./simulator program.bin -i <filepath>
```

`make debug`をすると、シミュレーターのデバッグ用にログが多めに流れるが、普通は使わない。

`make testFPU`をすると、`main.cpp`ではなく`testFPU.cpp`が実行される。FPUのテストはこっちでやる予定。

`CLK`が`100000`に達するか`ebreak`が呼ばれると停止する

`Simulator.cpp`の`#define MAXCLK 100000`を変更すれば`100000`から変えられる
git commit -m "Memory outputに対応, iMemoryとdMemoryの分割に対応, 実行時オプション-onlystdioに対応"