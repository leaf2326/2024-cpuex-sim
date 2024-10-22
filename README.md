# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルへのパスを与えるとそのバイナリファイルを実行する

アセンブリファイルを`6asm`で機械語にしてから用いてください

6asmのリンク: https://github.com/windows-server-2003/2024-cpuex-asm

使い方は、`$ ./simulator <filepath>`
実行例：
```
make
./simulator program.bin > simulator.log
```
`make debug`をすると、シミュレーターのデバッグ用にログが多めに流れる。

`CLK`が`100000`に達するか`ebreak`が呼ばれると停止する

`Simulator.cpp`の`#define MAXCLK 100000`を変更すれば`100000`から変えられる
