# 2024-cpuex-sim
CPU実験6班のシミュレータのリポジトリ

## 使い方
実行ファイルをprogram.binという名前でmain.cppと同じフォルダに入れるとそのバイナリファイルを実行する
```
make
./simulator > simulator.log
```
CLKが100000に達するかebreakが呼ばれると停止する
Simulator.cppの#define MAXCLKを変更すれば100000から変えられる