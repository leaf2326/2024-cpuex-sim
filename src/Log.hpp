#ifndef LOG_HPP
#define LOG_HPP

#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <array>

class Log
{
protected:
    uint64_t totalInstructions = 0;             // 命令の総実行回数
    std::unordered_map<int, int> nStallCount{}; // ストールの回数
    uint64_t branchPredCount = 0;               // 分岐予測の回数
    uint64_t flushCount = 0;                    // パイプラインのフラッシュ回数

    enum InstructionType {
        // NOP
        NOP = 0,

        // ALU命令
        ADD,
        SUB,
        SLLI,
        SRLI,
        ADDI,
        LUI,
        
        // 入出力命令
        INST_IN,
        INST_FIN,
        INST_OUT,
        
        // ジャンプ命令
        BEQ,
        BNE,
        BLT,
        BGE,
        BFLT,
        BFGE,
        JAL,
        JALR,
        
        // メモリ命令
        LW,
        LWR,
        SW,
        FLW,
        FLWR,
        FSW,
        
        // 特別命令
        EBREAK,
        
        // FPU命令
        FTOI,
        ITOF,
        FADD,
        FSUB,
        FMUL,
        FDIV,
        FMV,
        FSQRT,
        FFLOOR,
        FLT,
        FEQ,
        FMADD,
        
        MAX_INSTRUCTION_TYPE
    };

    const std::array<std::string, MAX_INSTRUCTION_TYPE> InstructionTypeNames = {
        "nop", "add", "sub", "slli", "srli", "addi", "lui",
        "in", "fin", "out",
        "beq", "bne", "blt", "bge", "bflt", "bfge", "jal", "jalr",
        "lw", "lwr", "sw", "flw", "flwr", "fsw",
        "ebreak",
        "ftoi", "itof", "fadd","fsub", "fmul", "fdiv", "fmv", "fsqrt", "ffloor", "flt", "feq", "fmadd"
    };

    [[nodiscard]]
    inline std::string typeToString(int type) const noexcept
    {
        if (type >= 0 && type < MAX_INSTRUCTION_TYPE)
        {
            return InstructionTypeNames[type];
        }
        return "UNKNOWN";
    }

    // 命令ごとの実行回数
    std::array<int64_t, MAX_INSTRUCTION_TYPE> instructionCounts;

    uint64_t sumCLKCount();

    std::vector<uint64_t> instAddrCounts;

public:
    inline void logInstAddr(uint32_t address)
    {
        ++instAddrCounts[address];
    }
    inline void logInstruction(int type)
    {
        ++totalInstructions;
        ++instructionCounts[type];
    }

    inline void logFlush()
    {
        ++flushCount;
    }

    inline void logBranchPrediction()
    {
        ++branchPredCount;
    }

    inline void logStall(int stallType)
    {
        ++nStallCount[stallType];
    }

    void printLog();
};

#endif // LOG_HPP
