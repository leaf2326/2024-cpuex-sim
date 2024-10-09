#include "Simulator.hpp"
#include <iostream>

int main()
{
    Simulator simulator;

    // バイナリファイル "program.bin" をメモリにロード
    try
    {
        simulator.loadMemoryFromBinary("program.bin");
        simulator.runProgram();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
