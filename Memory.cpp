#include "Memory.hpp"
#include <cmath>
#include <bit>

Memory::Memory()
{

    offsetBits = static_cast<size_t>(std::log2(BLOCK_SIZE));
    indexBits = static_cast<size_t>(std::log2(numBlocks));
    for (int i = 0; i < numBlocks; ++i)
    {
        cache[i] = CacheBlock{false, false, 0, {}};
    }
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
        for (size_t i = 0; i < BLOCK_SIZE/4; ++i)
        {
            mainMemory[baseAddress / 4 + i] = cache[index].data[i];
        }
        cache[index].dirty = false;
        std::cerr << "Write-back occurred for index 0x" << std::hex << index << std::dec << std::endl;
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

    for (size_t i = 0; i < BLOCK_SIZE / 4; ++i)
    {
        cache[index].data[i] = mainMemory[baseAddress / 4 + i];
    }
    std::cerr << "Cache miss: Loaded block to cache at index 0x" << std::hex << index << " with tag 0x" << tag << std::dec << std::endl;
}

int32_t Memory::loadWord(uint32_t address, bool isLw)
{
    if (address < 0 || address >= DMEMORY_SIZE)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    if (address == INPUT_ADDRESS || address == OUTPUT_ADDRESS)
    {
        auto temp = mainMemory[address / 4];
        if (address == INPUT_ADDRESS)
        {
            std::cerr << "Input requested at INPUT_ADDRESS (0x" << std::hex << address << ")" << std::dec << std::endl;

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
                std::cerr << "Input: 0x" << std::hex << temp << std::dec << std::endl;
                storeWord(INPUT_ADDRESS, temp);
                inputIndex++;
            }
        }
        return temp;
    }

    uint32_t index = getIndex(address);
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (cache[index].valid && cache[index].tag == tag)
    {
        std::cerr << "Cache hit at index 0x" << std::hex << index << " for address 0x" << address << std::dec << std::endl;
        return cache[index].data[offset];
    }
    else
    {
        std::cerr << "Cache miss at index 0x" << std::hex << index << " for address 0x" << address << std::dec << std::endl;
        loadBlockToCache(address);
        return cache[index].data[offset];
    }
}

void Memory::storeWord(uint32_t address, int32_t value)
{
    if (address < 0 || address >= DMEMORY_SIZE)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }

    if (address == INPUT_ADDRESS || address == OUTPUT_ADDRESS)
    {
        if (address == OUTPUT_ADDRESS)
        {
            std::cerr << "Output written at OUTPUT_ADDRESS (0x" << std::hex << address << "): " << value << std::dec << std::endl;
            std::cerr << "Output: 0x" << std::hex << mainMemory[address / 4] << std::dec << std::endl;
            output.emplace_back(value);
        }
        mainMemory[address / 4] = value;
        return;
    }

    uint32_t index = getIndex(address);
    uint32_t tag = getTag(address);
    uint32_t offset = getOffset(address) / sizeof(int32_t);

    if (cache[index].valid && cache[index].tag == tag)
    {
        std::cerr << "Cache hit at index 0x" << std::hex << index << " for address 0x" << address << std::dec << std::endl;
        cache[index].data[offset] = value;
        cache[index].dirty = true;
    }
    else
    {
        std::cerr << "Cache miss at index 0x" << std::hex << index << " for address 0x" << address << std::dec << std::endl;
        loadBlockToCache(address);
        cache[index].data[offset] = value;
        cache[index].dirty = true;
    }
}

void Memory::printCache() const
{
    std::cerr << "Current Cache State:" << std::endl;
    for (size_t i = 0; i < numBlocks; ++i)
    {
        std::cerr << "Index 0x" << std::hex << i << std::dec << ": ";
        if (cache[i].valid)
        {
            std::cerr << std::hex << "[Tag: 0x" << cache[i].tag << ", Dirty: " << (cache[i].dirty ? "Yes" : "No") << "] Data: ";
            for (const auto &word : cache[i].data)
            {
                std::cerr << "0x" << word << " ";
            }
        }
        else
        {
            std::cerr << "Invalid";
        }
        std::cerr << std::dec << std::endl;
    }
}
