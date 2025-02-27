#include "Memory.hpp"
#include <cmath>
#include <bit>
#include <sstream>

Memory::Memory(uint64_t memorySize,
               int64_t input_addr,
               int64_t output_addr,
               size_t l1Lines,
               size_t l2Lines,
               size_t lineSize,
               size_t l2Associativity)
    : memorySize(memorySize),
      lineSize(lineSize),
      wordsPerLine(lineSize / sizeof(int32_t)),
      l1Lines(l1Lines),
      l2Lines(l2Lines),
      l2Associativity(l2Associativity),
      l2Sets(l2Lines / l2Associativity),
      input_addr(input_addr),
      output_addr(output_addr)
{
    // 各種ビット数の計算
    offsetBits = static_cast<size_t>(std::log2(lineSize));
    // Memory.cpp コンストラクタ内で
    l1IndexBits = static_cast<size_t>(std::log2(l1Lines));
    l2IndexBits = static_cast<size_t>(std::log2(l2Lines / l2Associativity)); // 修正点

    // 計算結果の確認
    if (((size_t)1 << l1IndexBits) != l1Lines)
    {
        std::cerr << "Warning: L1 lines (" << l1Lines << ") is not a power of 2" << std::endl;
    }
    if (((size_t)1 << l2IndexBits) != l2Sets)
    {
        std::cerr << "Warning: L2 sets (" << l2Sets << ") is not a power of 2" << std::endl;
    }

    // L1キャッシュの初期化
    l1Cache.resize(l1Lines, CacheBlock{false, false, 0, std::vector<int32_t>(wordsPerLine)});

    // L2キャッシュの初期化
    l2Cache.resize(l2Sets, std::vector<CacheBlock>(l2Associativity, CacheBlock{false, false, 0, std::vector<int32_t>(wordsPerLine)}));
    l2LruOrder.resize(l2Sets, std::vector<size_t>(l2Associativity));
    l2IsFirstAccess.resize(l2Sets, std::vector<bool>(l2Associativity, true));

    for (auto &setOrder : l2LruOrder)
    {
        std::iota(setOrder.begin(), setOrder.end(), 0);
    }

    mainMemory.resize(memorySize >> 2, 0);
    isInitialized.resize(memorySize >> 2, 0);

    // キャッシュ設定の出力
    std::cerr << "Cache Configuration:" << std::endl;
    std::cerr << "  L1 Cache: " << l1Lines << " lines x " << lineSize << " bytes (Direct Mapped)" << std::endl;
    std::cerr << "  L2 Cache: " << l2Sets << " sets x " << l2Associativity << " ways x " << lineSize << " bytes" << std::endl;
    std::cerr << "  Total Cache Size: " << ((l1Lines * lineSize) + (l2Lines * lineSize)) / 1024 << " KiB" << std::endl;
}

void Memory::writeBackL2ToMain(uint32_t l2Index, uint32_t wayIndex)
{
    CacheBlock &block = l2Cache[l2Index][wayIndex];
    if (block.valid && block.dirty) [[unlikely]]
    {
        uint32_t baseAddress = (block.tag << (l2IndexBits + offsetBits)) | (l2Index << offsetBits);
        for (size_t i = 0; i < block.data.size(); ++i)
        {
            mainMemory[(baseAddress >> 2) + i] = block.data[i];
        }
        block.dirty = false;
        if (availableLog) [[unlikely]]
        {
            std::cerr << "Write-back occurred from L2 to main memory for set index " << l2Index << " and way " << wayIndex << std::endl;
        }
    }
}

void Memory::writeBackL1ToL2(const CacheBlock &l1Block, uint32_t l1Index)
{
    if (l1Block.valid && l1Block.dirty) [[unlikely]]
    {
        // L1ブロックのアドレスを計算
        uint32_t address = (l1Block.tag << (l1IndexBits + offsetBits)) | (l1Index << offsetBits);
        uint32_t l2Tag = getTag(address, false);
        uint32_t l2Index = getL2SetIndex(address);

        // L2で対応するブロックを探す
        bool found = false;
        size_t wayIndex = 0;

        for (size_t i = 0; i < l2Associativity; ++i)
        {
            if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
            {
                wayIndex = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            // L2にブロックが存在しない場合、L2の新しいブロックとして追加
            wayIndex = findL2LRUVictim(l2Index);
            writeBackL2ToMain(l2Index, wayIndex);

            l2Cache[l2Index][wayIndex].valid = true;
            l2Cache[l2Index][wayIndex].tag = l2Tag;
        }

        // L1のデータをL2にコピー
        l2Cache[l2Index][wayIndex].data = l1Block.data;
        l2Cache[l2Index][wayIndex].dirty = true;

        // LRUを更新
        updateL2LRU(l2Index, wayIndex);

        if (availableLog) [[unlikely]]
        {
            std::cerr << "Write-back occurred from L1 to L2 for index " << l1Index << " to L2 set index " << l2Index << ", way " << wayIndex << std::endl;
        }
    }
}

void Memory::writeBackL2ToL1(uint32_t address, uint32_t l1Index, const CacheBlock &l2Block)
{
    if (l1Index >= l1Lines)
    {
        std::stringstream ss;
        ss << "Error: Invalid L1 index " << l1Index << " (max " << l1Lines << ")" << std::endl;
        throw std::out_of_range(ss.str() + "Invalid L1 cache index");
    }
    CacheBlock &l1Block = l1Cache[l1Index];

    // 現在のL1ブロックがdirtyなら先にL2に書き戻す
    if (l1Block.valid && l1Block.dirty)
    {
        writeBackL1ToL2(l1Block, l1Index);
    }

    // L2のデータをL1にコピー
    l1Block.valid = true;
    l1Block.dirty = false;
    l1Block.tag = getTag(address, true);

    for (size_t i = 0; i < wordsPerLine; i++)
    {
        l1Block.data[i] = l2Block.data[i];
    }

    if (availableLog) [[unlikely]]
    {
        std::cerr << "Block copied from L2 to L1 cache at index " << l1Index << std::endl;
    }
}
void Memory::loadBlockToL1Cache(uint32_t address)
{
    // uint32_t l1Tag = getTag(address, true);
    uint32_t l1Index = getL1Index(address);

    uint32_t l2Tag = getTag(address, false);
    uint32_t l2Index = getL2SetIndex(address);
    bool l2Hit = false;
    size_t l2WayIndex = 0;

    for (size_t i = 0; i < l2Associativity; ++i)
    {
        if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
        {
            l2WayIndex = i;
            l2Hit = true;
            break;
        }
    }

    if (l2Hit)
    {
        ++l2HitCount;

        const CacheBlock &l2Block = l2Cache[l2Index][l2WayIndex];
        writeBackL2ToL1(address, l1Index, l2Block);

        updateL2LRU(l2Index, l2WayIndex);
    }
    else
    {
        ++missCount;

        loadBlockToL2Cache(address);

        size_t correctWayIndex = 0;
        bool found = false;
        for (size_t i = 0; i < l2Associativity; ++i)
        {
            if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
            {
                correctWayIndex = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            std::cerr << "Error: Could not find the loaded L2 block!" << std::endl;
            throw std::runtime_error("L2 block not found after loading");
        }

        writeBackL2ToL1(address, l1Index, l2Cache[l2Index][correctWayIndex]);
    }
}

void Memory::loadBlockToL2Cache(uint32_t address)
{
    uint32_t l2Tag = getTag(address, false);
    uint32_t l2Index = getL2SetIndex(address);

    size_t victimIndex = findL2LRUVictim(l2Index);
    CacheBlock &block = l2Cache[l2Index][victimIndex];
    if (block.valid && block.dirty) [[unlikely]]
    {
        uint32_t oldAddress = (block.tag << (l2IndexBits + offsetBits)) | (l2Index << offsetBits);
        uint32_t oldRange = (oldAddress >> 22) & 0x7;
        uint32_t newRange = (address >> 22) & 0x7;

        if (oldRange != newRange)
        {
            ++diffRangeWbCount;
        }
        else
        {
            ++sameRangeWbCount;
        }
    }
    else
    {
        ++nonWbCount;
    }

    writeBackL2ToMain(l2Index, victimIndex);

    block.tag = l2Tag;
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

    updateL2LRU(l2Index, victimIndex);

    if (availableLog) [[unlikely]]
    {
        std::cerr << "Cache miss: Loaded block to L2 cache at set index " << l2Index << " and victim way " << victimIndex << std::endl;
    }
}

void Memory::updateL2LRU(uint32_t setIndex, size_t blockIndex)
{
    auto &order = l2LruOrder[setIndex];
    auto it = std::find(order.begin(), order.end(), blockIndex);
    if (it == order.end()) [[unlikely]]
    {
        std::cerr << "Error: Block index not found in L2 LRU order." << std::endl;
        throw std::logic_error("LRU update failed");
    }
    else if (it != order.end())
    {
        order.erase(it);
        order.emplace_back(blockIndex);
    }
}

int32_t Memory::loadWord(uint32_t address, bool toInt)
{
    if (address < 0 || address >= memorySize) [[unlikely]]
    {
        std::stringstream ss;
        ss << "Error: dMemory access out of bounds at address "
           << std::hex << address << std::dec
           << " (word address " << std::hex << (address >> 2) << std::dec << ")" << std::endl;
        ss << "Memory size is " << std::hex << memorySize << std::dec << " bytes" << std::endl;
        throw std::out_of_range(ss.str() + "dMemory access out of bounds");
    }
    if (!isInitialized[address >> 2] && address != input_addr) [[unlikely]]
    {
        throw std::out_of_range("Access to uninitialized dMemory part");
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
                if (toInt)
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

    // L1キャッシュアクセス
    uint32_t l1Tag = getTag(address, true);
    uint32_t l1Index = getL1Index(address);
    uint32_t offset = getOffset(address);

    CacheBlock &l1Block = l1Cache[l1Index];
    if (l1Block.valid && l1Block.tag == l1Tag)
    {
        // L1キャッシュヒット
        ++l1HitCount;

        uint32_t byteOffset = address & ((1 << offsetBits) - 1);
        uint32_t wordOffset = byteOffset / sizeof(int32_t);

        if (offset >= wordsPerLine && address)
        {
            std::stringstream ss;
            ss << "Error: Word offset " << wordOffset
               << " exceeds line size " << wordsPerLine
               << " for address " << std::hex << address << std::dec << std::endl;
            throw std::runtime_error(ss.str());
        }

        int32_t value = l1Block.data[offset];

        // ログ出力
        if (availableLog)
        {
            std::cerr << "L1 cache hit for address " << std::hex << address
                      << " tag=" << l1Tag << " index=" << l1Index
                      << " offset=" << (address & ((1 << offsetBits) - 1))
                      << " word_offset=" << offset << std::dec << std::endl;
            std::cerr << "  Returning value: " << std::hex << value << std::dec << std::endl;
        }

        return value;
    }

    // L1ミス - L2チェック
    loadBlockToL1Cache(address);
    return l1Cache[l1Index].data[offset];
}

void Memory::storeWord(uint32_t address, int32_t value)
{
    if (address < 0 || address >= memorySize) [[unlikely]]
    {
        std::stringstream ss;
        ss << "Error: dMemory access out of bounds at address "
           << std::hex << address << std::dec
           << " (word address " << std::hex << (address >> 2) << std::dec << ")" << std::endl;
        ss << "Memory size is " << std::hex << memorySize << std::dec << " bytes" << std::endl;
        throw std::out_of_range(ss.str() + "dMemory access out of bounds");
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

    uint32_t l1Tag = getTag(address, true);
    uint32_t l1Index = getL1Index(address);
    uint32_t offset = getOffset(address);

    CacheBlock &l1Block = l1Cache[l1Index];
    if (l1Block.valid && l1Block.tag == l1Tag)
    {
        // L1キャッシュヒット
        ++l1HitCount;
        l1Block.data[offset] = value;
        l1Block.dirty = true;
        // ログ出力
        if (availableLog)
        {
            std::cerr << "L1 cache hit for address " << std::hex << address
                      << " tag=" << l1Tag << " index=" << l1Index
                      << " offset=" << (address & ((1 << offsetBits) - 1))
                      << " word_offset=" << offset << std::dec << std::endl;
            std::cerr << "  Storing value: " << std::hex << value << std::dec << std::endl;
        }
        return;
    }

    uint32_t l2Tag = getTag(address, false);
    uint32_t l2Index = getL2SetIndex(address);
    bool l2Hit = false;
    size_t l2WayIndex = 0;

    for (size_t i = 0; i < l2Associativity; ++i)
    {
        if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
        {
            l2WayIndex = i;
            l2Hit = true;
            break;
        }
    }

    if (l2Hit)
    {
        ++l2HitCount;

        // L1ブロックの置き換え
        if (l1Block.valid && l1Block.dirty)
        {
            writeBackL1ToL2(l1Block, l1Index);
        }

        // L2からL1にブロックをコピー
        writeBackL2ToL1(address, l1Index, l2Cache[l2Index][l2WayIndex]);
        l1Block.data[offset] = value;
        l1Block.dirty = true;
        updateL2LRU(l2Index, l2WayIndex);
    }
    else
    {
        // L2にもない - メインメモリからロード
        ++missCount;
        loadBlockToL2Cache(address);

        uint32_t l2Tag = getTag(address, false);
        uint32_t l2Index = getL2SetIndex(address);

        size_t correctWayIndex = 0;
        bool found = false;
        for (size_t i = 0; i < l2Associativity; ++i)
        {
            if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
            {
                correctWayIndex = i;
                found = true;
                break;
            }
        }
        if (!found)
        {
            throw std::runtime_error("L2 block not found after loading");
        }
        // L1に必要なブロックをL2からコピー
        writeBackL2ToL1(address, l1Index, l2Cache[l2Index][correctWayIndex]);
        l1Block.data[offset] = value;
        l1Block.dirty = true;
    }
}

bool Memory::checkCacheHit(uint32_t address)
{
    if (address < 0 || address >= memorySize) [[unlikely]]
    {
        throw std::out_of_range("dMemory access out of bounds");
    }

    // 特別なアドレス（入出力）は常にヒット
    if (address == input_addr || address == output_addr)
    {
        stallCycles = 1;
        return true;
    }

    // キャッシュが無効の場合も常にヒット扱い
    if (!availableCache)
    {
        stallCycles = 1;
        return true;
    }

    uint32_t l1Tag = getTag(address, true);
    uint32_t l1Index = getL1Index(address);

    const CacheBlock &l1Block = l1Cache[l1Index];
    if (l1Block.valid && l1Block.tag == l1Tag)
    {
        // L1キャッシュヒット
        stallCycles = 1;
        return true;
    }

    // L2キャッシュチェック
    uint32_t l2Tag = getTag(address, false);
    uint32_t l2Index = getL2SetIndex(address);
    bool l2Hit = false;

    for (size_t i = 0; i < l2Associativity; ++i)
    {
        if (l2Cache[l2Index][i].valid && l2Cache[l2Index][i].tag == l2Tag)
        {
            l2Hit = true;
            break;
        }
    }

    if (l2Hit)
    {
        stallCycles = 7;
        return true;
    }

    // L2キャッシュミス - DRAM アクセス
    size_t victimIndex = findL2LRUVictim(l2Index);
    const CacheBlock &block = l2Cache[l2Index][victimIndex];

    if (l2IsFirstAccess[l2Index][victimIndex])
    {
        // 初回参照ミス
        l2IsFirstAccess[l2Index][victimIndex] = false;
        stallCycles = 2;
    }
    // ライトバック必要性の判断
    else if (block.valid && block.dirty) [[unlikely]]
    {
        uint32_t oldAddress = (block.tag << (l2IndexBits + offsetBits)) | (l2Index << offsetBits);
        uint32_t oldRange = (oldAddress >> 22) & 0x7;
        uint32_t newRange = (address >> 22) & 0x7;

        if (oldRange != newRange)
        {
            stallCycles = 90;
        }
        else
        {
            stallCycles = 90;
        }
    }
    else
    {
        stallCycles = 90;
    }

    return false;
}

void Memory::printCacheState() const
{
    std::cerr << "________Current Cache State________" << std::endl;

    // キャッシュ設定表示
    std::cerr << "Cache Configuration:" << std::endl;
    std::cerr << "  L1 Cache: " << l1Lines << " lines x " << lineSize << " bytes (Direct Mapped)" << std::endl;
    std::cerr << "  L2 Cache: " << l2Sets << " sets x " << l2Associativity << " ways x " << lineSize << " bytes" << std::endl;
    std::cerr << "  Total Cache Size: " << ((l1Lines * lineSize) + (l2Lines * lineSize)) / 1024 << " KiB" << std::endl;

    // L1キャッシュ状態表示
    std::cerr << "L1 Cache State (showing up to 10 valid entries):" << std::endl;
    int displayCount = 0;
    for (size_t i = 0; i < l1Cache.size(); ++i)
    {
        const CacheBlock &block = l1Cache[i];
        if (block.valid)
        {
            std::cerr << "  Index " << i << ": ";
            std::cerr << "[Tag: " << block.tag << ", Dirty: " << (block.dirty ? "Yes" : "No") << "] Data: ";
            for (size_t j = 0; j < std::min<size_t>(4, block.data.size()); ++j)
            {
                std::cerr << block.data[j] << " ";
            }
            std::cerr << std::endl;
            ++displayCount;
            if (displayCount >= 10)
                break;
        }
    }

    // L2キャッシュ状態表示
    std::cerr << "L2 Cache State (showing up to 5 sets with valid entries):" << std::endl;
    displayCount = 0;
    for (size_t i = 0; i < l2Cache.size(); ++i)
    {
        bool showSet = false;
        for (size_t j = 0; j < l2Associativity; ++j)
        {
            if (l2Cache[i][j].valid)
            {
                showSet = true;
                break;
            }
        }

        if (showSet)
        {
            std::cerr << "  Set Index " << i << ":" << std::endl;
            for (size_t j = 0; j < l2Associativity; ++j)
            {
                const CacheBlock &block = l2Cache[i][j];
                if (block.valid)
                {
                    std::cerr << "    Way " << j << ": ";
                    std::cerr << "[Tag: " << block.tag << ", Dirty: " << (block.dirty ? "Yes" : "No") << "] Data: ";
                    for (size_t k = 0; k < std::min<size_t>(4, block.data.size()); ++k)
                    {
                        std::cerr << block.data[k] << " ";
                    }
                    std::cerr << std::endl;
                }
            }
            ++displayCount;
            if (displayCount >= 5)
                break;
        }
    }

    // キャッシュ統計表示
    std::cerr << "Cache Statistics:" << std::endl;
    std::cerr << "  L1 Hits: " << l1HitCount << std::endl;
    std::cerr << "  L2 Hits: " << l2HitCount << std::endl;
    std::cerr << "  Misses: " << missCount << std::endl;
    double totalAccesses = l1HitCount + l2HitCount + missCount;
    if (totalAccesses > 0)
    {
        std::cerr << "  Total Hit Rate: " << (double)(l1HitCount + l2HitCount) / totalAccesses << std::endl;
        std::cerr << "  L1 Hit Rate: " << (double)l1HitCount / totalAccesses << std::endl;
        if (l2HitCount + missCount > 0)
        {
            std::cerr << "  L2 Hit Rate: " << (double)l2HitCount / (double)(l2HitCount + missCount) << " (of L1 misses)" << std::endl;
        }
    }
}