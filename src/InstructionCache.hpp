#ifndef INSTRUCTION_CACHE_HPP
#define INSTRUCTION_CACHE_HPP

#include <cstdint>
#include <queue>
#include <vector>
#include <array>
#include <optional>
#include "Util.hpp"

class InstructionCache
{
public:
    static constexpr int64_t IMEMORY_SIZE = 512 * 1024; // Iメモリサイズ（128KiB）
    InstructionCache(const std::array<uint64_t, IMEMORY_SIZE / 4> &instructionMemory);
    void init();
    bool fetch(uint32_t addr);
    [[nodiscard]] inline uint32_t getMissCount() const
    {
        return missCount;
    }

private:
    static constexpr size_t CacheLines = 512;
    static constexpr size_t JALTSize = 2048;
    static constexpr size_t LineWords = 8;

    const std::array<uint64_t, IMEMORY_SIZE / 4> &instructionMemory;
    std::vector<std::optional<uint32_t>> cacheTag;
    std::vector<std::optional<uint32_t>> jaltTag;
    std::vector<std::optional<uint32_t>> jaltContent;
    std::queue<uint32_t> pcHistory;
    std::queue<uint32_t> prefetchPipeline;
    size_t missCount;

    inline void jaltWrite(uint32_t addr, uint32_t content)
    {
        size_t index = addr & (JALTSize - 1);
        jaltTag[index] = addr >> 11;
        jaltContent[index] = content;
    }

    [[nodiscard]] inline bool isJaltValid(uint32_t addr)
    {
        size_t index = addr & (JALTSize - 1);
        return jaltTag[index].has_value() && jaltTag[index].value() == (addr >> 11);
    }

    [[nodiscard]] inline uint32_t jaltGet(uint32_t addr)
    {
        return jaltContent[addr & (JALTSize - 1)].value();
    }

    inline void cacheLoad(uint32_t addr)
    {
        size_t index = (addr & ((CacheLines - 1) << 3)) >> 3;
        cacheTag[index] = addr >> 12;
    }

    [[nodiscard]] inline bool isCacheValid(uint32_t addr)
    {
        size_t index = (addr & ((CacheLines - 1) << 3)) >> 3;
        return cacheTag[index].has_value() && cacheTag[index].value() == (addr >> 12);
    }

    [[nodiscard]] inline bool isJumpOrBranch(uint64_t instruction)
    {
        uint32_t opcode = getOpcode(instruction);
        return opcode == 0x3 || opcode == 0x4 || opcode == 0xE || opcode == 0xF;
    }

    int32_t getJumpTarget(uint64_t instruction);
};

#endif