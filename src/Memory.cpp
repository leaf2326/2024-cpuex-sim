#include "Memory.hpp"
#include <cmath>
#include <bit>

Memory::Memory(uint64_t memorySize, size_t cacheSize, size_t lineSize, int64_t input_addr, int64_t output_addr, bool enableDirect, size_t associativity)
    : memorySize(memorySize), cacheSize(cacheSize), lineSize(lineSize), input_addr(input_addr), output_addr(output_addr),
      enableDirect(enableDirect), cacheAssociativity(enableDirect ? 1 : associativity)
{
    offsetBits = static_cast<size_t>(std::log2(lineSize));
    if (enableDirect)
    {
        numLines = cacheSize / lineSize;
        indexBits = static_cast<size_t>(std::log2(numLines));
        directCache.resize(numLines, CacheBlock{false, false, 0, std::vector<int32_t>(lineSize / sizeof(int32_t))});
    }
    else
    {
        size_t totalLines = cacheSize / lineSize;
        size_t linesPerWay = totalLines / cacheAssociativity;
        indexBits = static_cast<size_t>(std::log2(linesPerWay));

        setAssociativeCache.resize(linesPerWay, std::vector<CacheBlock>(cacheAssociativity, CacheBlock{false, false, 0, std::vector<int32_t>(lineSize / sizeof(int32_t))}));
        lruOrder.resize(linesPerWay, std::vector<size_t>(cacheAssociativity));
        for (auto &setOrder : lruOrder)
        {
            std::iota(setOrder.begin(), setOrder.end(), 0);
        }
    }

    mainMemory.resize(memorySize / 4, 0);
    isInitialized.resize(memorySize / 4, 0);
}

void Memory::writeBack(uint32_t index, uint32_t setIndex, bool isDirect)
{
    if (isDirect)
    {
        CacheBlock &block = directCache[index];
        if (block.valid && block.dirty)
        {
            uint32_t baseAddress = (block.tag << (indexBits + offsetBits)) | (index << offsetBits);
            for (size_t i = 0; i < block.data.size(); ++i)
            {
                mainMemory[baseAddress + i] = block.data[i];
            }
            block.dirty = false;
            if (availableLog)
            {
                std::cerr << "Write-back occurred for index " << std::hex << index << std::dec << std::endl;
            }
        }
    }
    else
    {
        CacheBlock &block = setAssociativeCache[setIndex][index];
        if (block.valid && block.dirty)
        {
            uint32_t baseAddress = (block.tag << (indexBits + offsetBits)) | (setIndex << offsetBits);
            for (size_t i = 0; i < block.data.size(); ++i)
            {
                mainMemory[baseAddress + i] = block.data[i];
            }
            block.dirty = false;
            if (availableLog)
            {
                std::cerr << "Write-back occurred for set index " << setIndex << " and block " << index << std::endl;
            }
        }
    }
}

void Memory::loadBlockToCache(uint32_t address)
{
    uint32_t tag = getTag(address);

    if (enableDirect)
    {
        uint32_t index = getIndex(address);
        if (index >= directCache.size())
        {
            std::cerr << "Error: Cache index out of range: " << index << std::endl;
            throw std::out_of_range("Cache index out of range");
        }
        writeBack(index, 0, true);

        CacheBlock &block = directCache[index];
        block.tag = tag;
        block.valid = true;
        block.dirty = false;

        uint32_t baseAddress = address & ~((1 << offsetBits) - 1);
        for (size_t i = 0; i < block.data.size(); ++i)
        {
            if (baseAddress + i >= mainMemory.size())
            {
                std::cerr << "Error: Main memory access out of range: " << baseAddress + i << std::endl;
                throw std::out_of_range("Main memory access out of range");
            }
            block.data[i] = mainMemory[baseAddress + i];
        }

        if (availableLog)
        {
            std::cerr << "Cache miss: Loaded block to cache at index " << std::hex << index << " with tag " << tag << std::dec << std::endl;
        }
    }
    else
    {
        uint32_t setIndex = getSetIndex(address);
        if (setIndex >= setAssociativeCache.size())
        {
            std::cerr << "Error: Cache set index out of range: " << setIndex << std::endl;
            throw std::out_of_range("Cache set index out of range");
        }
        size_t victimIndex = findLRUVictim(setIndex);

        writeBack(victimIndex, setIndex, false);

        CacheBlock &block = setAssociativeCache[setIndex][victimIndex];
        block.tag = tag;
        block.valid = true;
        block.dirty = false;

        uint32_t baseAddress = address & ~((1 << offsetBits) - 1);
        for (size_t i = 0; i < block.data.size(); ++i)
        {
            if (baseAddress + i >= mainMemory.size())
            {
                std::cerr << "Error: Main memory access out of range: " << baseAddress + i << std::endl;
                throw std::out_of_range("Main memory access out of range");
            }
            block.data[i] = mainMemory[baseAddress + i];
        }
        updateLRU(setIndex, victimIndex);

        if (availableLog)
        {
            std::cerr << "Cache miss: Loaded block to cache at set index " << setIndex << " and victim block " << victimIndex << std::endl;
        }
    }
}

void Memory::updateLRU(uint32_t setIndex, size_t blockIndex)
{
    auto &order = lruOrder[setIndex];
    auto it = std::find(order.begin(), order.end(), blockIndex);
    if (it == order.end())
    {
        std::cerr << "Error: Block index not found in LRU order." << std::endl;
        throw std::logic_error("LRU update failed");
    }
    if (it != order.end())
    {
        order.erase(it);
        order.push_back(blockIndex);
    }
}

size_t Memory::findLRUVictim(uint32_t setIndex)
{
    return lruOrder[setIndex].front();
}

int32_t Memory::loadWord(uint32_t address, bool isLw)
{
    if (address < 0 || address >= memorySize)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    if (!isInitialized[address] && address != input_addr)
    {
        throw std::out_of_range("Access to uninitialized dMemory part");
    }
    if (address == input_addr || address == output_addr)
    {
        auto temp = mainMemory[address];
        if (address == input_addr)
        {
            if (availableLog)
            {
                std::cerr << "Input requested at input_addr (" << std::hex << address << ")" << std::dec << std::endl;
            }
            if (inputIndex >= inputData.size())
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
                if (availableLog)
                {
                    std::cerr << "Input: " << std::hex << temp << std::dec << std::endl;
                }
                storeWord(input_addr, temp);
                inputIndex++;
            }
        }
        return temp;
    }
    if (!availableCache)
    {
        int32_t value = mainMemory[address];
        if (availableLog)
        {
            std::cerr << "Cache disabled. Loaded from main memory: " << std::hex << address << ": " << value << std::dec << std::endl;
        }
        return value;
    }
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (enableDirect)
    {
        uint32_t index = getIndex(address);
        CacheBlock &block = directCache[index];

        if (block.valid && block.tag == tag)
        {
            ++hitCount;
            if (availableLog)
            {
                std::cerr << "Cache hit at index " << std::hex << index << " for address " << address << std::dec << std::endl;
            }
            return block.data[offset];
        }
        else
        {
            ++missCount;
            if (availableLog)
            {
                std::cerr << "Cache miss at index " << std::hex << index << " for address " << address << std::dec << std::endl;
            }
            loadBlockToCache(address);
            return directCache[index].data[offset];
        }
    }
    else
    {
        uint32_t setIndex = getSetIndex(address);

        for (size_t i = 0; i < cacheAssociativity; ++i)
        {
            CacheBlock &block = setAssociativeCache[setIndex][i];
            if (block.valid && block.tag == tag)
            {
                ++hitCount;
                if (availableLog)
                {
                    std::cerr << "Cache hit at set index " << setIndex << ", way " << i << " for address " << std::hex << address << std::dec << std::endl;
                }
                updateLRU(setIndex, i);
                return block.data[offset];
            }
        }

        ++missCount;
        if (availableLog)
        {
            std::cerr << "Cache miss at set index " << " for address " << std::hex << address << std::dec << std::endl;
        }

        size_t victimIndex = findLRUVictim(setIndex);
        loadBlockToCache(address);
        return setAssociativeCache[setIndex][victimIndex].data[offset];
    }
}

void Memory::storeWord(uint32_t address, int32_t value)
{
    if (address < 0 || address >= memorySize / 4)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    isInitialized[address] = true;
    if (address == input_addr || address == output_addr)
    {
        if (address == output_addr)
        {
            if (availableLog)
            {
                std::cerr << "Output written at output_addr (" << std::hex << address << "): " << value << std::dec << std::endl;
                std::cerr << "Output: " << std::hex << mainMemory[address] << std::dec << std::endl;
            }
            if(char(value & 0xFF) == '\n'){
                lineOutputCount++;
            }
            output.emplace_back(value);
        }
        mainMemory[address] = value;
        return;
    }
    if (!availableCache)
    {
        if (availableLog)
        {
            std::cerr << "Cache disabled. Stored directly to main memory at " << std::hex << address << ": " << value << std::dec << std::endl;
        }
        mainMemory[address] = value;
        return;
    }
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (enableDirect)
    {
        uint32_t index = getIndex(address);
        CacheBlock &block = directCache[index];

        if (block.valid && block.tag == tag)
        {
            ++hitCount;
            block.data[offset] = value;
            block.dirty = true;
        }
        else
        {
            ++missCount;
            loadBlockToCache(address);
            block.data[offset] = value;
            block.dirty = true;
        }
    }
    else
    {
        uint32_t setIndex = getSetIndex(address);

        for (size_t i = 0; i < cacheAssociativity; ++i)
        {
            CacheBlock &block = setAssociativeCache[setIndex][i];
            if (block.valid && block.tag == tag)
            {
                ++hitCount;
                block.data[offset] = value;
                block.dirty = true;
                updateLRU(setIndex, i);
                if (availableLog)
                {
                    std::cerr << "Cache hit at set index " << setIndex << ", way " << i << " for address " << std::hex << address << std::dec << std::endl;
                }
                return;
            }
        }

        ++missCount;
        if (availableLog)
        {
            std::cerr << "Cache miss at set index " << setIndex << " for address " << std::hex << address << std::dec << std::endl;
        }

        size_t victimIndex = findLRUVictim(setIndex);
        loadBlockToCache(address);
        setAssociativeCache[setIndex][victimIndex].data[offset] = value;
        setAssociativeCache[setIndex][victimIndex].dirty = true;
    }
}

void Memory::printCacheState() const
{
    std::cerr << "________Current Cache State________" << std::endl;
    if (enableDirect)
    {
        for (size_t i = 0; i < directCache.size(); ++i)
        {
            const CacheBlock &block = directCache[i];
            std::cerr << "Index " << i << ": ";
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
    else
    {
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
}
