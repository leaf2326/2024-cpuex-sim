#include "InstructionCache.hpp"

InstructionCache::InstructionCache(const std::array<uint32_t, IMEMORY_SIZE / 4> &instructionMemory)
    : instructionMemory(instructionMemory), missCount(0) {
    init();
}

void InstructionCache::init() {
    cacheTag.resize(CacheLines, std::nullopt);
    jaltTag.resize(JALTSize, std::nullopt);
    jaltContent.resize(JALTSize, std::nullopt);
    while (pcHistory.size() < 4) pcHistory.push(0);
    while (prefetchPipeline.size() < 5) prefetchPipeline.push(0);
}

int32_t InstructionCache::getJumpTarget(uint32_t instruction)
{
    uint32_t opcode = getOpcode(instruction);
    if (opcode == 0x3 || opcode == 0xE)
    {
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xE)
        {
            imm |= 1 << 14;
        }
        return imm;
    }
    if (opcode == 0x4 || opcode == 0xF)
    {
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xF)
        {
            imm |= 1 << 14;
        }
        return imm;
    }
    return 0;
}

bool InstructionCache::fetch(uint32_t addr) {
    cacheLoad(prefetchPipeline.front());
    prefetchPipeline.pop();

    bool cacheHit = isCacheValid(addr);
    uint32_t prefetchAddr = cacheHit ? (isJaltValid(addr) ? jaltGet(addr) : addr + 3) : addr;
    prefetchPipeline.push(prefetchAddr);

    uint32_t prevPC = pcHistory.front();
    pcHistory.pop();
    pcHistory.push(addr);
    uint32_t instruction = instructionMemory[addr];
    if (isJumpOrBranch(instruction)) {
        uint32_t jumpTarget = getJumpTarget(instruction);
        jaltWrite(prevPC, jumpTarget);
    }

    if (!cacheHit) ++missCount;
    return cacheHit;
}