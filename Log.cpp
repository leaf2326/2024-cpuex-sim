#include "Log.hpp"

void Log::logInstruction(const std::string &instructionName)
{
    totalInstructions++;
    instructionCounts[instructionName]++;
}

void Log::logFlush()
{
    flushCount++;
}

void Log::logBranchPrediction()
{
    branchPredCount++;
}

void Log::logStall(int stallType)
{
    nStallCount[stallType]++;
}
int Log::sumCLKCount()
{
    int CLK = 0;
    // 命令の数だけ加算
    CLK += totalInstructions;
    // ストールの数だけ加算
    for (const auto &pair : nStallCount)
    {
        CLK += pair.first * pair.second;
    }
    // 分岐予測ミスのフラッシュ分だけ加算
    CLK += branchPredCount * 2;
    // パイプラインのステージ分加算
    CLK += 4;
    return CLK;
}

void Log::printLog()
{
    std::cerr << "__Start printLog__" << std::endl;
    std::cerr << "Total instructions executed: " << totalInstructions << std::endl;

    std::cerr << "Instruction counts:" << std::endl;
    for (const auto &pair : instructionCounts)
    {
        std::cerr << "  " << pair.first << ": " << pair.second << std::endl;
    }

    std::cerr << "Branch predictions: " << branchPredCount << std::endl;
    std::cerr << "Flushes due to branch misprediction: " << flushCount << std::endl;
    if (nStallCount.empty())
    {
        std::cerr << "No n-cycle stalls detected!" << std::endl;
    }
    else
    {
        for (const auto &pair : nStallCount)
        {
            std::cerr << pair.first << "-cycle stalls: " << pair.second << std::endl;
        }
    }
    std::cerr << "Clock cycle: " << sumCLKCount() << std::endl;
}
