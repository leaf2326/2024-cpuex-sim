#include "Simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <bit>
#define MAXCLK 100000

Simulator::Simulator(Options op)
{
    registers[0] = 0;                    // x0
    registers[2] = DMEMORY_SIZE / 4 - 1; // sp
    pc = 0;
    isBreakpoint = false;
    options = op;
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
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "Register x" << reg << " changed from 0x" << std::hex << registers[reg] << " to 0x" << value << std::dec << std::endl;
    }
    registers[reg] = value;
    // printRegisters();
}

int32_t Simulator::getPC() const
{
    return pc;
}

void Simulator::setPC(int32_t newPC)
{
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "PC changed from 0x" << std::hex << pc << " to 0x" << newPC << std::dec << std::endl;
    }
    pc = newPC;
    // printRegisters();
}

int32_t Simulator::loadWord(int32_t address) const
{
    if (address < 0 || address >= DMEMORY_SIZE)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    return dMemory[address / 4];
}
int32_t Simulator::loadInstruction(int32_t address) const
{
    if (address < 0 || address >= IMEMORY_SIZE)
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
    return iMemory[address / 4];
}

void Simulator::storeWord(int32_t address, int32_t value)
{
    if (address < 0 || address >= DMEMORY_SIZE)
    {
        throw std::out_of_range("dMemory access out of bounds");
    }
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << std::hex << "dMemory 0x" << address << " changed from 0x" << dMemory[address / 4] << " to 0x" << value << std::dec << std::endl;
    }
    dMemory[address / 4] = value;
    if (address == OUTPUT_ADDRESS)
    {
        if (!options.has(options.ONLYSTDIO))
        {
            std::cout << "Output: " << std::hex << dMemory[address / 4] << std::dec << std::endl;
        }
        output.emplace_back(value);
    }
}
void Simulator::storeInstruction(int32_t address, int32_t instruction)
{
    if (address < 0 || address >= IMEMORY_SIZE)
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
#ifdef DEBUG
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "0x" << std::hex << address << ": " << std::dec << instToString(instruction) << std::endl;
    }
#endif // DEBUG
    iMemory[address / 4] = instruction;
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
        int32_t imm = ((instruction >> 7) & 0x1F) | (((instruction >> 25) & 0x7F) << 5);
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
        throw std::runtime_error("Error: Could not open binary file");
    }

    uint32_t instruction;
    int64_t address = 0;
#ifdef DEBUG
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "DEBUG MODE INSTRUCTION LIST" << std::endl;
    }
#endif // DEBUG

    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {
        storeInstruction(address, instruction);
        address += 4;
        if (address >= IMEMORY_SIZE)
        {
            throw std::runtime_error("Error: Program size exceeds iMemory limits");
        }
    }

    /*
    for (int i = 0; i < address / 4; i++)
    {
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << std::dec << "iMemory[" << i << "] is 0b" << std::bitset<32>(iMemory[i]) << std::endl;
    }
    }
    */
}

void Simulator::loadInputData(const std::string &inputFilePath)
{
    std::ifstream file(inputFilePath);
    if (!file)
    {
        std::cerr << "Error: Could not open input file" << inputFilePath << std::endl;
        return;
    }

    std::string token;
    while (file >> token)
    {
        try
        {
            if (token.find('.') != std::string::npos)
            {
                // 小数として扱う場合
                float floatValue = std::stof(token);
                int32_t intValue = std::bit_cast<int32_t>(floatValue);
                inputData.push_back(intValue);
            }
            else
            {
                // 整数として扱う場合
                int32_t intValue = std::stoi(token);
                inputData.push_back(intValue);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: Failed to process token " << token << " - " << e.what() << std::endl;
        }
    }
#ifdef DEBUG
    std::cout << "Input Data:" << std::endl;
    for (const auto &data : inputData)
    {
        std::cout << data << " ";
    }
    std::cout << std::endl;
#endif
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

void Simulator::detectPrevLoad(int32_t rs1, int32_t rs2)
{

    // 1命令前にロードしたレジスタであるかを検出
    if (rs1 == prevLoadReg || rs2 == prevLoadReg)
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

inline void Simulator::updatePrevLoadReg(int currLoadReg)
{
    prevLoadReg = currLoadReg;
}

void Simulator::executeInstruction(uint32_t instruction)
{
    int currLoadReg = NULLREG;
    const uint32_t opcode = getOpcode(instruction);
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "Executing: " << instToString(instruction) << std::endl;
    }
    switch (opcode)
    {
    case 0x33:
    {
        // R-type (add, sub)
        const uint32_t funct3 = getFunct3(instruction);
        const uint32_t funct7 = getFunct7(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        detectPrevLoad(rs1, rs2);

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
        break;
    }

    case 0x13:
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

        detectPrevLoad(rs1, NOLOADREG);

        if (funct3 == 0x0)
        {
            logInstruction("addi");
            setRegister(rd, getRegister(rs1) + imm);
        }
        setPC(getPC() + 4);
        break;
    }
    case 0x63:
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
        detectPrevLoad(rs1, rs2);
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
        break;
    }
    case 0x6F:
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
        break;
    }
    case 0x67:
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

        detectPrevLoad(rs1, NOLOADREG);

        logInstruction("jalr"); // 命令の記録
        const int64_t temp = getPC() + 4;
        setPC((getRegister(rs1) + imm) & ~1);
        setRegister(rd, temp);
        break;
    }
    case 0x03:
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

        detectPrevLoad(rs1, NOLOADREG);

        if (funct3 == 0x2)
        {
            const int64_t address = getRegister(rs1) + imm;
            logInstruction("lw"); // 命令の記録
            setRegister(rd, loadWord(address));
        }
        setPC(getPC() + 4);

        currLoadReg = rd;
        break;
    }
    case 0x23:
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

        detectPrevLoad(rs1, rs2);

        if (funct3 == 0x2)
        {
            const int64_t address = getRegister(rs1) + imm;
            logInstruction("sw"); // 命令の記録
            storeWord(address, getRegister(rs2));
        }
        setPC(getPC() + 4);
        break;
    }
    case 0x73:
    {
        if (instruction == 0b00000000000100000000000001110011)
        {
            if (!options.has(options.ONLYSTDIO))
            {
                std::cout << "Program reached breakpoint" << std::endl;
            }
            logInstruction("ebreak"); // 命令の記録
            isBreakpoint = true;
        }
        break;
    }
    default:
        throw std::runtime_error("Unknown instruction");
    }

    updatePrevLoadReg(currLoadReg);
}

// ログの出力
void Simulator::printLog()
{
    Log::printLog();
}

void Simulator::printOutput()
{

    std::cout << "P3" << std::endl;
    uint32_t r, g, b;
    for (const auto &o : output)
    {
        r = o & 0xFF000000;
        g = o & 0x00FF0000;
        b = o & 0x0000FF00;
        std::cout << std::setw(3) << std::setfill('0') << r << " " << g << " " << b << std::endl;
    }
}

void Simulator::runProgram()
{
#ifdef DEBUG
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "__DEBUG_MODE__" << std::endl;
    }
#endif // DEBUG
    uint32_t CLK = 0;
    while (MAXCLK > CLK && !isBreakpoint)
    {
        if (!options.has(options.ONLYSTDIO))
        {
            std::cout << "CLK : " << CLK << std::endl;
        }
        const uint32_t instruction = loadInstruction(pc);
#ifdef DEBUG
        if (!options.has(options.ONLYSTDIO))
        {
            std::cout << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
        }
#endif // DEBUG

        executeInstruction(instruction);
        CLK++;
    }
    if (!options.has(options.ONLYSTDIO))
    {
        printRegisters();
        printLog();
        std::cout << "__Output__" << std::endl;
    }
    printOutput();
    if (!options.has(options.ONLYSTDIO))
    {
        std::cout << "__Simulator Terminated__" << std::endl;
    }
}
