#include "Simulator.hpp"
#include <fstream>
#include <iostream>

Simulator::Simulator()
{
    registers.fill(0);
}

int64_t Simulator::getRegister(int reg) const
{
    if (reg < 0 || reg >= REG_COUNT)
    {
        throw std::out_of_range("Invalid register index");
    }
    return reg == 0 ? 0 : registers[reg];
}

void Simulator::setRegister(int reg, int64_t value)
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

int64_t Simulator::getPC() const
{
    return pc;
}

void Simulator::setPC(int64_t newPC)
{
    std::cout << "PC changed from 0x" << std::hex << pc << " to 0x" << newPC << std::dec << std::endl;
    pc = newPC;
    // printRegisters();
}

int64_t Simulator::loadWord(int64_t address) const
{
    if (address < 0 || address >= MEMORY_SIZE)
    {
        throw std::out_of_range("Memory access out of bounds");
    }
    return memory[address / 8];
}

void Simulator::storeWord(int64_t address, int64_t value)
{
    if (address < 0 || address >= MEMORY_SIZE)
    {
        throw std::out_of_range("Memory access out of bounds");
    }
    memory[address / 8] = value;
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

    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {
        storeWord(address, instruction);
        address += 4;
        if (address >= MEMORY_SIZE)
        {
            throw std::runtime_error("Program size exceeds memory limits");
        }
    }
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

uint32_t Simulator::getOpcode(uint32_t instruction) const
{
    return instruction & 0x7F;
}

uint32_t Simulator::getRd(uint32_t instruction) const
{
    return (instruction >> 7) & 0x1F;
}

uint32_t Simulator::getFunct3(uint32_t instruction) const
{
    return (instruction >> 12) & 0x7;
}

uint32_t Simulator::getRs1(uint32_t instruction) const
{
    return (instruction >> 15) & 0x1F;
}

uint32_t Simulator::getRs2(uint32_t instruction) const
{
    return (instruction >> 20) & 0x1F;
}

uint32_t Simulator::getFunct7(uint32_t instruction) const
{
    return (instruction >> 25) & 0x7F;
}

int32_t Simulator::getImmediate(uint32_t instruction) const
{
    return (instruction >> 20);
}

void Simulator::executeInstruction(uint32_t instruction)
{
    const uint32_t opcode = getOpcode(instruction);

    static const std::unordered_map<uint32_t, std::function<void()>> dispatchTable = {
        {0x33, [this, instruction]
         {
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t funct7 = getFunct7(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);

             if (funct3 == 0x0)
             {
                 if (funct7 == 0x00)
                 {
                     std::cout << "Executing: add x" << rd << ", x" << rs1 << ", x" << rs2 << std::endl;
                     setRegister(rd, getRegister(rs1) + getRegister(rs2));
                 }
                 else if (funct7 == 0x20)
                 {
                     std::cout << "Executing: sub x" << rd << ", x" << rs1 << ", x" << rs2 << std::endl;
                     setRegister(rd, getRegister(rs1) - getRegister(rs2));
                 }
             }
         }},
        {0x13, [this, instruction]
         {
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const int32_t imm = getImmediate(instruction);

             if (funct3 == 0x0)
             {
                 std::cout << "Executing: addi x" << rd << ", x" << rs1 << ", " << imm << std::endl;
                 setRegister(rd, getRegister(rs1) + imm);
             }
         }},
        {0x63, [this, instruction]
         {
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);
             const int32_t imm = ((instruction >> 7) & 0x1E) | ((instruction >> 25) & 0x7F) | ((instruction >> 31) << 12);

             if (funct3 == 0x0)
             {
                 std::cout << "Executing: beq x" << rs1 << ", x" << rs2 << ", offset " << imm << std::endl;
                 if (getRegister(rs1) == getRegister(rs2))
                 {
                     setPC(getPC() + imm);
                 }
                 else
                 {
                     setPC(getPC() + 4);
                 }
             }
             else if (funct3 == 0x1)
             {
                 std::cout << "Executing: bne x" << rs1 << ", x" << rs2 << ", offset " << imm << std::endl;
                 if (getRegister(rs1) != getRegister(rs2))
                 {
                     setPC(getPC() + imm);
                 }
                 else
                 {
                     setPC(getPC() + 4);
                 }
             }
         }},
        {0x6F, [this, instruction]
         {
             const uint32_t rd = getRd(instruction);
             const int32_t imm = ((instruction >> 12) & 0xFF) | ((instruction & 0x80000000) >> 11) | ((instruction >> 20) & 0x7FE) | ((instruction >> 31) << 20);

             std::cout << "Executing: jal x" << rd << ", offset " << imm << std::endl;
             setRegister(rd, getPC() + 4);
             setPC(getPC() + imm);
         }},
        {0x67, [this, instruction]
         {
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const int32_t imm = getImmediate(instruction);

             std::cout << "Executing: jalr x" << rd << ", x" << rs1 << ", offset " << imm << std::endl;
             const int64_t temp = getPC() + 4;
             setPC((getRegister(rs1) + imm) & ~1);
             setRegister(rd, temp);
         }},
        {0x03, [this, instruction]
         {
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rd = getRd(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const int32_t imm = getImmediate(instruction);

             if (funct3 == 0x2)
             {
                 const int64_t address = getRegister(rs1) + imm;
                 std::cout << "Executing: lw x" << rd << ", " << imm << "(x" << rs1 << ")" << std::endl;
                 setRegister(rd, loadWord(address));
             }
         }},
        {0x23, [this, instruction]
         {
             const uint32_t funct3 = getFunct3(instruction);
             const uint32_t rs1 = getRs1(instruction);
             const uint32_t rs2 = getRs2(instruction);
             const int32_t imm = ((instruction >> 7) & 0x1F) | ((instruction >> 25) & 0x7F);

             if (funct3 == 0x2)
             {
                 const int64_t address = getRegister(rs1) + imm;
                 std::cout << "Executing: sw x" << rs2 << ", " << imm << "(x" << rs1 << ")" << std::endl;
                 storeWord(address, getRegister(rs2));
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
}

void Simulator::runProgram()
{
    constexpr int MAXCLK = 10000;
    uint32_t CLK = 0;
    while (MAXCLK > CLK)
    {
        const uint32_t instruction = loadWord(pc);
        executeInstruction(instruction);
        setPC(getPC() + 4);
        CLK++;
    }
}
