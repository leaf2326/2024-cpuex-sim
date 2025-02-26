#include "Memory.hpp"
#include <cmath>
#include <bit>

Memory::Memory(uint64_t memorySize, size_t cacheSize, size_t lineSize, int64_t input_addr, int64_t output_addr, size_t associativity)
    : memorySize(memorySize), cacheSize(cacheSize), lineSize(lineSize), input_addr(input_addr), output_addr(output_addr), cacheAssociativity(associativity)
{
    offsetBits = static_cast<size_t>(std::log2(lineSize));

    size_t totalLines = cacheSize / lineSize;
    size_t linesPerWay = totalLines / cacheAssociativity;
    indexBits = static_cast<size_t>(std::log2(linesPerWay));

    setAssociativeCache.resize(linesPerWay, std::vector<CacheBlock>(cacheAssociativity, CacheBlock{false, false, 0, std::vector<int32_t>(lineSize / sizeof(int32_t))}));
    lruOrder.resize(linesPerWay, std::vector<size_t>(cacheAssociativity));
    isFirstAccess.resize(linesPerWay, std::vector<bool>(cacheAssociativity, true));
    for (auto &setOrder : lruOrder)
    {
        std::iota(setOrder.begin(), setOrder.end(), 0);
    }

    mainMemory.resize(memorySize >> 2, 0);
    isInitialized.resize(memorySize >> 2, 0);
}

void Memory::writeBack(uint32_t index, uint32_t setIndex)
{

    CacheBlock &block = setAssociativeCache[setIndex][index];
    if (block.valid && block.dirty) [[unlikely]]
    {
        uint32_t baseAddress = (block.tag << (indexBits + offsetBits)) | (setIndex << offsetBits);
        for (size_t i = 0; i < block.data.size(); ++i)
        {
            mainMemory[(baseAddress >> 2) + i] = block.data[i];
        }
        block.dirty = false;
        if (availableLog) [[unlikely]]
        {
            std::cerr << "Write-back occurred for set index " << setIndex << " and block " << index << std::endl;
        }
    }
}

void Memory::loadBlockToCache(uint32_t address)
{
    uint32_t tag = getTag(address);

    uint32_t setIndex = getSetIndex(address);
    if (setIndex >= setAssociativeCache.size()) [[unlikely]]
    {
        std::cerr << "Error: Cache set index out of range: " << setIndex << std::endl;
        throw std::out_of_range("Cache set index out of range");
    }
    size_t victimIndex = findLRUVictim(setIndex);
    CacheBlock &block = setAssociativeCache[setIndex][victimIndex];
    if (block.valid && block.dirty) [[unlikely]]
    {
        uint32_t oldAddress = (block.tag << (indexBits + offsetBits)) | (setIndex << offsetBits);
        uint32_t oldRange = (oldAddress >> 22) & 0x7;
        uint32_t newRange = (address >> 22) & 0x7;
        if (isFirstAccess[setIndex][victimIndex])
        {
            // 初回参照ミス
            if (availableLog) [[unlikely]]
            {
                std::cerr << "First access miss" << std::endl;
            }
            isFirstAccess[setIndex][victimIndex] = false;
        }
        else if (oldRange != newRange)
        {
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Different range write-back" << std::endl;
            }
            ++diffRangeWbCount;
        }
        else
        {
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Same range write-back" << std::endl;
            }
            ++sameRangeWbCount;
        }
    }
    else
    {
        ++nonWbCount;
    }

    writeBack(victimIndex, setIndex);

    block.tag = tag;
    block.valid = true;
    block.dirty = false;

    uint32_t baseAddress = address & ~((1 << offsetBits) - 1);
    for (size_t i = 0; i < block.data.size(); ++i)
    {
        if ((baseAddress >> 2) + i >= mainMemory.size()) [[unlikely]]
        {
            std::cerr << "Error: Main memory access out of range: " << baseAddress + i << std::endl;
            throw std::out_of_range("Main memory access out of range");
        }
        block.data[i] = mainMemory[(baseAddress >> 2) + i];
    }
    updateLRU(setIndex, victimIndex);

    if (availableLog) [[unlikely]]
    {
        std::cerr << "Cache miss: Loaded block to cache at set index " << setIndex << " and victim block " << victimIndex << std::endl;
    }
}

void Memory::updateLRU(uint32_t setIndex, size_t blockIndex)
{
    auto &order = lruOrder[setIndex];
    auto it = std::find(order.begin(), order.end(), blockIndex);
    if (it == order.end()) [[unlikely]]
    {
        std::cerr << "Error: Block index not found in LRU order." << std::endl;
        throw std::logic_error("LRU update failed");
    }
    else if (it != order.end())
    {
        order.erase(it);
        order.emplace_back(blockIndex);
    }
}

int32_t Memory::loadWord(uint32_t address, bool isLw)
{
    if (address < 0 || address >= memorySize) [[unlikely]]
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    if (!isInitialized[address >> 2] && address != input_addr) [[unlikely]]
    {
        throw std::out_of_range("Access to uninitialized dMemory part " + std::to_string(address));
    }
    if (address == input_addr || address == output_addr)
    {
        auto temp = mainMemory[address >> 2];
        if (address == input_addr)
        {
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Input requested at input_addr (" << std::hex << address << ")" << std::dec << std::endl;
            }
            if (inputIndex >= inputData.size()) [[unlikely]]
            {
                throw std::out_of_range("No more input data available");
            }
            else
            {
                if (isLw)
                {
                    float floatValue = std::bit_cast<float>(inputData[inputIndex]);
                    int32_t intValue = static_cast<int32_t>(floatValue);
                    temp = intValue;
                }
                else
                {
                    temp = inputData[inputIndex];
                }
                if (availableLog) [[unlikely]]
                {
                    std::cerr << "Input: " << std::hex << temp << std::dec << std::endl;
                }
                storeWord(input_addr, temp);
                ++inputIndex;
            }
        }
        return temp;
    }
    if (!availableCache)
    {
        int32_t value = mainMemory[address >> 2];
        if (availableLog) [[unlikely]]
        {
            std::cerr << "Cache disabled. Loaded from main memory: " << std::hex << address << ": " << value << std::dec << std::endl;
        }
        return value;
    }
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    uint32_t setIndex = getSetIndex(address);

    for (size_t i = 0; i < cacheAssociativity; ++i)
    {
        CacheBlock &block = setAssociativeCache[setIndex][i];
        if (block.valid && block.tag == tag) [[unlikely]]
        {
            ++hitCount;
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Cache hit at set index " << setIndex << ", way " << i << " for address " << std::hex << address << std::dec << std::endl;
            }
            updateLRU(setIndex, i);
            return block.data[offset];
        }
    }

    ++missCount;
    if (availableLog) [[unlikely]]
    {
        std::cerr << "Cache miss at set index " << " for address " << std::hex << address << std::dec << std::endl;
    }

    size_t victimIndex = findLRUVictim(setIndex);
    loadBlockToCache(address);
    return setAssociativeCache[setIndex][victimIndex].data[offset];
}

void Memory::storeWord(uint32_t address, int32_t value)
{
    if (address < 0 || address >= memorySize) [[unlikely]]
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    isInitialized[address >> 2] = true;
    if (address == input_addr || address == output_addr)
    {
        if (address == output_addr)
        {
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Output written at output_addr (" << std::hex << address << "): " << value << std::dec << std::endl;
                std::cerr << "Output: " << std::hex << mainMemory[address >> 2] << std::dec << std::endl;
            }
            if (char(value & 0xFF) == '\n') [[unlikely]]
            {
                ++lineOutputCount;
            }
            output.emplace_back(value);
        }
        mainMemory[address >> 2] = value;
        return;
    }
    if (!availableCache)
    {
        if (availableLog) [[unlikely]]
        {
            std::cerr << "Cache disabled. Stored directly to main memory at " << std::hex << address << ": " << value << std::dec << std::endl;
        }
        mainMemory[address >> 2] = value;
        return;
    }
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    uint32_t setIndex = getSetIndex(address);

    for (size_t i = 0; i < cacheAssociativity; ++i)
    {
        CacheBlock &block = setAssociativeCache[setIndex][i];
        if (block.valid && block.tag == tag) [[unlikely]]
        {
            ++hitCount;
            block.data[offset] = value;
            block.dirty = true;
            updateLRU(setIndex, i);
            if (availableLog) [[unlikely]]
            {
                std::cerr << "Cache hit at set index " << setIndex << ", way " << i << " for address " << std::hex << address << std::dec << std::endl;
            }
            return;
        }
    }

    ++missCount;
    if (availableLog) [[unlikely]]
    {
        std::cerr << "Cache miss at set index " << setIndex << " for address " << std::hex << address << std::dec << std::endl;
    }

    size_t victimIndex = findLRUVictim(setIndex);
    loadBlockToCache(address);
    setAssociativeCache[setIndex][victimIndex].data[offset] = value;
    setAssociativeCache[setIndex][victimIndex].dirty = true;
}

void Memory::printCacheState() const
{
    std::cerr << "________Current Cache State________" << std::endl;

    for (size_t i = 0; i < setAssociativeCache.size(); ++i)
    {
        std::cerr << "Set Index " << i << ":" << std::endl;
        for (size_t j = 0; j < cacheAssociativity; ++j)
        {
            const CacheBlock &block = setAssociativeCache[i][j];
            std::cerr << "  Way " << j << ": ";
            if (block.valid)
            {
                std::cerr << "[Tag: " << block.tag << ", Dirty: " << (block.dirty ? "Yes" : "No") << "] Data: ";
                for (const auto &word : block.data)
                {
                    std::cerr << word << " ";
                }
            }
            else
            {
                std::cerr << "Invalid";
            }
            std::cerr << std::endl;
        }
    }
}

bool Memory::checkCacheHit(uint32_t address) {
    if (address < 0 || address >= memorySize) [[unlikely]] {
        throw std::out_of_range("dMemory access out of bounds");
    }
    
    if (address == input_addr || address == output_addr) {
        stallCycles = 1;
        return true;
    }
    
    // キャッシュが無効の場合常にヒット扱い
    if (!availableCache) {
        stallCycles = 1;
        return true;
    }
    
    uint32_t tag = getTag(address);
    uint32_t setIndex = getSetIndex(address);
    
    for (size_t i = 0; i < cacheAssociativity; ++i) {
        CacheBlock &block = setAssociativeCache[setIndex][i];
        if (block.valid && block.tag == tag) [[unlikely]] {
            stallCycles = 1;
            return true;
        }
    }
    
    size_t victimIndex = findLRUVictim(setIndex);
    CacheBlock &block = setAssociativeCache[setIndex][victimIndex];

    // ミス時のストールサイクル数を計算
    if (block.valid && block.dirty) [[unlikely]] {
        uint32_t oldAddress = (block.tag << (indexBits + offsetBits)) | (setIndex << offsetBits);
        uint32_t oldRange = (oldAddress >> 22) & 0x7;
        uint32_t newRange = (address >> 22) & 0x7;
        if (isFirstAccess[setIndex][victimIndex]) {
            // 初回参照ミス
            stallCycles = 2;
        }
        else if (oldRange != newRange) {
            stallCycles = 57;
        }
        else {
            stallCycles = 66;
        }
    }
    else {
        stallCycles = 54;
    }
    
    return false;
}