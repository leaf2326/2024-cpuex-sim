#ifndef OPTION_HANDLER_HPP
#define OPTION_HANDLER_HPP

#include <string>
#include <cstdint>
#include <boost/program_options.hpp>

class OptionHandler {
public:
    // コンストラクタ
    OptionHandler();

    // 引数解析メソッド
    void parse(int argc, char* argv[]);

    // オプション値へのアクセス用メンバ
    std::string inputFilePath;
    std::string programFilePath;
    int outputRegNum;
    uint64_t maxStep;
    uint64_t memorySize;
    bool enableNotify;
    bool enableCache;
    bool enableICount;
    bool enableDebug;
    bool enableGDB;

    static constexpr uint64_t DEFAULT_MAX_STEP = UINT64_MAX;
    static constexpr uint64_t DEFAULT_MEMORY_SIZE = 4 * 1024 * 1024; //4MiB (Default dMemory Size)

private:
    boost::program_options::options_description desc;
};

#endif