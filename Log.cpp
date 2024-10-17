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
    if (stallType == 1)
    {
        stallCount1++;
    }
    else if (stallType == 2)
    {
        stallCount2++;
    }
}

void Log::printLog() const
{
    std::cout << "Total instructions executed: " << totalInstructions << std::endl;

    std::cout << "Instruction counts:" << std::endl;
    for (const auto &pair : instructionCounts)
    {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }

    std::cout << "Branch predictions: " << branchPredCount << std::endl;
    std::cout << "Flushes due to branch misprediction: " << flushCount << std::endl;
    std::cout << "1-cycle stalls: " << stallCount1 << std::endl;
    std::cout << "2-cycle stalls: " << stallCount2 << std::endl;
}
