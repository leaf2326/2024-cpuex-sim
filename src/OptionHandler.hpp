#ifndef OPTION_HANDLER_HPP
#define OPTION_HANDLER_HPP

#include <string>
#include <cstdint>
#include <cxxopts.hpp>

class OptionHandler
{
public:
    OptionHandler();

    void parse(int argc, char *argv[]);

    std::string inputFilePath;
    std::string outputFilePath;
    std::string programFilePath;
    int outputRegNum;
    uint64_t maxStep;
    uint64_t memorySize;
    size_t l1Lines;
    size_t l2Lines;
    size_t lineSize;
    size_t l2Associativity;
    bool enableNotify;
    bool enableCache;
    bool noCache;
    bool enableICount;
    bool enableIStats;
    bool enableDebug;
    bool enableStdout;
    uint64_t imageSize;
    bool enableGDB;
    bool enableICache;
    bool enableNoPipeline;

    static constexpr uint64_t DEFAULT_MAX_STEP = UINT64_MAX;
    static constexpr uint64_t DEFAULT_MEMORY_SIZE = 4 * 1024 * 1024; // 4MiB (Default dMemory Size)
    static constexpr size_t DEFAULT_L1LINES = 256;
    static constexpr size_t DEFAULT_L2LINES = 1024;
    static constexpr size_t DEFAULT_LINESIZE = 64; // 512bits
    static constexpr size_t DEFAULT_L2ASSOCIATIVITY = 4;

private:
    cxxopts::Options options;
};

#endif