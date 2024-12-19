#ifndef OPTION_HANDLER_HPP
#define OPTION_HANDLER_HPP

#include <string>
#include <cstdint>
#include "../include/cxxopts.hpp"

class OptionHandler {
public:
    OptionHandler();

    void parse(int argc, char* argv[]);

    std::string inputFilePath;
    std::string programFilePath;
    int outputRegNum;
    uint64_t maxStep;
    uint64_t memorySize;
    bool enableNotify;
    bool enableCache;
    bool enableICount;
    bool enableIStats;
    bool enableDebug;
    bool enableGDB;

    static constexpr uint64_t DEFAULT_MAX_STEP = UINT64_MAX;
    static constexpr uint64_t DEFAULT_MEMORY_SIZE = 4 * 1024 * 1024; //4MiB (Default dMemory Size)

private:
    cxxopts::Options options;
};

#endif