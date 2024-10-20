#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include "Log.hpp"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>

#define NULLREG -1
#define NOWRITEREG -2 // NOWRITEREG != NULLREG

class Simulator : public Log
{
private:
    static constexpr int REG_COUNT = 32;
    static constexpr int64_t MEMORY_SIZE = 8192; // メモリサイズ（8KB）
    bool isBreakpoint;

    std::array<int32_t, REG_COUNT> registers{};
    int32_t pc;
    std::array<int32_t, MEMORY_SIZE / 4> memory{};

    // 前の命令で書き込んだレジスタ
    int prevWriteReg = NULLREG;
    int prevPrevWriteReg = NULLREG;

public:
    Simulator();
    ~Simulator();

    int32_t getRegister(int reg) const;
    void setRegister(int reg, int32_t value);

    int32_t getPC() const;
    void setPC(int32_t newPC);

    int32_t loadWord(int32_t address) const;
    void storeWord(int32_t address, int32_t value);

    std::string instToString(uint32_t instruction);

    void loadMemoryFromBinary(const std::string &filename);

    void printRegisters() const;

    uint32_t getOpcode(uint32_t instruction) const;
    uint32_t getRd(uint32_t instruction) const;
    uint32_t getFunct3(uint32_t instruction) const;
    uint32_t getRs1(uint32_t instruction) const;
    uint32_t getRs2(uint32_t instruction) const;
    uint32_t getFunct7(uint32_t instruction) const;
    int32_t getImmediate(uint32_t instruction) const;

    // 直近に書き込んだレジスタの更新
    void updateWriteReg(int currWriteReg);

    // データハザード検出
    void detectDataHazard(int32_t rs1, int32_t rs2);

    // 分岐予測
    void branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken);

    // 命令実行
    void executeInstruction(uint32_t instruction);

    // ログの出力
    void printLog() const;

    void runProgram();
};

#endif // SIMULATOR_HPP
