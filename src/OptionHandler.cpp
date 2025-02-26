#include "OptionHandler.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>

OptionHandler::OptionHandler()
    : outputRegNum(-1),
      cacheNumWay(1),
      enableCache(true),
      enableICount(false),
      enableDebug(false),
      enableStdout(false),
      imageSize(0),
      enableGDB(false),
      enableICache(false),
      options("simulator")
{
    
    options.add_options()
        ("h,help", "Show help message")
        ("FILE", "Program file path", cxxopts::value<std::string>(programFilePath))
        ("i,input", "Input file path", cxxopts::value<std::string>(inputFilePath)->default_value("sld/contest.sld"))
        ("o,output", "Output file path", cxxopts::value<std::string>(outputFilePath)->default_value("output.ppm"))
        ("n,notify", "Send notification to Discord Webhook after execution (Webhook URL in 'discordWebhook.txt')",cxxopts::value<bool>(enableNotify))
        ("r,reg", "Specify register to output (0-31: x0-x31, 32-63: fp0-fp31)", cxxopts::value<int>(outputRegNum))
        ("l,limit", "Set max instruction count", cxxopts::value<uint64_t>(maxStep)->default_value(std::to_string(DEFAULT_MAX_STEP)))
        ("m,memory", "Set DRAM size in MiB", cxxopts::value<uint64_t>(memorySize)->default_value(std::to_string(DEFAULT_MEMORY_SIZE/1024/1024)))
        ("c,cache", "Enable cache memory with configurable associativity. Specify the number of ways for set-associative cache (1 for direct-mapped).", cxxopts::value<int>(cacheNumWay))
        ("icount", "Output each instruction's count in memory", cxxopts::value<bool>(enableICount))
        ("istats", "Outputs statistics about executed instructions, focusing on `mv`, `mvi`, and `lw`/`sw` offset distributions.", cxxopts::value<bool>(enableIStats))
        ("icache", "Enable instruction cache", cxxopts::value<bool>(enableICache))
        ("d,debug", "Enable verbose logging (intended for short code execution))", cxxopts::value<bool>(enableDebug))
        ("stdout", "Enable output to standard output stream, not only file", cxxopts::value<bool>(enableStdout))
        ("p,pbar", "Show progress bar, only when in terminal. Use this when program outputs ppm and specify the size(only when width and height are same. e.g. when the image is 128*128, specify 128) of image. The bar is according to numbar of lines in output", cxxopts::value<uint64_t>(imageSize))
        ("g,gdb", "Enable GDB-like debugging", cxxopts::value<bool>(enableGDB))
        ("no-pipeline", "Unable pipeline", cxxopts::value<bool>(enableNoPipeline));
        
        
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
        else if(cacheNumWay == 0){
            std::cerr << "No cache." << std::endl;
            enableCache = false;
        }
        else {
            throw std::runtime_error("The argument of --cache should be non-negative integer.");
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
