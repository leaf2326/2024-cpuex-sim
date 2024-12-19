#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include "Log.hpp"
#include "Util.hpp"
#include "OptionHandler.hpp"
#include "FPU.hpp"
#include "Memory.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>
#include <sstream>

#define NULLREG -1
#define NOLOADREG -2 // NOLOADREG != NULLREG
#define ALLREG 0
#define PC 1
#define REG 2
#define FPREG 3

class Simulator : public Log
{
public:
    Simulator(OptionHandler &op);
    uint64_t DMEMORY_SIZE;

    void loadMemoryFromBinary(const std::string &programFilePath);
    void loadInputData(const std::string &inputFilePath);
    void runProgram(int outputRegNum);
    int64_t getStep() const;
    int32_t getPC() const;
    void printRegisters(int regType) const;
    // ログの出力
    void printLog();

private:
    uint64_t maxStep = UINT64_MAX;
    uint64_t step = 0;
    static constexpr int REG_COUNT = 32;
    static constexpr int FPREG_COUNT = 32;
    static constexpr int64_t IMEMORY_SIZE = 512 * 1024; // Iメモリサイズ（512KiB）
    static constexpr int64_t CACHE_SIZE = 1024 * 16;
    static constexpr int64_t BLOCK_SIZE = 16;
    static constexpr int64_t INPUT_ADDRESS = 100;
    static constexpr int64_t OUTPUT_ADDRESS = 104;
    static constexpr double CPUFREQUENCY = 16000000;

    static constexpr int NUM_ENTRIES = 128; // 2^7エントリ
    static constexpr int PHT_DEFAULT = 2;
    std::vector<uint8_t> patternHistoryTable;  // 2-bit飽和カウンタ
    
    bool isBreakpoint;
    bool currentInstIsLoadOrStore = false;
    bool prevInstIsLoadOrStore = false;
    uint64_t loadStoreSequence = 0;
    uint64_t hazardRAW = 0;

    uint64_t mviCount = 0;
    uint64_t lwNegativeCount = 0;
    uint64_t lwNonNegativeCount = 0;
    uint64_t swNegativeCount = 0;
    uint64_t swNonNegativeCount = 0;

    FPU fpu;
    std::array<int32_t, REG_COUNT> registers{};
    std::array<int32_t, FPREG_COUNT> fpRegisters{};
    int32_t pc;
    std::array<int32_t, IMEMORY_SIZE / 4> iMemory{};
    int instructionSize = 0;
    Memory dMemory;
    uint32_t dataSectionSize = 0;

    // 前の命令で書き込んだレジスタ
    int prevLoadReg = NULLREG;
    uint32_t output_num;

    bool enableCache;
    bool enableICount;
    bool enableIStats;
    bool enableDebug;
    bool enableGDB;

    bool availableLog = false;

    int32_t getRegister(int reg) const;
    void setRegister(int reg, int32_t value);
    int32_t getFpRegister(int fpreg) const;
    void setFpRegister(int fpreg, int32_t fpvalue);

    void setPC(int32_t newPC);

    int32_t loadWord(int32_t address);
    int32_t loadInstruction(int32_t adsdress) const;
    void storeWord(int32_t address, int32_t value);
    void storeInstruction(int32_t address, int32_t instruction);

    std::string instToString(uint32_t instruction) const;

    // 直近に書き込んだレジスタの更新
    void updatePrevLoadReg(int currLoadReg);

    // 一つ前のロード命令でロードしたレジスタであるかを検出
    void detectPrevLoad(int32_t rs1, int32_t rs2);


    // 分岐予測
    uint32_t getIndex(uint32_t pc) const;   // patternHistoryTableのindexを計算
    void updateCounter(uint8_t &counter, bool isTaken);
    bool predict(uint8_t counter) const;
    void branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken);

    // 命令出力
    void printInstruction(uint32_t instruction) const;
    // 命令実行
    void executeInstruction(uint32_t instruction);

    void printInstAddrCounts();
    void printInstStats() const;
    void printProgram(bool aroundPC) const noexcept;
    void printCacheHitMissCounts() const;

    void printOutput() const noexcept;
};

#endif // SIMULATOR_HPP
