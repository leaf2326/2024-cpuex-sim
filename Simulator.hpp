#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include "Log.hpp"
#include "Util.hpp"
#include "Option.hpp"
#include "FPU.hpp"
#include "Memory.hpp"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>
#include <sstream>

#define NULLREG -1
#define NOLOADREG -2 // NOLOADREG != NULLREG

class Simulator : public Log
{
public:
    Simulator(Options op);
    void loadMemoryFromBinary(const std::string &programFilePath);
    void loadInputData(const std::string &inputFilePath);
    void runProgram(int outputRegNum);

private:
    static constexpr int REG_COUNT = 32;
    static constexpr int FPREG_COUNT = 32;
    static constexpr int64_t IMEMORY_SIZE = 512 * 1024;      // Iメモリサイズ（512KiB）
    static constexpr int64_t DMEMORY_SIZE = 4 * 1024 * 1024; // Dメモリサイズ（4MiB）
    static constexpr int64_t CACHE_SIZE = 1024 * 16;
    static constexpr int64_t BLOCK_SIZE = 16;
    static constexpr int64_t INPUT_ADDRESS = 100;
    static constexpr int64_t OUTPUT_ADDRESS = 104;
    bool isBreakpoint;

    FPU fpu;
    std::array<int32_t, REG_COUNT> registers{};
    std::array<int32_t, FPREG_COUNT> fpRegisters{};
    int32_t pc;
    std::array<int32_t, IMEMORY_SIZE / 4> iMemory{};
    int instructionCount = 0;
    Memory dMemory{DMEMORY_SIZE, CACHE_SIZE, BLOCK_SIZE, INPUT_ADDRESS, OUTPUT_ADDRESS};
    uint32_t dataSectionSize = 0;

    // 前の命令で書き込んだレジスタ
    int prevLoadReg = NULLREG;
    Options options;
    uint32_t output_num;

    int32_t getRegister(int reg) const;
    void setRegister(int reg, int32_t value);
    int32_t getFpRegister(int fpreg) const;
    void setFpRegister(int fpreg, int32_t fpvalue);

    int32_t getPC() const;
    void setPC(int32_t newPC);

    int32_t loadWord(int32_t address);
    int32_t loadInstruction(int32_t adsdress) const;
    void storeWord(int32_t address, int32_t value);
    void storeInstruction(int32_t address, int32_t instruction);

    std::string instToString(uint32_t instruction) const;

    void printProgram(bool aroundPC) const noexcept;

    void printRegisters() const;

    // 直近に書き込んだレジスタの更新
    void updatePrevLoadReg(int currLoadReg);

    // 一つ前のロード命令でロードしたレジスタであるかを検出
    void detectPrevLoad(int32_t rs1, int32_t rs2);

    // 分岐予測
    void branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken);

    // 命令出力
    void printInstruction(uint32_t instruction) const;
    // 命令実行
    void executeInstruction(uint32_t instruction);

    // ログの出力
    void printLog();

    void printOutput();
};

#endif // SIMULATOR_HPP
