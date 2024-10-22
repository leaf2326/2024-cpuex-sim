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

void Log::printLog() const
{
    std::cout <<"__Start printLog__" << std::endl;
    std::cout << "Total instructions executed: " << totalInstructions << std::endl;

    std::cout << "Instruction counts:" << std::endl;
    for (const auto &pair : instructionCounts)
    {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }

    std::cout << "Branch predictions: " << branchPredCount << std::endl;
    std::cout << "Flushes due to branch misprediction: " << flushCount << std::endl;

    for (const auto &pair : nStallCount)
    {
        std::cout << pair.first << "-cycle stalls: " << pair.second << std::endl;
    }
    
    std::cout <<"__End printLog__" << std::endl;
}
