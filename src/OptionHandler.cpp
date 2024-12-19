#include "OptionHandler.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>

OptionHandler::OptionHandler()
    : outputRegNum(-1),
      cacheNumWay(0),
      enableCache(false),
      enableICount(false),
      enableDebug(false),
      enableGDB(false),
      options("simulator")
{
    
    options.add_options()
        ("h,help", "Show help message")
        ("FILE", "Program file path", cxxopts::value<std::string>(programFilePath))
        ("i,input", "Input file path", cxxopts::value<std::string>(inputFilePath)->default_value("sld/contest.sld"))
        ("n,notify", "Send notification to Discord Webhook after execution (Webhook URL in 'discordWebhook.txt')",cxxopts::value<bool>(enableNotify))
        ("r,reg", "Specify register to output (0-31: x0-x31, 32-63: fp0-fp31)", cxxopts::value<int>(outputRegNum))
        ("l,limit", "Set max instruction count", cxxopts::value<uint64_t>(maxStep)->default_value(std::to_string(DEFAULT_MAX_STEP)))
        ("m,memory", "Set DRAM size in MiB", cxxopts::value<uint64_t>(memorySize)->default_value(std::to_string(DEFAULT_MEMORY_SIZE/1024/1024)))
        ("c,cache", "Enable cache memory with configurable associativity. Specify the number of ways for set-associative cache (1 for direct-mapped).", cxxopts::value<int>(cacheNumWay))
        ("icount", "Output each instruction's count in memory", cxxopts::value<bool>(enableICount))
        ("istats", "Outputs statistics about executed instructions, focusing on `mv`, `mvi`, and `lw`/`sw` offset distributions.", cxxopts::value<bool>(enableIStats))
        ("d,debug", "Enable verbose logging (intended for short code execution))", cxxopts::value<bool>(enableDebug))
        ("g,gdb", "Enable GDB-like debugging", cxxopts::value<bool>(enableGDB));
        
    options.parse_positional({"FILE"});
}

void OptionHandler::parse(int argc, char *argv[])
{
    try
    {
        auto const result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cerr << options.help({}) << std::endl;
            exit(0);
        }

        memorySize *= 1024 * 1024;

        if (cacheNumWay > 0)
        {
            std::cerr << "The cache have " << cacheNumWay << " way." << (cacheNumWay == 1 ? "(Direct Mapped)" : "") << std::endl;
            enableCache = true;
        }

        if (enableDebug)
        {
            std::cerr << "Debug mode enabled: Verbose logging will be displayed.\n"
                      << "Note: This mode is intended for short code execution due to high output volume.\n";
        }
    }
    catch (cxxopts::OptionException &e)
    {
        std::cerr << options.help({}) << std::endl;
        std::cerr << "Error" << e.what() << std::endl;
        exit(1);
    }
}
