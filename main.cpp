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
    std::string filepath;
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg[0] == '-')
        {
            if (arg == "-b")
            {
                options.set(options.B);
            }
            else if (arg == "-onlystdio")
            {
                options.set(options.ONLYSTDIO);
            }
            else
            {
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
            }
        }
        else if (filepath.empty())
        {
            filepath = arg;
        }
        else
        {
            std::cerr << "Error: Too many arguments. Expected: $ ./simulator <filepath>" << std::endl;
            return 1;
        }
    }

    if (filepath.empty())
    {
        std::cerr << "Error: Filepath is required. Expected: $ ./simulator <filepath>" << std::endl;
        return 1;
    }
    #ifdef DEBUG
    std::cout << "Filepath: " << filepath << std::endl;
    if (option_o)
    {
        std::cout << "Option -o is enabled." << std::endl;
    }
    if (option_i)
    {
        std::cout << "Option -i is enabled." << std::endl;
    }
    #endif

    Simulator simulator(options);
    try
    {
        simulator.loadMemoryFromBinary(filepath);
        simulator.runProgram();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}