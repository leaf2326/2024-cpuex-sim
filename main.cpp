#include "Simulator.hpp"
#include "Option.hpp"
#include <iostream>
#include <chrono>
int main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    
    // optionの処理
#ifdef DEBUG
    for (int i = 0; i < argc; ++i)
    {
        std::cerr << "argv[" << i << "]: " << argv[i] << std::endl;
    }
#endif
    std::string programFilePath;
    std::string inputFilePath = "sld/contest.sld";
    int outputRegNum = -1;
    Options options;
    uint64_t max_clk = 100000;

    std::streambuf *oldBuffer = nullptr;

    try
    {

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg[0] == '-')
            {
                if (arg == "-onlystdio")
                {
                    options.on(options.ONLYSTDIO);
                }
                else if (arg == "-gdb")
                {
                    options.on(options.GDB);
                }
                else if (arg == "-cache")
                {
                    options.on(options.CACHE);
                }
                else if (arg == "-i")
                {
                    if (i + 1 < argc)
                    {
                        inputFilePath = argv[i + 1];
                        options.on(options.I);
                        ++i;
                    }
                    else
                    {
                        throw std::runtime_error("Filepath is required. Expected: $ -i <filepath>");
                    }
                }
                else if (arg == "-limit")
                {
                    if (i + 1 < argc)
                    {
                        max_clk = std::stoi(argv[i + 1]);
                        options.on(options.LIMIT);
                        ++i;
                    }
                    else
                    {
                        throw std::runtime_error("maxClock is required. Expected: $ -limit <maxClock>");
                    }
                }
                else if (arg == "-reg")
                {
                    if (i + 1 < argc)
                    {
                        outputRegNum = std::stoi(argv[i + 1]);
                        options.on(options.REG);
                        ++i;
                    }
                    else
                    {
                        throw std::runtime_error("the number of output register is required. Expected: $ -reg <outputRegNum>");
                    }
                }
                else
                {
                    throw std::runtime_error("Unknown option " + arg);
                }
            }
            else if (programFilePath.empty())
            {
                programFilePath = arg;
            }
            else
            {
                throw std::invalid_argument("Too many arguments. Expected: $ ./simulator <filepath>");
            }
        }

        if (programFilePath.empty())
        {
            throw std::runtime_error("Filepath is required. Expected: $ ./simulator <filepath>");
        }
#ifdef DEBUG
        std::cerr << "programFilepath: " << programFilePath << std::endl;
        std::cerr << "Option -onlystdio is enabled." << std::endl;
        if (options.has(options.I))
        {
            std::cerr << "Option -i is enabled." << std::endl;
            std::cerr << "inputFilepath: " << programFilePath << std::endl;
        }
#endif
    }
    catch (const std::exception &e)
    {
        if (options.has(options.ONLYSTDIO) && oldBuffer != nullptr)
        {
            std::cerr.rdbuf(oldBuffer);
        }
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    Simulator simulator(options, max_clk);
    // 開始日時を取得する
    auto start = std::chrono::system_clock::now();
    try
    {
        if (options.has(options.ONLYSTDIO))
        {
            oldBuffer = std::cerr.rdbuf(nullptr);
        }
        // プログラムシミュレート
        simulator.loadInputData(inputFilePath);
        simulator.loadMemoryFromBinary(programFilePath);
    }
    catch (const std::exception &e)
    {
        if (options.has(options.ONLYSTDIO) && oldBuffer != nullptr)
        {
            std::cerr.rdbuf(oldBuffer);
        }
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    try
    {
        simulator.runProgram(outputRegNum);
        auto end = std::chrono::system_clock::now();

        if (options.has(options.ONLYSTDIO) && oldBuffer != nullptr)
        {
            std::cerr.rdbuf(oldBuffer);
            std::cerr << "CLK : " << simulator.getCLK() << std::endl;
            simulator.printProgram(true);
            simulator.printRegisters();
            simulator.printLog();
        }
        std::chrono::duration<double, std::milli> elapsed = end - start;

        // end - start を秒単位で計算する
        std::chrono::duration<double> elapsed2 = end - start;
        std::cerr << "Execution time: " << elapsed.count() << "ms" << std::endl;
        std::cerr << "Instruction Per Second: " << simulator.getCLK() / elapsed.count() * 1000.0 << std::endl;
    }
    catch (const std::exception &e)
    {
        if (options.has(options.ONLYSTDIO) && oldBuffer != nullptr)
        {
            std::cerr.rdbuf(oldBuffer);
        }
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "__DEBUG INFO__ " << std::endl;
        std::cerr << "CLK : " << simulator.getCLK() << std::endl;
        simulator.printProgram(true);
        simulator.printRegisters();
        simulator.printLog();
        return 1;
    }

    return 0;
}