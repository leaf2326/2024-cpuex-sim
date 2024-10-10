# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルを`program.bin`という名前で`main.cpp`と同じフォルダに入れるとそのバイナリファイルを実行する
`fib.s`は実行には必要ないが`program.bin`の元のプログラムなので一応入れておく
```
make
./simulator > simulator.log
```
`CLK`が`100000`に達するか`ebreak`が呼ばれると停止する
`Simulator.cpp`の`#define MAXCLK 100000`を変更すれば`100000`から変えられる
