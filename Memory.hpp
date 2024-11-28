#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <iostream>
class Memory
{
public:
    Memory(uint64_t memorySize, size_t cacheSize, size_t blockSize, int64_t input_addr, int64_t output_addr);
    int32_t loadWord(uint32_t address, bool toInt);
    void storeWord(uint32_t address, int32_t value);
    void printCache() const;
    std::vector<int32_t> inputData{};
    unsigned int inputIndex = 0;
    std::vector<int32_t> output{};

    bool availableCache = false;

    std::vector<int32_t> mainMemory{};
    std::vector<bool> isInitialized{};

private:
    struct CacheBlock
    {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
        std::vector<int32_t> data;
    };

    const uint64_t memorySize;

    size_t cacheSize;
    size_t blockSize;
    size_t numBlocks;
    size_t offsetBits;
    size_t indexBits;
    const int64_t input_addr;
    const int64_t output_addr;

    std::vector<CacheBlock> cache;

    uint32_t getTag(uint32_t address);
    uint32_t getIndex(uint32_t address);
    uint32_t getOffset(uint32_t address);
    void writeBack(uint32_t index);
    void loadBlockToCache(uint32_t address);
};

#endif
