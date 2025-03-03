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
    Memory(uint64_t memorySize,
           int64_t input_addr,
           int64_t output_addr,
           size_t l1Lines = 1024,
           size_t l2Lines = 1024,
           size_t lineSize = 16,
           size_t l2Associativity = 4);

    int32_t loadWord(uint32_t address, bool toInt);
    void storeWord(uint32_t address, int32_t value);

    // キャッシュアクセスをシミュレートしてヒット/ミスのみを判定する関数
    bool checkCacheHit(uint32_t address);

    [[nodiscard]]
    inline uint64_t getHitCount() const noexcept
    {
        return l1HitCount + l2HitCount;
    }

    [[nodiscard]]
    inline uint64_t getL1HitCount() const noexcept
    {
        return l1HitCount;
    }

    [[nodiscard]]
    inline uint64_t getL2HitCount() const noexcept
    {
        return l2HitCount;
    }

    [[nodiscard]]
    inline uint64_t getMissCount() const noexcept
    {
        return missCount;
    }

    [[nodiscard]]
    inline uint64_t getNonWbCount() const noexcept
    {
        return nonWbCount;
    }

    [[nodiscard]]
    inline uint64_t getSameRangeWbCount() const noexcept
    {
        return sameRangeWbCount;
    }

    [[nodiscard]]
    inline uint64_t getDiffRangeWbCount() const noexcept
    {
        return diffRangeWbCount;
    }

    size_t getL1Lines() const { return l1Lines; }
    size_t getL2Lines() const { return l2Lines; }
    size_t getL2Sets() const { return l2Sets; }
    size_t getLineSize() const { return lineSize; }
    size_t getL2Associativity() const { return l2Associativity; }
    size_t getWordsPerLine() const { return wordsPerLine; }

    void printCacheState() const;

    std::vector<int32_t> inputData{};
    unsigned int inputIndex = 0;
    std::vector<int32_t> output{};

    bool availableCache = false;
    bool availableLog = true;

    uint64_t lineOutputCount = 0;

    std::vector<int32_t> mainMemory{};
    std::vector<bool> isInitialized{};

    int stallCycles;

private:
    struct CacheBlock
    {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
        std::vector<int32_t> data;
    };

    const uint64_t memorySize;

    // キャッシュパラメータ
    size_t lineSize;
    size_t wordsPerLine;
    size_t offsetBits;

    // L1キャッシュパラメータ
    size_t l1Lines;
    size_t l1IndexBits;

    // L2キャッシュパラメータ
    size_t l2Lines;
    size_t l2Associativity;
    size_t l2Sets;
    size_t l2IndexBits;

    const int64_t input_addr;
    const int64_t output_addr;

    // L1キャッシュ (Direct Mapped)
    std::vector<CacheBlock> l1Cache;
    std::vector<bool> l1IsFirstAccess; 

    // L2キャッシュ (Set Associative)
    std::vector<std::vector<CacheBlock>> l2Cache;
    std::vector<std::vector<size_t>> l2LruOrder;
    std::vector<std::vector<bool>> l2IsFirstAccess;

    [[nodiscard]]
    inline uint32_t getTag(uint32_t address, bool isL1) const noexcept
    {
        if (isL1)
        {
            return address >> (l1IndexBits + offsetBits);
        }
        else
        {
            return address >> (l2IndexBits + offsetBits);
        }
    }

    [[nodiscard]]
    inline uint32_t getL1Index(uint32_t address) const noexcept
    {
        return (address >> offsetBits) & ((1 << l1IndexBits) - 1);
    }

    [[nodiscard]]
    inline uint32_t getL2SetIndex(uint32_t address) const noexcept
    {
        return (address >> offsetBits) & ((1 << l2IndexBits) - 1);
    }

    [[nodiscard]]
    inline uint32_t getOffset(uint32_t address) const noexcept
    {
        uint32_t byteOffset = address & ((1 << offsetBits) - 1);
        // ワードオフセットに変換
        return (byteOffset / sizeof(int32_t));
    }

    void writeBackL2ToL1(uint32_t address, uint32_t l1Index, const CacheBlock &l2Block);
    void writeBackL1ToL2(const CacheBlock &l1Block, uint32_t l1Index);
    void writeBackL2ToMain(uint32_t l2Index, uint32_t wayIndex);

    void loadBlockToL1Cache(uint32_t address);
    void loadBlockToL2Cache(uint32_t address);

    void updateL2LRU(uint32_t setIndex, size_t blockIndex);
    [[nodiscard]] inline size_t findL2LRUVictim(uint32_t setIndex)
    {
        return l2LruOrder[setIndex].front();
    }

    uint64_t l1HitCount = 0;
    uint64_t l2HitCount = 0;
    uint64_t missCount = 0;
    uint64_t nonWbCount = 0;
    uint64_t diffRangeWbCount = 0;
    uint64_t sameRangeWbCount = 0;
};

#endif // MEMORY_HPP