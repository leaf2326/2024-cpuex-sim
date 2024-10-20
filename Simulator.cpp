#include "Simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#define MAXCLK 100000

Simulator::Simulator()
{
    registers[0] = 0;                   // x0
    registers[2] = MEMORY_SIZE / 4 - 1; // sp
    pc = 0;
    isBreakpoint = false;
}

Simulator::~Simulator()
{
}

int32_t Simulator::getRegister(int reg) const
{
    if (reg < 0 || reg >= REG_COUNT)
    {
        throw std::out_of_range("Invalid register index");
    }
    return reg == 0 ? 0 : registers[reg];
}

void Simulator::setRegister(int reg, int32_t value)
{
    if (reg == 0)
        return;
    if (reg < 0 || reg >= REG_COUNT)
    {
        throw std::out_of_range("Invalid register index");
    }
    std::cout << "Register x" << reg << " changed from 0x" << std::hex << registers[reg] << " to 0x" << value << std::dec << std::endl;
    registers[reg] = value;
    // printRegisters();
}

int32_t Simulator::getPC() const
{
    return pc;
}

void Simulator::setPC(int32_t newPC)
{
    std::cout << "PC changed from 0x" << std::hex << pc << " to 0x" << newPC << std::dec << std::endl;
    pc = newPC;
    // printRegisters();
}

int32_t Simulator::loadWord(int32_t address) const
{
    if (address < 0 || address >= MEMORY_SIZE)
    {
        throw std::out_of_range("Memory access out of bounds");
    }
    return memory[address / 4];
}

void Simulator::storeWord(int32_t address, int32_t value)
{
    if (address < 0 || address >= MEMORY_SIZE)
    {
        throw std::out_of_range("Memory access out of bounds");
    }
    std::cout << std::hex << "Memory 0x" << address << " changed from 0x" << memory[address / 4] << " to 0x" << value << std::dec << std::endl;
    memory[address / 4] = value;
}

std::string Simulator::instToString(uint32_t instruction)
{
    std::ostringstream sstr;
    const uint32_t opcode = getOpcode(instruction);

    switch (opcode)
    {
    case 0x33:
    {
        // add, sub
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t funct7 = getFunct7(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        if (funct3 == 0x0)
        {
            if (funct7 == 0x00)
            {
                sstr << "add x" << rd << ", x" << rs1 << ", x" << rs2;
            }
            else if (funct7 == 0x20)
            {
                sstr << "sub x" << rd << ", x" << rs1 << ", x" << rs2;
            }
        }
        break;
    }
    case 0x13:
    {
        // addi
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 11) & 1)
        {
            imm -= 1 << 12;
        }

        if (funct3 == 0x0)
        {
            sstr << "addi x" << rd << ", x" << rs1 << ", " << imm;
        }
        break;
    }
    case 0x63:
    {
        // beq, bne
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = ((instruction >> 7) & 0x1E) | (((instruction >> 25) & 0x7F) << 5) | ((instruction >> 31) << 12) | (((instruction >> 7) & 0x1) << 11);
        // 符号ビットを処理
        if ((imm >> 12) & 1)
        {
            imm -= 1 << 13;
        }

        if (funct3 == 0x0)
        {
            sstr << "beq x" << rs1 << ", x" << rs2 << ", offset " << imm;
        }
        else if (funct3 == 0x1)
        {
            sstr << "bne x" << rs1 << ", x" << rs2 << ", offset " << imm;
        }
        break;
    }
    case 0x6F:
    {
        // jal
        const uint32_t rd = getRd(instruction);
        int32_t imm = (((instruction >> 12) & 0xFF) << 12) | (((instruction >> 20) & 0x1) << 11) | ((instruction >> 20) & 0x7FE) | ((instruction >> 31) << 20);
        // 符号ビットを処理
        if ((imm >> 20) & 1)
        {
            imm -= 1 << 21;
        }
        sstr << "jal x" << rd << ", offset " << imm;
        break;
    }
    case 0x67:
    {
        // jalr
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 11) & 1)
        {
            imm -= 1 << 12;
        }
        sstr << "jalr x" << rd << ", x" << rs1 << ", offset " << imm;
        break;
    }
    case 0x03:
    {
        // lw
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 11) & 1)
        {
            imm -= 1 << 12;
        }

        if (funct3 == 0x2)
        {
            sstr << "lw x" << rd << ", " << imm << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x23:
    {
        // sw
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = ((instruction >> 7) & 0x1F) | ((instruction >> 25) & 0x7F);
        // 符号ビットを処理
        if ((imm >> 11) & 1)
        {
            imm -= 1 << 12;
        }

        if (funct3 == 0x2)
        {
            sstr << "sw x" << rs2 << ", " << imm << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x73:
    {
        if (instruction == 0b00000000000100000000000001110011)
        {

            sstr << "ebreak";
        }
        break;
    }
    default:
        throw std::runtime_error("Unknown instruction");
    }
    return sstr.str();
}

void Simulator::loadMemoryFromBinary(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open binary file");
    }

    uint32_t instruction;
    int64_t address = 0;
#ifdef DEBUG
    std::cout << "DEBUG MODE INSTRUCTION LIST" << std::endl;
#endif // DEBUG

    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {

#ifdef DEBUG

        std::cout << "0x" << std::hex << address << ": " << std::dec << instToString(instruction) << std::endl;

#endif // DEBUG
        storeWord(address, instruction);
        address += 4;
        if (address >= MEMORY_SIZE)
        {
            throw std::runtime_error("Program size exceeds memory limits");
        }
    }

    /*
    for (int i = 0; i < address / 4; i++)
    {
        std::cout << std::dec << "memory[" << i << "] is 0b" << std::bitset<32>(memory[i]) << std::endl;
    }
    */
}

void Simulator::printRegisters() const
{
    std::cout << "Registers state:" << std::endl;
    for (int i = 0; i < REG_COUNT; ++i)
    {
        std::cout << "x" << i << ": 0x" << std::hex << registers[i] << std::dec << std::endl;
    }
    std::cout << "PC: 0x" << std::hex << pc << std::dec << std::endl;
}

inline uint32_t Simulator::getOpcode(uint32_t instruction) const
{
    return instruction & 0x7F;
}

inline uint32_t Simulator::getRd(uint32_t instruction) const
{
    return (instruction >> 7) & 0x1F;
}

inline uint32_t Simulator::getFunct3(uint32_t instruction) const
{
    return (instruction >> 12) & 0x7;
}

inline uint32_t Simulator::getRs1(uint32_t instruction) const
{
    return (instruction >> 15) & 0x1F;
}

inline uint32_t Simulator::getRs2(uint32_t instruction) const
{
    return (instruction >> 20) & 0x1F;
}

inline uint32_t Simulator::getFunct7(uint32_t instruction) const
{
    return (instruction >> 25) & 0x7F;
}

inline int32_t Simulator::getImmediate(uint32_t instruction) const
{
    return (instruction >> 20);
}

void Simulator::detectDataHazard(int32_t rs1, int32_t rs2)
{

    // 1命令前に書き込んだレジスタとのハザードを検出
    if (rs1 == prevWriteReg || rs2 == prevWriteReg)
    {
        logStall(2);
    }
    // 2命令前に書き込んだレジスタとのハザードを検出
    else if (rs1 == prevPrevWriteReg || rs2 == prevPrevWriteReg)
    {
        logStall(1);
    }
}

// 分岐予測(常にUntaken)
void Simulator::branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken)
{
    logBranchPrediction();

    // 分岐が実際に取られた場合はパイプラインをフラッシュ
    if (isTaken)
    {
        logFlush();
        setPC(getPC() + imm);
    }
    else
    {
        setPC(getPC() + 4);
    }
}

inline void Simulator::updateWriteReg(int currWriteReg)
{
    prevPrevWriteReg = prevWriteReg;
    prevWriteReg = currWriteReg;
}

void Simulator::executeInstruction(uint32_t instruction)
{
    int currWriteReg = NULLREG;
    const uint32_t opcode = getOpcode(instruction);

    std::cout << "Executing: " << instToString(instruction) << std::endl;
    
    const std::unordered_map<uint32_t, std::function<void()>> dispatchTable = {
        {0x33, [this, instruction, &currWriteReg]
         {
             // R-type (add, sub)
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t funct7 = getFunct7(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);

             detectDataHazard(rs1, rs2);

             if (funct3 == 0x0)
             {
                 if (funct7 == 0x00)
                 {
                     logInstruction("add");
                     setRegister(rd, getRegister(rs1) + getRegister(rs2));
                 }
                 else if (funct7 == 0x20)
                 {
                     logInstruction("sub");
                     setRegister(rd, getRegister(rs1) - getRegister(rs2));
                 }
             }
             setPC(getPC() + 4);
             currWriteReg = rd;
         }},
        {0x13, [this, instruction, &currWriteReg]
         {
             // I-type (addi)
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             int32_t imm = getImmediate(instruction);
             // 符号ビットを処理
             if ((imm >> 11) & 1)
             {
                 imm -= 1 << 12;
             }

             detectDataHazard(rs1, NOWRITEREG);

             if (funct3 == 0x0)
             {
                 logInstruction("addi");
                 setRegister(rd, getRegister(rs1) + imm);
             }
             setPC(getPC() + 4);
             currWriteReg = rd;
         }},
        {0x63, [this, instruction, &currWriteReg]
         {
             // B-type (beq, bne)
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);
             int32_t imm = ((instruction >> 7) & 0x1E) | (((instruction >> 25) & 0x7F) << 5) | ((instruction >> 31) << 12) | (((instruction >> 7) & 0x1) << 11);
             // 符号ビットを処理
             if ((imm >> 12) & 1)
             {
                 imm -= 1 << 13;
             }

             bool isTaken = false;

             if (funct3 == 0x0)
             {
                 logInstruction("beq"); // 命令の記録
                 isTaken = (getRegister(rs1) == getRegister(rs2));
             }
             else if (funct3 == 0x1)
             {
                 logInstruction("bne"); // 命令の記録
                 isTaken = (getRegister(rs1) != getRegister(rs2));
             }
             branchPrediction(rs1, rs2, imm, isTaken); // 分岐予測の実行
         }},
        {0x6F, [this, instruction, &currWriteReg]
         {
             // J-type (jal)
             const uint32_t rd = getRd(instruction);
             int32_t imm = (((instruction >> 12) & 0xFF) << 12) | (((instruction >> 20) & 0x1) << 11) | ((instruction >> 20) & 0x7FE) | ((instruction >> 31) << 20);
             // 符号ビットを処理
             if ((imm >> 20) & 1)
             {
                 imm -= 1 << 21;
             }
             logInstruction("jal"); // 命令の記録
             setRegister(rd, getPC() + 4);
             setPC(getPC() + imm);
         }},
        {0x67, [this, instruction, &currWriteReg]
         {
             // I-type (jalr)
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             int32_t imm = getImmediate(instruction);
             // 符号ビットを処理
             if ((imm >> 11) & 1)
             {
                 imm -= 1 << 12;
             }

             detectDataHazard(rs1, NOWRITEREG);

             logInstruction("jalr"); // 命令の記録
             const int64_t temp = getPC() + 4;
             setPC((getRegister(rs1) + imm) & ~1);
             setRegister(rd, temp);
             currWriteReg = rd;
         }},
        {0x03, [this, instruction, &currWriteReg]
         {
             // I-type (lw)
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             int32_t imm = getImmediate(instruction);
             // 符号ビットを処理
             if ((imm >> 11) & 1)
             {
                 imm -= 1 << 12;
             }

             detectDataHazard(rs1, NOWRITEREG);

             if (funct3 == 0x2)
             {
                 const int64_t address = getRegister(rs1) + imm;
                 logInstruction("lw"); // 命令の記録
                 setRegister(rd, loadWord(address));
             }
             setPC(getPC() + 4);

             currWriteReg = rd;
         }},
        {0x23, [this, instruction, &currWriteReg]
         {
             // S-type (sw)
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);
             int32_t imm = ((instruction >> 7) & 0x1F) | (((instruction >> 25) & 0x7F) << 5);
             // 符号ビットを処理
             if ((imm >> 11) & 1)
             {
                 imm -= 1 << 12;
             }

             detectDataHazard(rs1, NOWRITEREG);

             if (funct3 == 0x2)
             {
                 const int64_t address = getRegister(rs1) + imm;
                 logInstruction("sw"); // 命令の記録
                 storeWord(address, getRegister(rs2));
             }
             setPC(getPC() + 4);
         }},
        {0x73, [this, instruction]
         {
             if (instruction == 0b00000000000100000000000001110011)
             {
                 std::cout << "Program reached breakpoint" << std::endl;
                 logInstruction("ebreak"); // 命令の記録
                 isBreakpoint = true;
             }
         }}};

    if (dispatchTable.find(opcode) != dispatchTable.end())
    {
        dispatchTable.at(opcode)();
    }
    else
    {
        throw std::runtime_error("Unknown instruction");
    }
    updateWriteReg(currWriteReg);
}

// ログの出力
void Simulator::printLog() const
{
    Log::printLog();
}

void Simulator::runProgram()
{
#ifdef DEBUG
    std::cout << "__DEBUG_MODE__" << std::endl;
#endif // DEBUG
    uint32_t CLK = 0;
    while (MAXCLK > CLK && !isBreakpoint)
    {
        std::cout << "CLK : " << CLK << std::endl;
        const uint32_t instruction = loadWord(pc);
#ifdef DEBUG
        std::cout << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG

        executeInstruction(instruction);
        CLK++;
    }
    printRegisters();
    printLog();
}
