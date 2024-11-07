#include "Simulator.hpp"
#include "Option.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
#ifdef DEBUG
    for (int i = 0; i < argc; ++i)
    {
        std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
    }
#endif
    std::string programFilePath;
    std::string inputFilePath = "sld/contest.sld";
    Options options;

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
                     std::cerr << "Filepath is required. Expected: $ -i <filepath>" << std::endl;
                }
            }
            else
            {
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
            }
        }
        else if (programFilePath.empty())
        {
            programFilePath = arg;
        }
        else
        {
            std::cerr << "Error: Too many arguments. Expected: $ ./simulator <filepath>" << std::endl;
            return 1;
        }
    }

    if (programFilePath.empty())
    {
        std::cerr << "Error: Filepath is required. Expected: $ ./simulator <filepath>" << std::endl;
        return 1;
    }
#ifdef DEBUG
    std::cout << "programFilepath: " << programFilePath << std::endl;
    if (options.has(options.ONLYSTDIO))
    {
        std::cout << "Option -onlystdio is enabled." << std::endl;
    }
    if (options.has(options.I))
    {
        std::cout << "Option -i is enabled." << std::endl;
        std::cout << "inputFilepath: " << programFilePath << std::endl;
    }
#endif

    Simulator simulator(options);
    try
    {
        simulator.loadInputData(inputFilePath);
        simulator.loadMemoryFromBinary(programFilePath);
        simulator.runProgram();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}