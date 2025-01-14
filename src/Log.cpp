#include "Log.hpp"
#include <iomanip>
#include <algorithm>
#include <utility>

/*
uint64_t Log::sumCLKCount()
{
    uint64_t CLK = 0;
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
*/

void Log::printLog()
{
    std::cerr << "________Start printLog________" << std::endl;
    std::cerr << "Total instructions executed: " << totalInstructions << std::endl;

    std::cerr << "Instruction counts: " << std::endl;
    {
        std::vector<std::pair<uint64_t, std::string>> v;
        for (int i = 0; i<MAX_INSTRUCTION_TYPE;++i)
        {
            v.emplace_back(std::make_pair(instructionCounts[i], typeToString(i)));
        }
        sort(v.rbegin(), v.rend());
        for (const auto &p : v)
        {
            std::cerr << std::setw(8) << p.second + ":" << std::setw(15) << p.first << std::endl;
        }
    }
    std::cerr << "Total Branch predictions: " << branchPredCount << std::endl;
    std::cerr << "Number of Branch Prediction Misses: " << flushCount << std::endl;
    std::cerr << "Branch Prediction Accuracy: " << (double)(branchPredCount - flushCount) / (double)branchPredCount << std::endl;
    /*
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
    */
}
