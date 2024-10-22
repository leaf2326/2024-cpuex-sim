#include "Simulator.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    Simulator simulator;
    #ifdef DEBUG
    for (int i = 0; i < argc; ++i) {
        std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
    }
    #endif
    if(argc != 2){
         std::cerr << "Error: Wrong number of arguments. Expected: $ ./simulator <filepath>" << std::endl;
         return 1;
    }
    // バイナリファイル "program.bin" をメモリにロード
    try
    {
        simulator.loadMemoryFromBinary(argv[1]);
        simulator.runProgram();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}