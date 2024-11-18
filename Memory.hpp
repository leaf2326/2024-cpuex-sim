#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <iostream>
class Memory
{
public:
    Memory();
    int32_t loadWord(uint32_t address, bool toInt);
    void storeWord(uint32_t address, int32_t value);
    void printCache() const;
    std::vector<int32_t> inputData{};
    unsigned int inputIndex = 0;
    std::vector<int32_t> output{};
   
    static constexpr int64_t CACHE_SIZE = 1024 * 16;
    static constexpr int64_t BLOCK_SIZE = 16;
    static constexpr int64_t numBlocks = CACHE_SIZE / BLOCK_SIZE;
    static constexpr int64_t INPUT_ADDRESS = 100;
    static constexpr int64_t OUTPUT_ADDRESS = 104;
     static constexpr int64_t DMEMORY_SIZE = 4 * 1024 * 1024; // Dメモリサイズ（4MiB）
    int32_t mainMemory[DMEMORY_SIZE / 4]{};

private:
    struct CacheBlock
    {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
        int32_t data[BLOCK_SIZE / 4];
    };
    size_t offsetBits;
    size_t indexBits;

    CacheBlock cache[numBlocks];

    uint32_t getTag(uint32_t address);
    uint32_t getIndex(uint32_t address);
    uint32_t getOffset(uint32_t address);
    void writeBack(uint32_t index);
    void loadBlockToCache(uint32_t address);
};

#endif
