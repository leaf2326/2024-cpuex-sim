#include "Simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <bit>

Simulator::Simulator(Options op, uint64_t maxClock)
{
    registers[0] = 0;                    // x0
    registers[2] = DMEMORY_SIZE / 4 - 1; // sp
    pc = 0;
    isBreakpoint = false;
    options = op;
    max_clk = maxClock;
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
        std::cerr << "Register x" << reg << " changed from 0x" << std::hex << registers[reg] << " to 0x" << value << std::dec << std::endl;
    }
    registers[reg] = value;
    // printRegisters();
}
int32_t Simulator::getFpRegister(int fpreg) const
{
    if (fpreg < 0 || fpreg >= FPREG_COUNT)
    {
        throw std::out_of_range("Invalid fpregister index");
    }
    return fpreg == 0 ? 0 : fpRegisters[fpreg];
}

void Simulator::setFpRegister(int fpreg, int32_t fpvalue)
{
    if (fpreg == 0)
        return;
    if (fpreg < 0 || fpreg >= FPREG_COUNT)
    {
        throw std::out_of_range("Invalid fpregister index");
    }
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "fpRegister fp" << fpreg << " changed from 0x" << std::hex << fpRegisters[fpreg] << " to 0x" << fpvalue << std::dec << std::endl;
    }
    fpRegisters[fpreg] = fpvalue;
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
        std::cerr << "PC changed from 0x" << std::hex << pc << " to 0x" << newPC << std::dec << std::endl;
    }
    pc = newPC;
    // printRegisters();
}

int32_t Simulator::loadInstruction(int32_t address) const
{
    if (address < 0 || address >= IMEMORY_SIZE)
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
    return iMemory[address / 4];
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
        std::cerr << "0x" << std::hex << address << ": " << std::dec << instToString(instruction) << std::endl;
    }
#endif // DEBUG
    iMemory[address / 4] = instruction;
}

std::string Simulator::instToString(uint32_t instruction) const
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
        else if (funct3 == 0x1)
        {
            sstr << "slli x" << rd << ", x" << rs1 << ", " << imm;
        }
        else if (funct3 == 0x5)
        {
            sstr << "srli x" << rd << ", x" << rs1 << ", " << imm;
        }
        break;
    }
    case 0x63:
    {
        // beq, bne, blt, bge
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
        else if (funct3 == 0x4)
        {
            sstr << "blt x" << rs1 << ", x" << rs2 << ", offset " << imm;
        }
        else if (funct3 == 0x5)
        {
            sstr << "bge x" << rs1 << ", x" << rs2 << ", offset " << imm;
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
    case 0x37:
    {
        const uint32_t rd = getRd(instruction);
        int32_t imm = (instruction >> 12) & 0xFFFFF;
        sstr << "lui x" << rd << ", 0x" << std::hex << imm << std::dec;
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
    case 0x07:
    {
        // flw
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
            sstr << "flw fp" << rd << ", " << imm << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x27:
    {
        // fsw
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
            sstr << "fsw fp" << rs2 << ", " << imm << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x53:
    {
        // fadd, fsub, fmul, fdiv, ftoi, itof
        const uint32_t funct7 = getFunct7(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        if (funct7 == 0x00)
        {
            sstr << "fadd fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (funct7 == 0x04)
        {
            sstr << "fsub fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (funct7 == 0x08)
        {
            sstr << "fmul fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (funct7 == 0x0C)
        {
            sstr << "fdiv fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (funct7 == 0x60)
        {
            sstr << "ftoi x" << rd << ", fp" << rs1;
        }
        else if (funct7 == 0x68)
        {
            sstr << "itof fp" << rd << ", x" << rs1;
        }
        else if (funct7 == 0x2C)
        {
            sstr << "fsqrt fp" << rd << ", fp" << rs1;
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
    std::cerr << "Load memory from binary file..." << std::endl;
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open binary file");
    }
#ifdef DEBUG
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "DEBUG MODE INSTRUCTION LIST" << std::endl;
    }
#endif // DEBUG
    uint32_t instruction;
    uint32_t data;
    int64_t address;
    file.read(reinterpret_cast<char *>(&dataSectionSize), sizeof(dataSectionSize));
    address = 256;
    for (unsigned int i = 0; i < dataSectionSize / sizeof(dataSectionSize); i++)
    {
        file.read(reinterpret_cast<char *>(&data), sizeof(data));
        if (address < 0 || address >= DMEMORY_SIZE)
        {
            throw std::out_of_range("dMemory access out of bounds");
        }

        dMemory.mainMemory[address / 4] = data;
        address += 4;
        if (address >= DMEMORY_SIZE)
        {
            throw std::out_of_range("Program size exceeds dMemory limits");
        }
    }

    address = 0;
    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {
        storeInstruction(address, instruction);
        address += 4;
        if (address >= IMEMORY_SIZE)
        {
            throw std::out_of_range("Program size exceeds iMemory limits");
        }
    }
    instructionCount = address / 4;
    /*
    for (int i = 0; i < address / 4; i++)
    {
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << std::dec << "iMemory[" << i << "] is 0b" << std::bitset<32>(iMemory[i]) << std::endl;
    }
    }
    */
    std::cerr << "Completed loading memory" << std::endl;
}

void Simulator::printProgram(bool aroundPC) const noexcept
{
    printBoundary();
    if (aroundPC)
    {

        for (int i = -1; i < 2; i++)
        {
            if (!options.has(options.ONLYSTDIO))
            {
                int32_t address = getPC() / 4 + i;
                if (address < 0 || address >= IMEMORY_SIZE / 4)
                {
                    std::cerr << "-: " << std::endl;
                }
                else
                {
                    const uint32_t instruction = iMemory[address];
                    std::cerr << address + 1 << ": " << instToString(instruction);
                    if (i == 0)
                    {
                        std::cerr << " ←-";
                    }
                    std::cerr << std::endl;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < instructionCount; i++)
        {
            const uint32_t instruction = iMemory[i];
            if (!options.has(options.ONLYSTDIO))
            {
                std::cerr << i + 1 << ": " << instToString(instruction);
                if (i == getPC() / 4)
                {
                    std::cerr << " ←-";
                }
                std::cerr << std::endl;
            }
        }
    }
    printBoundary();
}

void Simulator::loadInputData(const std::string &inputFilePath)
{
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "Loading input file..." << std::endl;
    }
    std::ifstream file(inputFilePath);
    if (!file)
    {
        throw std::runtime_error("Could not open input file" + inputFilePath);
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
                dMemory.inputData.push_back(intValue);
            }
            else
            {
                // 整数として扱う場合
                int32_t intValue = std::stoi(token);
                dMemory.inputData.push_back(intValue);
            }
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to process token " + token + " - ");
        }
    }
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "Completed loading input file" << std::endl;
    }
#ifdef DEBUG
    std::cerr << "Input Data:" << std::endl;
    for (const auto &data : dMemory.inputData)
    {
        std::cerr << data << " ";
    }
    std::cerr << std::endl;
#endif
}

void Simulator::printRegisters() const
{
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "Registers state:" << std::endl;
        for (int i = 0; i < REG_COUNT; ++i)
        {
            std::cerr << "x" << i << ": 0x" << std::hex << registers[i] << std::dec << std::endl;
        }
        for (int i = 0; i < FPREG_COUNT; ++i)
        {
            std::cerr << "fp" << i << ": 0x" << std::hex << fpRegisters[i] << std::dec << std::endl;
        }
        std::cerr << "PC: 0x" << std::hex << pc << std::dec << std::endl;
    }
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

void Simulator::printInstruction(uint32_t instruction) const
{
    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "Executing: " << instToString(instruction) << std::endl;
    }
}

void Simulator::executeInstruction(uint32_t instruction)
{
    int currLoadReg = NULLREG;
    const uint32_t opcode = getOpcode(instruction);
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
        else if (funct3 == 0x1)
        {
            if (!(imm >= 0 && imm <= 3))
            {
                std::cerr << "Warning: shamt is not between 0 and 3" << std::endl;
            }
            logInstruction("slli");
            setRegister(rd, getRegister(rs1) << imm);
        }
        else if (funct3 == 0x5)
        {
            if (!(imm >= 0 && imm <= 3))
            {
                std::cerr << "Warning: shamt is not between 0 and 3" << std::endl;
            }
            logInstruction("srli");
            setRegister(rd, getRegister(rs1) >> imm);
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
        else if (funct3 == 0x4)
        {
            logInstruction("blt"); // 命令の記録
            isTaken = (getRegister(rs1) < getRegister(rs2));
        }
        else if (funct3 == 0x5)
        {
            logInstruction("bge"); // 命令の記録
            isTaken = (getRegister(rs1) >= getRegister(rs2));
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
            setRegister(rd, dMemory.loadWord(address));
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
            dMemory.storeWord(address, getRegister(rs2));
        }
        setPC(getPC() + 4);
        break;
    }
    case 0x37:
    {
        // ?-type (lui)
        const uint32_t rd = getRd(instruction);
        int32_t imm = (instruction >> 12) & 0xFFFFF;
        setRegister(rd, imm << 12);
        setPC(getPC() + 4);
        break;
    }
    case 0x07:
    {
        // I-type (flw)
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
            logInstruction("flw"); // 命令の記録
            setFpRegister(rd, dMemory.loadWord(address));
        }
        setPC(getPC() + 4);

        currLoadReg = rd + REG_COUNT; // Register:0~REG_COUNT-1, fpRegister: REG_COUNT~REG_COUNT+FPREG_COUNT-1
        break;
    }
    case 0x27:
    {
        // S-type (fsw)
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
            logInstruction("fsw"); // 命令の記録
            dMemory.storeWord(address, getFpRegister(rs2));
        }
        setPC(getPC() + 4);
        break;
    }
    case 0x53:
    {
        // fadd, fsub, fmul, fdiv, ftoi, itof
        const uint32_t funct7 = getFunct7(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        detectPrevLoad(rs1, rs2);

        if (funct7 == 0x00)
        {
            logInstruction("fadd");
            setFpRegister(rd, fpu.fadd(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (funct7 == 0x04)
        {
            logInstruction("fsub");
            setFpRegister(rd, fpu.fsub(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (funct7 == 0x08)
        {
            logInstruction("fmul");
            setFpRegister(rd, fpu.fmul(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (funct7 == 0x0C)
        {
            logInstruction("fdiv");
            setFpRegister(rd, fpu.fdiv(getFpRegister(rs1), getFpRegister(rs2)));
        }
        /*
        else if (funct7 == 0x60)
        {
            sstr << "ftoi x" << rd << ", fp" << rs1;
        }
        else if (funct7 == 0x68)
        {
            sstr << "itof fp" << rd << ", x" << rs1;
        }
        */
        else if (funct7 == 0x2C)
        {
            logInstruction("fsqrt");
            setFpRegister(rd, fpu.fsqrt(getFpRegister(rs1)));
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
                std::cerr << "Program reached breakpoint" << std::endl;
            }
            logInstruction("ebreak"); // 命令の記録
            isBreakpoint = true;
        }
        setPC(getPC() + 4);
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

void Simulator::printOutput() const noexcept
{
    for (const auto &o : dMemory.output)
    {
        char output_c = o & 0xFF;
        std::cout << output_c;
    }
}

void Simulator::runProgram(int outputRegNum)
{
    if (!options.has(options.ONLYSTDIO))
    {
#ifdef DEBUG
        std::cerr << "__DEBUG_MODE__" << std::endl;
#endif // DEBUG
        std::cerr << "__Simulating Program__" << std::endl;
        if (options.has(options.GDB))
        {
            std::cerr << "__GDB MODE__" << std::endl;
        }
    }
    uint32_t CLK = 0;
    if (options.has(options.GDB))
    {
        bool breakMode = false;
        std::string gdbCommand;
        while (true)
        {
            if (!breakMode)
            {
                std::cin >> gdbCommand;
                if (gdbCommand == "l")
                {
                    printProgram(false);
                    continue;
                }
                else if (gdbCommand == "s")
                {
                    const uint32_t instruction = loadInstruction(pc);
                    if (!options.has(options.ONLYSTDIO))
                    {
                        std::cerr << "CLK : " << CLK << std::endl;
#ifdef DEBUG
                        std::cerr << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG
                    }
                    printInstruction(instruction);
                    executeInstruction(instruction);
                    CLK++;
                    printProgram(true);
                }
                else if (gdbCommand == "c")
                {
                    std::cerr << "heading to eBreak..." << std::endl;
                    printBoundary();
                    breakMode = true;
                    options.on(options.ONLYSTDIO);
                }
                else if (gdbCommand == "quit")
                {
                    break;
                }
                else
                {
                    std::cerr << "Unknown command: " << gdbCommand << std::endl;
                }
            }
            else
            {
                const uint32_t instruction = loadInstruction(pc);
                executeInstruction(instruction);
                if (max_clk > CLK && isBreakpoint)
                {
                    breakMode = false;
                    options.off(options.ONLYSTDIO);
                    if (!options.has(options.ONLYSTDIO))
                    {
                        std::cerr << "CLK : " << CLK << std::endl;
#ifdef DEBUG
                        std::cerr << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG
                    }
                    printInstruction(instruction);
                }
                CLK++;
            }
        }
    }
    else
    {
        while (max_clk > CLK && !isBreakpoint)
        {
            const uint32_t instruction = loadInstruction(pc);
            if (!options.has(options.ONLYSTDIO))
            {
                std::cerr << "CLK : " << CLK << std::endl;
#ifdef DEBUG
                std::cerr << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG
            }
            printInstruction(instruction);
            executeInstruction(instruction);
            CLK++;
        }
        if (!options.has(options.ONLYSTDIO))
        {
            printRegisters();
            printLog();
            std::cerr << "__Cache__" << std::endl;
            dMemory.printCache();
        }
        printOutput();
        if (options.has(options.REG))
        {
            // 0-31はレジスタに対応
            if (outputRegNum >= 0 && outputRegNum < REG_COUNT)
            {
                std::cout << "x" << outputRegNum << ": 0x" << std::hex << getRegister(outputRegNum) << std::dec << std::endl;
            }
            // 32以上はfpレジスタに対応
            else if (outputRegNum >= REG_COUNT && outputRegNum < REG_COUNT + FPREG_COUNT)
            {
                std::cout << "fp" << outputRegNum - REG_COUNT << ": 0x" << std::hex << getFpRegister(outputRegNum - REG_COUNT) << std::dec << std::endl;
            }
            else
            {
                throw std::out_of_range("-reg <RegNum>: RegNum isn't between 0 to" + std::to_string(REG_COUNT + FPREG_COUNT - 1));
            }
        }
    }

    if (!options.has(options.ONLYSTDIO))
    {
        std::cerr << "__Simulator Terminated__" << std::endl;
    }
}
