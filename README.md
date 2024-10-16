# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルを`program.bin`という名前で`main.cpp`と同じフォルダに入れるとそのバイナリファイルを実行する

```
make
./simulator > simulator.log
```
`make debug`をすると、シミュレーターのデバッグ用にログが多めに流れる。

`CLK`が`100000`に達するか`ebreak`が呼ばれると停止する

`Simulator.cpp`の`#define MAXCLK 100000`を変更すれば`100000`から変えられる
