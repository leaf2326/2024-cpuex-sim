#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>

class Memory
{
public:
    Memory(uint64_t memorySize, size_t cacheSize, size_t lineSize, int64_t input_addr, int64_t output_addr, bool enableDirect = true, size_t associativity = 1);

    int32_t loadWord(uint32_t address, bool toInt);
    void storeWord(uint32_t address, int32_t value);

    uint64_t getHitCount() const;
    uint64_t getMissCount() const;

    void printCacheState() const;

    std::vector<int32_t> inputData{};
    unsigned int inputIndex = 0;
    std::vector<int32_t> output{};

    bool availableCache = false;
    bool availableLog = true;

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
    size_t lineSize;
    size_t numLines;
    size_t offsetBits;
    size_t indexBits;
    const int64_t input_addr;
    const int64_t output_addr;

    
    const bool enableDirect;

    std::vector<CacheBlock> directCache;

    size_t cacheAssociativity;
    std::vector<std::vector<CacheBlock>> setAssociativeCache;
    std::vector<std::vector<size_t>> lruOrder;

    uint32_t getTag(uint32_t address);
    uint32_t getIndex(uint32_t address);
    uint32_t getSetIndex(uint32_t address);
    uint32_t getOffset(uint32_t address);
    void writeBack(uint32_t index, uint32_t setIndex = 0, bool isDirect = true);
    void loadBlockToCache(uint32_t address);

    void updateLRU(uint32_t setIndex, size_t blockIndex);
    size_t findLRUVictim(uint32_t setIndex);

    uint64_t hitCount = 0;
    uint64_t missCount = 0;
};

#endif
