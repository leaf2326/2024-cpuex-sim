#include "Simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <bit>
#include "../include/pbar.hpp"

Simulator::Simulator(OptionHandler &op)
    : patternHistoryTable(NUM_ENTRIES, PHT_DEFAULT), dMemory(op.memorySize, CACHE_SIZE, BLOCK_SIZE, INPUT_ADDRESS, OUTPUT_ADDRESS, (op.cacheNumWay == 1), (op.cacheNumWay == 0 ? 1 : op.cacheNumWay))
{
    DMEMORY_SIZE = op.memorySize;
    registers[0] = 0;                 // x0
    registers[2] = DMEMORY_SIZE / 4 - 4; // sp
    pc = 0;
    isBreakpoint = false;
    enableCache = op.enableCache;
    enableICount = op.enableDebug;
    enableIStats = op.enableIStats;
    enableDebug = op.enableDebug;
    enableStdout = op.enableStdout;
    outputSize = op.imageSize * op.imageSize + 2;
    enableGDB = op.enableGDB;
    maxStep = op.maxStep;
    dMemory.availableCache = op.enableCache;
    availableLog = op.enableGDB || op.enableDebug;
    dMemory.availableLog = availableLog;
    outputFilePath = op.outputFilePath;
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
    if (availableLog)
        std::cerr << "Register x" << reg << " changed from " << std::hex << registers[reg] << " to " << value << std::dec << std::endl;

    registers[reg] = value;
    if (registers[2] <= registers[3])
    {
        throw std::out_of_range("Stack overflow! sp=" + std::to_string(registers[2]) + " hp=" + std::to_string(registers[3]));
    }
    // printRegisters(ALLREG);
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
    if (availableLog)
        std::cerr << "fpRegister fp" << fpreg << " changed from " << std::hex << fpRegisters[fpreg] << " to " << fpvalue << std::dec << std::endl;

    fpRegisters[fpreg] = fpvalue;
    // printRegisters(ALLREG);
}

void Simulator::setPC(int32_t newPC)
{
    if (availableLog)
        std::cerr << "PC changed from " << std::hex << pc << " to " << newPC << std::dec << std::endl;

    pc = newPC;
    // printRegisters(ALLREG)
}



int32_t Simulator::loadInstruction(int32_t address) const
{
    if (address < 0 || address >= IMEMORY_SIZE / 4)
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
    return iMemory[address];
}

void Simulator::storeInstruction(int32_t address, int32_t instruction)
{
    if (address < 0 || address >= IMEMORY_SIZE / 4)
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
#ifdef DEBUG
    std::cerr << std::hex << address << ": " << std::dec << instToString(instruction) << std::endl;
#endif // DEBUG
    iMemory[address] = instruction;
}

std::string Simulator::instToString(uint32_t instruction) const
{
    std::ostringstream sstr;
    const uint32_t opcode = getOpcode(instruction);

    switch (opcode)
    {
    case 0x1:
    {
        // add, sub
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        if (subop == 0x0)
        {
            sstr << "add x" << rd << ", x" << rs1 << ", x" << rs2;
        }
        else if (subop == 0x1)
        {
            sstr << "sub x" << rd << ", x" << rs1 << ", x" << rs2;
        }

        break;
    }
    case 0x2:
    {
        // addi, lui, slli, srli
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t imm = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((imm >> 13) & 1)
            {
                imm -= 1 << 14;
            }
            sstr << "addi x" << rd << ", x" << rs1 << ", " << imm;
        }
        else if (subop == 0x2)
        {
            const int32_t shamt = (instruction >> 6) & 0x3;
            sstr << "slli x" << rd << ", x" << rs1 << ", " << shamt;
        }
        else if (subop == 0x3)
        {
            const int32_t shamt = (instruction >> 6) & 0x3;
            sstr << "srli x" << rd << ", x" << rs1 << ", " << shamt;
        }
        else if (subop == 0x1)
        {
            const int32_t imm = ((((instruction >> 20) & 0xFFF) << 8) | ((instruction >> 6) & 0xFF)) << 12;
            sstr << "lui x" << rd << ", " << std::hex << imm << std::dec;
        }
        break;
    }
    case 0x3:
    {
        // beq, bne, blt, bge
        const uint32_t subop = getSubop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);    

        if (subop == 0x0)
        {
            sstr << "beq x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        else if (subop == 0x1)
        {
            sstr << "bne x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        else if (subop == 0x2)
        {
            sstr << "blt x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        else if (subop == 0x3)
        {
            sstr << "bge x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        break;
    }
    case 0x4:
    {
        // jal
        const uint32_t rd = getRd(instruction);
        int32_t imm = getImmediate(instruction);
        sstr << "jal x" << rd << ", " << imm;
        break;
    }
    case 0x5:
    {
        // jalr
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        sstr << "jalr x" << rd << ", x" << rs1;
        break;
    }
    case 0x8:
    {
        // lw, lwr
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            sstr << "lw x" << rd << ", " << offset << "(x" << rs1 << ")";
        }
        if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            sstr << "lwr x" << rd << ", x" << rs2 << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x9:
    {
        // sw
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t offset = getImmediate(instruction);
        // 符号ビットを処理
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }
        sstr << "sw x" << rs2 << ", " << offset << "(x" << rs1 << ")";
        break;
    }
    case 0xA:
    {
        // flw, flwr
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            sstr << "flw fp" << rd << ", " << offset << "(x" << rs1 << ")";
        }
        if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            sstr << "flwr fp" << rd << ", x" << rs2 << "(x" << rs1 << ")";
        }

        break;
    }
    case 0xB:
    {
        // fsw
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 13) & 1)
        {
            imm -= 1 << 14;
        }
        sstr << "fsw fp" << rs2 << ", " << imm << "(x" << rs1 << ")";
        break;
    }
    case 0xC:
    {
        // ftoi, flt, feq
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        if (fpuop == 0x4)
        {
            sstr << "ftoi x" << rd << ", fp" << rs1;
        }
        else if (fpuop == 0x0)
        {
            sstr << "flt x" << rd << ", fp" << rs1 << ", fp " << rs2;
        }
        else if (fpuop == 0x1)
        {
            sstr << "feq x" << rd << ", fp" << rs1 << ", fp " << rs2;
        }
        break;
    }
    case 0xD:
    {
        // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        if (fpuop == 0x9)
        {
            sstr << "itof fp" << rd << ", x" << rs1;
        }
        else if (fpuop == 0x0)
        {
            sstr << "fadd fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (fpuop == 0x1)
        {
            sstr << "fsub fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (fpuop == 0x2)
        {
            sstr << "fmul fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (fpuop == 0x3)
        {
            sstr << "fdiv fp" << rd << ", fp" << rs1 << ", fp" << rs2;
        }
        else if (fpuop == 0x4)
        {
            sstr << "fmv fp" << rd << ", fp" << rs1;
        }
        else if (fpuop == 0x5)
        {
            sstr << "fneg fp" << rd << ", fp" << rs1;
        }
        else if (fpuop == 0x6)
        {
            sstr << "fabs fp" << rd << ", fp" << rs1;
        }
        else if (fpuop == 0x7)
        {
            sstr << "fsqrt fp" << rd << ", fp" << rs1;
        }
        else if (fpuop == 0x8)
        {
            sstr << "ffloor fp" << rd << ", fp" << rs1;
        }
        break;
    }
    case 0x6:
    {
        sstr << "ebreak";
        break;
    }
    default:
        std::stringstream ss;
        ss << "Unknown instruction 0x" << std::hex << instruction;
        throw std::runtime_error(ss.str());
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

    std::cerr << "DEBUG MODE INSTRUCTION LIST" << std::endl;

#endif // DEBUG
    uint32_t instruction;
    uint32_t data;
    uint64_t address;
    file.read(reinterpret_cast<char *>(&dataSectionSize), sizeof(dataSectionSize));
    address = 64;
    for (unsigned int i = 0; i < dataSectionSize / sizeof(dataSectionSize); ++i)
    {
        file.read(reinterpret_cast<char *>(&data), sizeof(data));
        if (address < 0 || address >= DMEMORY_SIZE / 4)
        {
            throw std::out_of_range("dMemory access out of bounds");
        }
        dMemory.isInitialized[address] = true;
        dMemory.mainMemory[address] = data;
        address++;
        if (address >= DMEMORY_SIZE / 4)
        {
            throw std::out_of_range("Program size exceeds dMemory limits");
        }
    }

    address = 0;
    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {
        storeInstruction(address, instruction);
        address++;
        if (address >= IMEMORY_SIZE / 4)
        {
            throw std::out_of_range("Program size exceeds iMemory limits");
        }
    }
    instructionSize = address;
    std::cerr << "Completed loading memory" << std::endl;
}
void Simulator::printInstAddrCounts()
{
    std::cerr << "________Instructions________" << std::endl;
    std::cerr << std::setw(15) << "[iMEM Address]"
              << std::setw(15) << "[Count]:"
              << std::setw(19) << "Instruction" << std::endl;
    for (int i = 0; i < instructionSize; ++i)
    {
        const uint32_t instruction = iMemory[i];
        std::cerr << std::setw(15) << std::hex << i * 4 << std::dec
                  << std::setw(14) << instAddrCounts[i] << ":        "
                  << instToString(instruction) << std::endl;
    }
}
void Simulator::printInstStats() const
{
    std::cerr << "________Instruction Stats________" << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `add`" << std::right << std::setw(28) << " with immediate 0 (mv): " << mvCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `addi`" << std::right << std::setw(28) << " with register x0 (mvi): " << mviCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `lw`" << std::right << std::setw(28) << " with negative offsets: " << lwNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `lw`" << std::right << std::setw(28) << " with non-negative offsets: " << lwNonNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `sw`" << std::right << std::setw(28) << " with negative offsets: " << swNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `sw`" << std::right << std::setw(28) << " with non-negative offsets: " << swNonNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `flw`" << std::right << std::setw(28) << " with negative offsets: " << flwNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `flw`" << std::right << std::setw(28) << " with non-negative offsets: " << flwNonNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `fsw`" << std::right << std::setw(28) << " with negative offsets: " << fswNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `fsw`" << std::right << std::setw(28) << " with non-negative offsets: " << fswNonNegativeCount << std::endl;
    std::cerr << std::left << std::setw(17) << "Number of `flw`" << std::right << std::setw(28) << " of immediate value: " << flwImmCount << std::endl;
}
void Simulator::printProgram(bool aroundPC) const noexcept
{
    printBoundary();
    if (aroundPC)
    {
        for (int i = -1; i < 2; ++i)
        {
            int32_t address = getPC() + i;
            if (address < 0 || address >= instructionSize)
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
    else
    {
        for (int i = 0; i < instructionSize; ++i)
        {
            const uint32_t instruction = iMemory[i];
            std::cerr << i + 1 << ": " << instToString(instruction);
            if (i == getPC())
            {
                std::cerr << " ←-";
            }
            std::cerr << std::endl;
        }
    }
    printBoundary();
}

void Simulator::loadInputData(const std::string &inputFilePath)
{
    std::cerr << "Loading input file..." << std::endl;

    std::ifstream file(inputFilePath);
    if (!file)
    {
        throw std::runtime_error("Could not open input file: " + inputFilePath);
    }

    std::string token;
    while (file >> token)
    {
        try
        {
            // 全て小数として扱って、lwの場合に整数に変換する
            float floatValue = std::stof(token);
            int32_t intValue = std::bit_cast<int32_t>(floatValue);
            dMemory.inputData.emplace_back(intValue);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to process token " + token + " - ");
        }
    }
    std::cerr << "Completed loading input file" << std::endl;
#ifdef DEBUG
    std::cerr << "Input Data:" << std::endl;
    for (const auto &data : dMemory.inputData)
    {
        std::cerr << data << " ";
    }
    std::cerr << std::endl;
#endif
}

void Simulator::printRegisters(int regType) const
{
    if (regType == PC || regType == ALLREG)
    {
        std::cerr << std::setw(8) << "PC:"
                  << std::setw(15) << std::hex << getPC()
                  << std::setw(15) << std::dec << "(" + std::to_string(getPC()) + ")" << std::endl;
    }
    if (regType == REG || regType == ALLREG)
    {
        std::cerr << "________Registers state________" << std::endl;
        for (int i = 0; i < REG_COUNT; i++)
        {
            std::cerr << std::setw(8) << ("x" + std::to_string(i) + ":")
                      << std::setw(15) << std::hex << getRegister(i)
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<int32_t>(getRegister(i))) + ")" << std::endl;
        }
    }

    if (regType == FPREG || regType == ALLREG)
    {
        std::cerr << "________FpRegisters state________" << std::endl;
        for (int i = 0; i < FPREG_COUNT; i++)
        {
            std::cerr << std::setw(8) << ("fp" + std::to_string(i) + ":")
                      << std::setw(15) << std::hex << getFpRegister(i)
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<float>(getFpRegister(i))) + ")" << std::endl;
        }
    }
}

void Simulator::detectPrevLoad(int32_t rs1, int32_t rs2)
{

    // 1命令前にロードしたレジスタであるかを検出
    if (rs1 == prevLoadReg || rs2 == prevLoadReg)
    {
        hazardRAW++;
    }
}

uint32_t Simulator::getIndex(uint32_t pc) const
{
    // pc[15:9] ^ pc[8:2]
    uint32_t upper = (pc >> 9) & 0x7F;
    uint32_t lower = (pc >> 2) & 0x7F;
    return upper ^ lower;
}

void Simulator::updateCounter(uint8_t &counter, bool isTaken)
{

    if (isTaken)
    {
        if (counter < 3)
            ++counter;
    }
    else
    {
        if (counter > 0)
            --counter;
    }
}

bool Simulator::predict(uint8_t counter) const
{
    return counter >= 2; // counter = 2 or 3 then Taken
}

// 分岐予測
void Simulator::branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken)
{
    uint32_t index = getIndex(getPC());
    uint8_t previousCounter = patternHistoryTable[index];
    if (availableLog)
        std::cerr << "PC: " << getPC()
                  << ", Prediction: " << (previousCounter >= 2 ? (previousCounter == 3 ? "Strongly Taken" : "Weakly Taken") : (previousCounter == 0 ? "Strongly UnTaken" : "Weakly UnTaken"))
                  << ", Actual: " << (isTaken ? "Taken" : "UnTaken") << std::endl;

    bool predictedTaken = predict(previousCounter);
    logBranchPrediction();
    // フラッシュ
    if (predictedTaken != isTaken)
    {
        if (availableLog)
            std::cerr << "Pipeline flushed due to misprediction." << std::endl;
        logFlush();
    }
    else
    {
        if (availableLog)
            std::cerr << "Prediction matched!" << std::endl;
    }

    if (isTaken)
    {
        setPC(imm);
    }
    else
    {
        setPC(getPC() + 1);
    }
    updateCounter(patternHistoryTable[index], isTaken);
    if (availableLog)
        std::cerr << "PHT Index " << index << ": Counter Value Changed from " << static_cast<int>(previousCounter) << " to " << static_cast<int>(patternHistoryTable[index]) << std::endl;
}

inline void Simulator::updatePrevLoadReg(int currLoadReg)
{
    prevLoadReg = currLoadReg;
}

void Simulator::printInstruction(uint32_t instruction) const
{
    if (availableLog)
        std::cerr << "Executing: " << instToString(instruction) << std::endl;
}

void Simulator::executeInstruction(uint32_t instruction)
{
    logInstAddr(getPC() + 1);
    int currLoadReg = NULLREG;
    const uint32_t opcode = getOpcode(instruction);
    switch (opcode)
    {
    case 0x1:
    {
        // R-type (add, sub)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        detectPrevLoad(rs1, rs2);

        if (subop == 0x0)
        {
            logInstruction("add");
            if (rs1 == 0 || rs2 == 0)
            {
                mvCount++;
            }
            setRegister(rd, getRegister(rs1) + getRegister(rs2));
        }
        else if (subop == 0x1)
        {
            logInstruction("sub");
            setRegister(rd, getRegister(rs1) - getRegister(rs2));
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0x2:
    {
        // addi, lui, slli, srli
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            detectPrevLoad(rs1, NOLOADREG);

            int32_t imm = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((imm >> 13) & 1)
            {
                imm -= 1 << 14;
            }
            logInstruction("addi");
            // count mvi
            if (rs1 == 0)
            {
                mviCount++;
            }
            setRegister(rd, getRegister(rs1) + imm);
        }
        else if (subop == 0x1)
        {
            detectPrevLoad(rs1, NOLOADREG);

            const int32_t shamt = (instruction >> 6) & 0x3;
            if (!(shamt >= 0 && shamt <= 3))
            {
                throw std::runtime_error("Warning: shamt is not between 0 and 3");
            }
            logInstruction("slli");
            setRegister(rd, getRegister(rs1) << shamt);
        }
        else if (subop == 0x2)
        {
            detectPrevLoad(rs1, NOLOADREG);

            const int32_t shamt = (instruction >> 6) & 0x3;
            if (!(shamt >= 0 && shamt <= 3))
            {
                throw std::runtime_error("Warning: shamt is not between 0 and 3");
            }
            logInstruction("srli");
            setRegister(rd, getRegister(rs1) >> shamt);
        }
        else if (subop == 0x1)
        {
            // ?-type (lui)
            const int32_t imm = ((((instruction >> 20) & 0xFFF) << 8) | ((instruction >> 6) & 0xFF)) << 12;
            logInstruction("lui");
            setRegister(rd, imm);
            setPC(getPC() + 1);
            break;
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0x3:
    {
        // B-type (beq, bne, blt, bge)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);

        bool isTaken = false;
        detectPrevLoad(rs1, rs2);
        if (subop == 0x0)
        {
            logInstruction("beq");
            isTaken = (getRegister(rs1) == getRegister(rs2));
        }
        else if (subop == 0x1)
        {
            logInstruction("bne");
            isTaken = (getRegister(rs1) != getRegister(rs2));
        }
        else if (subop == 0x2)
        {
            logInstruction("blt");
            isTaken = (getRegister(rs1) < getRegister(rs2));
        }
        else if (subop == 0x3)
        {
            logInstruction("bge");
            isTaken = (getRegister(rs1) >= getRegister(rs2));
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        branchPrediction(rs1, rs2, imm, isTaken); // 分岐予測の実行
        break;
    }
    case 0x4:
    {
        // J-type (jal)
        const uint32_t rd = getRd(instruction);
        int32_t imm = getImmediate(instruction);
        logInstruction("jal");
        setRegister(rd, getPC() + 1);
        setPC(imm);
        break;
    }
    case 0x5:
    {
        // I-type (jalr)
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        detectPrevLoad(rs1, NOLOADREG);

        logInstruction("jalr");
        setRegister(rd, getPC() + 1);
        setPC(getRegister(rs1));
        break;
    }
    case 0x8:
    {
        // I-type (lw), R-type (lwr)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            detectPrevLoad(rs1, NOLOADREG);

            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            const int64_t address = getRegister(rs1) + offset;
            logInstruction("lw");
            if (offset >= 0)
            {
                lwNonNegativeCount++;
            }
            else
            {
                lwNegativeCount++;
            }
            setRegister(rd, dMemory.loadWord(address, true));
            if (prevInstIsLoadOrStore)
            {
                loadStoreSequence++;
            }
            currentInstIsLoadOrStore = true;
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            detectPrevLoad(rs1, rs2);

            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction("lwr");
            setRegister(rd, dMemory.loadWord(address, true));
            if (prevInstIsLoadOrStore)
            {
                loadStoreSequence++;
            }
            currentInstIsLoadOrStore = true;
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);

        currLoadReg = rd;
        break;
    }
    case 0x9:
    {
        // S-type (sw)
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t offset = getImmediate(instruction);
        // 符号ビットを処理
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }

        detectPrevLoad(rs1, rs2);
        const int64_t address = getRegister(rs1) + offset;
        logInstruction("sw"); // 命令の記録
        if (offset >= 0)
        {
            swNonNegativeCount++;
        }
        else
        {
            swNegativeCount++;
        }
        dMemory.storeWord(address, getRegister(rs2));
        if (prevInstIsLoadOrStore)
        {
            loadStoreSequence++;
        }
        currentInstIsLoadOrStore = true;
        setPC(getPC() + 1);
        break;
    }
    case 0xA:
    {
        // I-type, (flw), R-type (flwr)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            detectPrevLoad(rs1, NOLOADREG);

            const int64_t address = getRegister(rs1) + offset;
            logInstruction("flw"); // 命令の記録
            if (rs1 == 0 && offset < 128 && offset >= 64)
            {
                flwImmCount++;
            }
            if (offset >= 0)
            {
                flwNonNegativeCount++;
            }
            else
            {
                flwNegativeCount++;
            }
            setFpRegister(rd, dMemory.loadWord(address, false));
            if (prevInstIsLoadOrStore)
            {
                loadStoreSequence++;
            }
            currentInstIsLoadOrStore = true;
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);

            detectPrevLoad(rs1, rs2);

            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction("flwr"); // 命令の記録
            setFpRegister(rd, dMemory.loadWord(address, false));
            if (prevInstIsLoadOrStore)
            {
                loadStoreSequence++;
            }
            currentInstIsLoadOrStore = true;
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);

        currLoadReg = rd + REG_COUNT; // Register:0~REG_COUNT-1, fpRegister: REG_COUNT~REG_COUNT+FPREG_COUNT-1
        break;
    }
    case 0xB:
    {
        // S-type (fsw)
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 13) & 1)
        {
            imm -= 1 << 14;
        }
        detectPrevLoad(rs1, rs2);

        const int64_t address = getRegister(rs1) + imm;
        logInstruction("fsw"); // 命令の記録
        if (imm >= 0)
        {
            fswNonNegativeCount++;
        }
        else
        {
            fswNegativeCount++;
        }
        dMemory.storeWord(address, getFpRegister(rs2));
        if (prevInstIsLoadOrStore)
        {
            loadStoreSequence++;
        }
        currentInstIsLoadOrStore = true;
        setPC(getPC() + 1);
        break;
    }
    case 0xC:
    {
        // ftoi, flt, feq
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        if (fpuop == 0x60)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("ftoi");
            setRegister(rd, fpu.ftoi(getFpRegister(rs1)));
        }
        else if (fpuop == 0x0)
        {

            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("flt");
            setRegister(rd, fpu.flt(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x1)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("feq");
            setRegister(rd, fpu.feq(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0xD:
    {
        // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        if (fpuop == 0x9)
        {

            detectPrevLoad(rs1, NOLOADREG);
            logInstruction("itof");
            setFpRegister(rd, fpu.itof(getRegister(rs1)));
        }
        else if (fpuop == 0x0)
        {

            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("fadd");
            setFpRegister(rd, fpu.fadd(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x1)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("fsub");
            setFpRegister(rd, fpu.fsub(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x2)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("fmul");
            setFpRegister(rd, fpu.fmul(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x3)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction("fdiv");
            setFpRegister(rd, fpu.fdiv(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x4)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("fmv");
            setFpRegister(rd, getFpRegister(rs1));
        }
        else if (fpuop == 0x5)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("fneg");
            setFpRegister(rd, fpu.fneg(getFpRegister(rs1)));
        }
        else if (fpuop == 0x7)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("fabs");
            setFpRegister(rd, fpu.fabs(getFpRegister(rs1)));
        }
        else if (fpuop == 0x7)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("fsqrt");
            setFpRegister(rd, fpu.fsqrt(getFpRegister(rs1)));
        }
        else if (fpuop == 0x8)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction("ffloor");
            setFpRegister(rd, fpu.ffloor(getFpRegister(rs1)));
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0x6:
    {

        logInstruction("ebreak");
        isBreakpoint = true;
        setPC(getPC() + 4);
        break;
    }
    default:
        std::stringstream ss;
        ss << "Unknown instruction 0x" << std::hex << instruction;
        throw std::runtime_error(ss.str());
    }
    prevInstIsLoadOrStore = currentInstIsLoadOrStore;
    currentInstIsLoadOrStore = false;
    updatePrevLoadReg(currLoadReg);
}

void Simulator::printCacheHitMissCounts() const
{
    const uint64_t hitCount = dMemory.getHitCount();
    const uint64_t missCount = dMemory.getMissCount();
    std::cerr << "Cache Hit Count: " << hitCount << std::endl;
    std::cerr << "Cache Miss Count: " << missCount << std::endl;
    std::cerr << "Cache Hit Rate: " << (double)hitCount / (double)(hitCount + missCount) << std::endl;
}

// ログの出力
void Simulator::printLog()
{
    uint64_t estimatedClock = 0;
    std::cerr << "Step : " << getStep() << std::endl;
    printProgram(true);
    printRegisters(ALLREG);
    if (enableICount)
    {
        printInstAddrCounts();
    }
    if (enableIStats)
    {
        printInstStats();
    }
    Log::printLog();
    std::cerr << "Detected RAW hazard: " << hazardRAW << std::endl;
    if (enableCache)
    {
        printCacheHitMissCounts();
        if (enableDebug)
        {
            dMemory.printCacheState();
        }
    }
    std::cerr << "jal + jalr: " << instructionCounts["jal"] + instructionCounts["jalr"] << std::endl;
    std::cerr << "Sequencial load and store: " << loadStoreSequence << std::endl;
    std::cerr << "________Estimation from data________" << std::endl;
    estimatedClock += 4;
    estimatedClock += totalInstructions;
    estimatedClock += (instructionCounts["jal"] + instructionCounts["jalr"]) * 2;
    estimatedClock += dMemory.getHitCount();
    estimatedClock += dMemory.getMissCount() * 14;
    estimatedClock += hazardRAW;
    estimatedClock += flushCount * 2;
    estimatedClock += loadStoreSequence;

    std::cerr << "Estimated clock: " << estimatedClock << std::endl;
    double estimatedTime = estimatedClock / CPUFREQUENCY;
    std::cerr << "Estimated execution time: " << estimatedTime << "sec" << std::endl;
    std::cerr << "Estimated instruction per clock: " << totalInstructions / (double)estimatedClock << std::endl;
    std::cerr << "Estimated instruction per sec: " << totalInstructions / estimatedTime << std::endl;
}

void Simulator::printOutput()
{
    std::ofstream file(outputFilePath);
    if (!file)
    {
        throw std::runtime_error("Could not open output file: " + outputFilePath);
    }
    for (const auto &o : dMemory.output)
    {
        char output_c = o & 0xFF;
        file << output_c;
        if (enableStdout)
        {
            std::cout << output_c;
        }
    }
    file.close();
}

void Simulator::runProgram(int outputRegNum)
{
#ifdef DEBUG
    std::cerr << "________DEBUG_MODE________" << std::endl;
#endif // DEBUG
    std::cerr << "________Simulating Program________" << std::endl;
    if (enableGDB)
    {
        std::cerr << "________GDB MODE________" << std::endl;
    }
    step = 0;

    // GDB実行
    if (enableGDB)
    {
        bool isQuit = false;
        std::ostringstream buffer;
        bool breakMode = false;
        bool isUnknownCommand = false;
        std::stringstream gdbCommandLine;
        std::string gdbCommand;
        std::string prevGdbCommand;
        int rep = 0;
        while (true)
        {
            if (rep == 0)
            {
                gdbCommand = "";
                gdbCommandLine.clear();
                std::getline(std::cin, gdbCommand);
                if (gdbCommand.empty())
                {
                    gdbCommand = prevGdbCommand;
                    // 改行の上書き
                    std::cerr << "\033[A\33[2K\r";

                    std::cerr << gdbCommand << std::endl;
                }
                gdbCommandLine.str(gdbCommand);
                prevGdbCommand = gdbCommand;
                gdbCommandLine >> gdbCommand;
                rep = 1;
                try
                {
                    rep = std::stoi(gdbCommand);

                    gdbCommandLine >> gdbCommand;
                }
                catch (const std::invalid_argument &e)
                {
                    // この場合はstoiにint化できる文字列が渡されていない
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: out of range" << std::endl;
                }
            }
            for (int i = 0; i < rep; ++i)
            {
                if (!breakMode)
                {

                    {
                        CerrRedirect redirect(buffer);
                        if (gdbCommand == "r")
                        {
                        }
                        else
                        {
                            buffer.str("");

                            if (gdbCommand == "l")
                            {
                                if (rep == 1)
                                {
                                    printProgram(true);
                                    std::cerr << std::endl;
                                }
                            }
                            else if (gdbCommand == "info" || gdbCommand == "i")
                            {
                                gdbCommandLine >> gdbCommand;
                                if (gdbCommand == "reg" || gdbCommand == "r")
                                {
                                    gdbCommandLine >> gdbCommand;
                                    // info regの後に何もない場合、gdbCommandは不変
                                    if (gdbCommand == "reg" || gdbCommand == "r")
                                    {
                                        printRegisters(REG);
                                    }
                                    else if (rep == 1)
                                    {
                                        int i = std::stoi(gdbCommand);
                                        std::cerr << std::setw(8) << ("x" + std::to_string(i) + ":")
                                                  << std::setw(15) << std::hex << getRegister(i)
                                                  << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<int32_t>(getRegister(i))) + ")" << std::endl;
                                    }
                                }
                                else if (gdbCommand == "fpreg" || gdbCommand == "f" || gdbCommand == "fp")
                                {
                                    gdbCommandLine >> gdbCommand;
                                    // info fpregの後に何もない場合、gdbCommandは不変
                                    if (gdbCommand == "fpreg" || gdbCommand == "f" || gdbCommand == "fp")
                                    {
                                        printRegisters(REG);
                                    }
                                    else if (rep == 1)
                                    {
                                        int i = std::stoi(gdbCommand);
                                        std::cerr << std::setw(8) << ("fp" + std::to_string(i) + ":")
                                                  << std::setw(15) << std::hex << getFpRegister(i)
                                                  << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<float>(getFpRegister(i))) + ")" << std::endl;
                                    }
                                }
                                else if (gdbCommand == "pc" || gdbCommand == "p")
                                {
                                    if (rep == 1)
                                    {
                                        printRegisters(PC);
                                    }
                                }
                                else
                                {
                                    if (rep == 1)
                                    {
                                        printRegisters(ALLREG);
                                        std::cerr << std::endl;
                                    }
                                }
                            }
                            else if (gdbCommand == "s")
                            {
                                const uint32_t instruction = loadInstruction(pc);

                                if (rep == 1)
                                {
                                    std::cerr << "Step : " << step << std::endl;
#ifdef DEBUG
                                    std::cerr << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG

                                    printProgram(true);
                                    printInstruction(instruction);
                                }
                                executeInstruction(instruction);
                                step++;
                                if (rep == 1)
                                {
                                    std::cerr << std::endl;
                                }
                            }
                            else if (gdbCommand == "c")
                            {
                                if (rep == 1)
                                {
                                    std::cerr << "heading to eBreak..." << std::endl;
                                    printBoundary();
                                }
                                breakMode = true;
                            }
                            else if (gdbCommand == "quit" || gdbCommand == "q")
                            {
                                isQuit = true;
                                break;
                            }
                            else
                            {

                                isUnknownCommand = true;
                            }
                        }
                    }
                    if (isUnknownCommand)
                    {
                        if (rep == 1)
                        {
                            std::cerr << "Unknown command: " << gdbCommand << std::endl;
                        }
                        rep = 1;
                    }
                    else
                    {
                        if (rep == 1)
                        {
                            std::cerr << buffer.str();
                        }
                        if (breakMode)
                        {

                            buffer.str("");
                        }
                    }
                    // update
                    rep--;
                    isUnknownCommand = false;
                }
                while (breakMode)
                {
                    std::cerr << buffer.str();
                    buffer.str("");
                    {
                        CerrRedirect redirect(buffer);
                        const uint32_t instruction = loadInstruction(pc);

                        executeInstruction(instruction);
                        step++;
                    }
                    if (isBreakpoint)
                    {
                        if (rep == 0)
                        {
                            std::cerr << buffer.str();
                            std::cerr << "Program reached ebreak at Step: " << step - 1 << std::endl;
                            std::cerr << std::endl;
                        }
                        breakMode = false;
                        isBreakpoint = false;
                    }
                    else
                    {
                        buffer.str("");
                    }
                }
            }
            if (isQuit)
            {
                break;
            }
        }
    }
    // 通常実行
    else
    {
        if (outputSize <= 2)
        {
            while (maxStep > step && !isBreakpoint)
            {
                const uint32_t instruction = loadInstruction(pc);
                if (enableDebug)
                {
                    printInstruction(instruction);
                }
                executeInstruction(instruction);
                step++;
            }
        }
        else
        {
            pbar::pbar bar(outputSize, 100);
            bar.set_description("[Simulation]");
            bar.init();
            bar.enable_recalc_console_width(1);
            uint64_t prevLineOutputCount = 0;
            while (maxStep > step && !isBreakpoint)
            {
                const uint32_t instruction = loadInstruction(pc);
                if (enableDebug)
                {
                    printInstruction(instruction);
                }
                executeInstruction(instruction);
                step++;

                if (prevLineOutputCount != dMemory.lineOutputCount)
                {
                    bar.tick();
                }
                prevLineOutputCount = dMemory.lineOutputCount;
            }
        }
        if (isBreakpoint)
        {
            std::cerr << "Program reached breakpoint" << std::endl;
        }
        std::cerr << "________Simulation Ended________" << std::endl;
        if (outputRegNum >= 0)
        {
            // 0-31はレジスタに対応
            if (outputRegNum >= 0 && outputRegNum < REG_COUNT)
            {
                std::cout << "x" << outputRegNum << ": " << std::hex << getRegister(outputRegNum) << std::dec << std::endl;
            }
            // 32以上はfpレジスタに対応
            else if (outputRegNum >= REG_COUNT && outputRegNum < REG_COUNT + FPREG_COUNT)
            {
                std::cout << "fp" << outputRegNum - REG_COUNT << ": " << std::hex << getFpRegister(outputRegNum - REG_COUNT) << std::dec << std::endl;
            }
            else
            {
                throw std::out_of_range("-reg <RegNum>: RegNum isn't between 0 to" + std::to_string(REG_COUNT + FPREG_COUNT - 1));
            }
        }
        printOutput();
    }
}
