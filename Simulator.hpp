#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <array>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>

class Simulator
{
private:
    static constexpr int REG_COUNT = 32;
    static constexpr int64_t MEMORY_SIZE = 1024; // メモリサイズ（1KB）
    bool isBreakpoint;
    
    std::array<int64_t, REG_COUNT> registers{};
    int64_t pc;
    std::array<int64_t, MEMORY_SIZE / 4> memory{};

public:
    Simulator();
    ~Simulator();
    
    int64_t getRegister(int reg) const;
    void setRegister(int reg, int64_t value);

    int64_t getPC() const;
    void setPC(int64_t newPC);

    int64_t loadWord(int64_t address) const;
    void storeWord(int64_t address, int64_t value);
    void loadMemoryFromBinary(const std::string &filename);

    void printRegisters() const;

    uint32_t getOpcode(uint32_t instruction) const;
    uint32_t getRd(uint32_t instruction) const;
    uint32_t getFunct3(uint32_t instruction) const;
    uint32_t getRs1(uint32_t instruction) const;
    uint32_t getRs2(uint32_t instruction) const;
    uint32_t getFunct7(uint32_t instruction) const;
    int32_t getImmediate(uint32_t instruction) const;

    void executeInstruction(uint32_t instruction);

    void runProgram();
};

#endif // RISCV_SIMULATOR_HPP
