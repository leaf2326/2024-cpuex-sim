#ifndef OPTION_HANDLER_HPP
#define OPTION_HANDLER_HPP

#include <string>
#include <cstdint>
#include <cxxopts.hpp>

class OptionHandler {
public:
    OptionHandler();

    void parse(int argc, char* argv[]);

    std::string inputFilePath;
    std::string outputFilePath;
    std::string programFilePath;
    int outputRegNum;
    uint64_t maxStep;
    uint64_t memorySize;
    int cacheNumWay;
    bool enableNotify;
    bool enableCache;
    bool enableICount;
    bool enableIStats;
    bool enableDebug;
    bool enableStdout;
    uint64_t imageSize;
    bool enableGDB;
    bool enableICache;

    static constexpr uint64_t DEFAULT_MAX_STEP = UINT64_MAX;
    static constexpr uint64_t DEFAULT_MEMORY_SIZE = 4 * 1024 * 1024; //4MiB (Default dMemory Size)

private:
    cxxopts::Options options;
};

#endif