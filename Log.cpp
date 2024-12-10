#include "Log.hpp"
#include <iomanip>
#include <algorithm>
#include <utility>
#include <vector>

void Log::logInstAddr(uint32_t address)
{
    instAddrCounts[address]++;
}
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
    std::cerr << "__Start printLog__" << std::endl;
    std::cerr << "Total instructions executed: " << totalInstructions << std::endl;

    std::cerr << "__Instruction counts__" << std::endl;
    {
        std::vector<std::pair<uint64_t,std::string>> v;
        for (const auto &p : instructionCounts)
        {
            v.push_back(std::make_pair(p.second, p.first));
        }
        sort(v.rbegin(), v.rend());
        for (const auto &p : v)
        {
            std::cerr << std::setw(8) << p.second + ":" << std::setw(15) << p.first << std::endl;
        }
    }
    std::cerr << "Branch predictions: " << branchPredCount << std::endl;
    std::cerr << "Flushes due to branch misprediction: " << flushCount << std::endl;
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
