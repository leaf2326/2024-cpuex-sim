#include "InstructionCache.hpp"

InstructionCache::InstructionCache(const std::array<uint64_t, IMEMORY_SIZE / 4> &instructionMemory)
    : missCount(0), instructionMemory(instructionMemory) {
    init();
}

void InstructionCache::init() {
    cacheTag.resize(CacheLines, std::nullopt);
    jaltTag.resize(JALTSize, std::nullopt);
    jaltContent.resize(JALTSize, std::nullopt);
    while (pcHistory.size() < 4) pcHistory.push(0);
    while (prefetchPipeline.size() < 5) prefetchPipeline.push(0);
}

int32_t InstructionCache::getJumpTarget(uint64_t instruction)
{
    return getImmediate(instruction);
}

bool InstructionCache::fetch(uint32_t addr) {
    cacheLoad(prefetchPipeline.front());
    prefetchPipeline.pop();

    bool cacheHit = isCacheValid(addr);
    if(!cacheHit){
        for (int i = 0; i < 5; i++) {
            
			cacheLoad(prefetchPipeline.front());
            prefetchPipeline.pop();
			prefetchPipeline.push(0);
		}
		// ミスしたところを取ってくる
		cacheLoad(addr);
    }
    uint32_t prefetchAddr = isJaltValid(addr) ? jaltGet(addr) : addr + 12;
    prefetchPipeline.push(prefetchAddr);

    uint32_t prevPC = pcHistory.front();
    pcHistory.pop();
    pcHistory.push(addr);
    uint64_t instruction = instructionMemory[addr];
    if (isJumpOrBranch(instruction)) {
        uint32_t jumpTarget = getJumpTarget(instruction);
        jaltWrite(prevPC, jumpTarget);
    }

    if (!cacheHit) ++missCount;
    return cacheHit;
}