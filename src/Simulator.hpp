#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include "Log.hpp"
#include "Util.hpp"
#include "OptionHandler.hpp"
#include "FPU.hpp"
#include "Memory.hpp"
#include "Predictor.hpp"
#include "InstructionCache.hpp"
#include <array>
#include <vector>
#include <cstdint>
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

class Pipeline;

class Simulator : public Log
{
public:
    Simulator(OptionHandler &op);
    ~Simulator();

    Simulator(const Simulator &) = delete;
    Simulator &operator=(const Simulator &) = delete;

    uint64_t DMEMORY_SIZE;

    void loadMemoryFromBinary(const std::string &programFilePath);
    void loadInputData(const std::string &inputFilePath);
    void runProgram(int outputRegNum);

    [[nodiscard]]
    inline int64_t getStep() const noexcept
    {
        return step;
    }
    [[nodiscard]]
    inline int32_t getPC() const noexcept
    {
        return pc;
    }
    void printRegisters(int regType) const;
    // ログの出力
    void printLog();
    void printPipelineLog();
    void printNonPipelineLog();

    // 分岐予測ミスとキャッシュミスの検出
    bool branchMispredicted = false;
    bool instructionCacheMiss = false;

    // パイプライン関連のフレンド宣言
    friend class Pipeline;

    // パイプライン内での命令実行用
    // パイプライン内での命令実行用に引数を追加
    void executeInstructionInPipeline(uint32_t instruction, int32_t pc, int32_t rs1Value = 0, int32_t rs2Value = 0);

    [[nodiscard]]
    inline int32_t getRegister(int reg) const
    {
        if (reg < 0 || reg >= REG_COUNT)
        {
            throw std::out_of_range("Invalid register index");
        }
        return reg == 0 ? 0 : registers[reg];
    }

    [[nodiscard]]
    inline int32_t getFpRegister(int fpreg) const
    {
        if (fpreg < 0 || fpreg >= FPREG_COUNT)
        {
            throw std::out_of_range("Invalid fpregister index");
        }
        return fpRegisters[fpreg];
    }

    inline void setRegister(int reg, int32_t value)
    {
        if (reg == 0)
            return;
        if (reg < 0 || reg >= REG_COUNT)
        {
            throw std::out_of_range("Invalid register index");
        }
        if (availableLog)
            std::cerr << "Register x" << reg << " changed from " << std::hex << registers[reg] << " to " << value << std::dec << std::endl;

        registers[reg] = value;
        if (registers[2] <= registers[3])
        {
            throw std::out_of_range("Stack overflow! sp=" + std::to_string(registers[2]) + " hp=" + std::to_string(registers[3]));
        }
        // printRegisters(ALLREG);
    }

    inline void setFpRegister(int fpreg, int32_t fpvalue)
    {
        if (fpreg < 0 || fpreg >= FPREG_COUNT)
        {
            throw std::out_of_range("Invalid fpregister index");
        }
        if (availableLog)
            std::cerr << "fpRegister fp" << fpreg << " changed from " << std::hex << fpRegisters[fpreg] << " to " << fpvalue << std::dec << std::endl;

        fpRegisters[fpreg] = fpvalue;
        // printRegisters(ALLREG);
    }

    [[nodiscard]]
    inline int32_t loadWord(uint32_t address, bool isLw)
    {
        return dMemory.loadWord(address, isLw);
    }

    inline void storeWord(uint32_t address, int32_t value)
    {
        dMemory.storeWord(address, value);
    }

    // パイプライン用のヘルパーメソッド
    [[nodiscard]] inline bool isLogEnabled() const { return availableLog; }
    bool simulateCacheAccess(int32_t address, bool isStore);
    int getCacheMissPenalty() const;

    inline void setBreakpoint(bool bp) { isBreakpoint = bp; }

private:
    Pipeline *pipeline = nullptr;
    // パイプラインサポート
    bool enablePipeline = false;

    // パイプラインモード用のメソッド
    void runPipelineProgram(int outputRegNum);
    void runPipelineProgramGDB(int outputRegNum);
    void runPipelineProgramNormal(int outputRegNum);
    void finishPipelineExecution(uint64_t &cycleCount);

    uint64_t maxStep = UINT64_MAX;
    uint64_t step = 0;
    static constexpr int REG_COUNT = 64;
    static constexpr int FPREG_COUNT = 64;
    static constexpr int64_t CACHE_SIZE = 16 * 1024;
    static constexpr int64_t BLOCK_SIZE = 16;
    static constexpr int64_t INPUT_ADDRESS = 25;
    static constexpr int64_t OUTPUT_ADDRESS = 26;
    static constexpr double CPUFREQUENCY = 100000000;
    uint64_t outputSize;
    std::string outputFilePath;

    GSharePredictor predictor;

    bool isBreakpoint;
    bool currentInstIsLoadOrStore = false;
    bool prevInstIsLoadOrStore = false;
    uint64_t loadStoreSequence = 0;
    uint64_t hazardRAW = 0;

    uint64_t mvCount = 0;
    uint64_t mviCount = 0;
    uint64_t flwNegativeCount = 0;
    uint64_t flwNonNegativeCount = 0;
    uint64_t fswNegativeCount = 0;
    uint64_t fswNonNegativeCount = 0;
    uint64_t lwNegativeCount = 0;
    uint64_t lwNonNegativeCount = 0;
    uint64_t swNegativeCount = 0;
    uint64_t swNonNegativeCount = 0;
    uint64_t flwImmCount = 0;
    uint64_t fhalfCount = 0;

    FPU fpu;
    std::array<int32_t, REG_COUNT> registers{};
    std::array<int32_t, FPREG_COUNT> fpRegisters{};
    int32_t pc;
    InstructionCache iCache;
    static constexpr int64_t IMEMORY_SIZE = InstructionCache::IMEMORY_SIZE; // Iメモリサイズ（128KiB）
    bool fetchInstruction(int32_t address);
    uint64_t getInstructionCacheMissCount() const { return iCache.getMissCount(); }

    std::array<uint32_t, IMEMORY_SIZE / 4> iMemory{};
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
    bool enableStdout;
    bool enableGDB;
    bool enableICache;

    bool availableLog = false;

    inline void setPC(int32_t newPC) noexcept
    {
        if (availableLog)
            std::cerr << "PC changed from " << std::hex << pc << " to " << newPC << std::dec << std::endl;

        pc = newPC;
        // printRegisters(ALLREG)
    }

    [[nodiscard]]
    inline int32_t loadInstruction(int32_t address) const
    {
        if (address < 0 || address >= IMEMORY_SIZE >> 2)
        {
            throw std::out_of_range("iMemory access out of bounds");
        }
        return iMemory[address];
    }

    void storeInstruction(int32_t address, int32_t instruction);

    std::string instToString(uint32_t instruction) const;

    // 直近に書き込んだレジスタの更新
    inline void updatePrevLoadReg(int currLoadReg)
    {
        prevLoadReg = currLoadReg;
    }

    // 一つ前のロード命令でロードしたレジスタであるかを検出
    inline void detectPrevLoad(int32_t rs1, int32_t rs2)
    {
        // 1命令前にロードしたレジスタであるかを検出
        if (rs1 == prevLoadReg || rs2 == prevLoadReg) [[unlikely]]
        {
            ++hazardRAW;
        }
    }
    void branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken);

    // 命令出力
    void printInstruction(uint32_t instruction) const;
    // 命令実行
    void executeInstruction(uint32_t instruction);

    void printInstAddrCounts();
    void printInstStats() const;
    void printProgram(bool aroundPC) const noexcept;
    void printCacheHitMissCounts() const;

    void printOutput();
};

#endif // SIMULATOR_HPP
