#ifndef LOG_HPP
#define LOG_HPP

#include <unordered_map>
#include <string>
#include <iostream>

class Log
{
protected:
    uint64_t totalInstructions = 0;                  // 命令の総実行回数
    std::unordered_map<int, int> nStallCount{}; // ストールの回数
    uint64_t branchPredCount = 0;                    // 分岐予測の回数
    uint64_t flushCount = 0;                         // パイプラインのフラッシュ回数

    // 命令ごとの実行回数
    std::unordered_map<std::string, uint64_t> instructionCounts;

    uint64_t sumCLKCount();

public:
    void logInstruction(const std::string &instructionName);

    void logFlush();

    void logBranchPrediction();

    void logStall(int stallType);

    void printLog();
};

#endif // LOG_HPP
