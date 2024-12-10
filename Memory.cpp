#include "Memory.hpp"
#include <cmath>
#include <bit>

Memory::Memory(uint64_t memorySize, size_t cacheSize, size_t blockSize, int64_t input_addr, int64_t output_addr)
    : memorySize(memorySize), cacheSize(cacheSize), blockSize(blockSize), input_addr(input_addr), output_addr(output_addr)
{
    numBlocks = cacheSize / blockSize;
    offsetBits = static_cast<size_t>(std::log2(blockSize));
    indexBits = static_cast<size_t>(std::log2(numBlocks));
    cache.resize(numBlocks, CacheBlock{false, false, 0, std::vector<int32_t>(blockSize / sizeof(int32_t))});
    mainMemory.resize(memorySize / 4, 0);
    isInitialized.resize(memorySize / 4, 0);
}

uint32_t Memory::getTag(uint32_t address)
{
    return address >> (indexBits + offsetBits);
}

uint32_t Memory::getIndex(uint32_t address)
{
    return (address >> offsetBits) & ((1 << indexBits) - 1);
}

uint32_t Memory::getOffset(uint32_t address)
{
    return address & ((1 << offsetBits) - 1);
}

void Memory::writeBack(uint32_t index)
{
    if (cache[index].valid && cache[index].dirty)
    {
        uint32_t baseAddress = (cache[index].tag << (indexBits + offsetBits)) | (index << offsetBits);
        for (size_t i = 0; i < cache[index].data.size(); ++i)
        {
            mainMemory[baseAddress / 4 + i] = cache[index].data[i];
        }
        cache[index].dirty = false;
        if (availableLog)
        {
            std::cerr << "Write-back occurred for index " << std::hex << index << std::dec << std::endl;
        }
    }
}

void Memory::loadBlockToCache(uint32_t address)
{
    uint32_t index = getIndex(address);
    uint32_t tag = getTag(address);
    writeBack(index);

    cache[index].tag = tag;
    cache[index].valid = true;
    uint32_t baseAddress = address & ~((1 << offsetBits) - 1);

    for (size_t i = 0; i < cache[index].data.size(); ++i)
    {
        cache[index].data[i] = mainMemory[baseAddress / 4 + i];
    }
    if (availableLog)
    {
        std::cerr << "Cache miss: Loaded block to cache at index " << std::hex << index << " with tag " << tag << std::dec << std::endl;
    }
}

int32_t Memory::loadWord(uint32_t address, bool isLw)
{
    if (address < 0 || address >= memorySize)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    if (!isInitialized[address / 4] && address != input_addr)
    {
        throw std::out_of_range("Access to uninitialized dMemory part");
    }
    if (address == input_addr || address == output_addr)
    {
        auto temp = mainMemory[address / 4];
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
        return mainMemory[address / 4];
    }
    uint32_t index = getIndex(address);
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (cache[index].valid && cache[index].tag == tag)
    {
        ++hitCount;
        if (availableLog)
        {
            std::cerr << "Cache hit at index " << std::hex << index << " for address " << address << std::dec << std::endl;
        }
        return cache[index].data[offset];
    }
    else
    {
        ++missCount;
        if (availableLog)
        {
            std::cerr << "Cache miss at index " << std::hex << index << " for address " << address << std::dec << std::endl;
        }
        loadBlockToCache(address);
        return cache[index].data[offset];
    }
}

void Memory::storeWord(uint32_t address, int32_t value)
{
    if (address < 0 || address >= memorySize)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    isInitialized[address / 4] = true;
    if (address == input_addr || address == output_addr)
    {
        if (address == output_addr)
        {
            if (availableLog)
            {
                std::cerr << "Output written at output_addr (" << std::hex << address << "): " << value << std::dec << std::endl;
                std::cerr << "Output: " << std::hex << mainMemory[address / 4] << std::dec << std::endl;
            }
            output.emplace_back(value);
        }
        mainMemory[address / 4] = value;
        return;
    }
    if (!availableCache)
    {
        mainMemory[address / 4] = value;
        return;
    }
    uint32_t index = getIndex(address);
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (cache[index].valid && cache[index].tag == tag)
    {
        ++hitCount;
        if (availableLog)
        {
            std::cerr << "Cache hit at index " << std::hex << index << " for address " << address << std::dec << std::endl;
        }
        cache[index].data[offset] = value;
        cache[index].dirty = true;
    }
    else
    {
        ++missCount;
        if (availableLog)
        {
            std::cerr << "Cache miss at index " << std::hex << index << " for address " << address << std::dec << std::endl;
        }
        loadBlockToCache(address);
        cache[index].data[offset] = value;
        cache[index].dirty = true;
    }
}

uint64_t Memory::getHitCount() const
{
    return hitCount;
}

uint64_t Memory::getMissCount() const
{
    return missCount;
}

void Memory::printCacheState() const
{
    std::cerr << "Current Cache State:" << std::endl;
    for (size_t i = 0; i < cache.size(); ++i)
    {
        std::cerr << "Index " << std::hex << i << std::dec << ": ";
        if (cache[i].valid)
        {
            std::cerr << std::hex << "[Tag: " << cache[i].tag << ", Dirty: " << (cache[i].dirty ? "Yes" : "No") << "] Data: ";
            for (const auto &word : cache[i].data)
            {
                std::cerr << word << " ";
            }
        }
        else
        {
            std::cerr << "Invalid";
        }
        std::cerr << std::dec << std::endl;
    }
}