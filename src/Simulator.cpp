#include "Simulator.hpp"
#include "Pipeline.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <bit>
#include <pbar.hpp>

Simulator::Simulator(OptionHandler &op)
    : iCache(iMemory), dMemory(op.memorySize,
                               INPUT_ADDRESS * 4,
                               OUTPUT_ADDRESS * 4,
                               op.l1Lines,
                               op.l2Lines,
                               op.lineSize,
                               op.l2Associativity)
{
    DMEMORY_SIZE = op.memorySize;
    registers[0] = 0;                       // x0
    registers[2] = (DMEMORY_SIZE >> 2) - 4; // sp
    pc = 0;
    isBreakpoint = false;
    enableCache = !op.noCache;
    enableICount = op.enableICount;
    enableIStats = op.enableIStats;
    enableDebug = op.enableDebug;
    enableStdout = op.enableStdout;
    enableICache = op.enableICache;
    outputSize = op.imageSize * op.imageSize + 2;
    enableGDB = op.enableGDB;
    maxStep = op.maxStep;
    dMemory.availableCache = enableCache;
    availableLog = op.enableGDB || op.enableDebug;
    dMemory.availableLog = availableLog;
    outputFilePath = op.outputFilePath;
    enablePipeline = !op.enableNoPipeline;
    if (enablePipeline)
    {
        pipeline = new Pipeline(*this);
    }
}

Simulator::~Simulator()
{
    delete pipeline;
}

void Simulator::storeInstruction(int32_t address, uint64_t instruction)
{
    if (address < 0 || address >= IMEMORY_SIZE >> 2) [[unlikely]]
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
    iMemory[address] = instruction;
}
std::string Simulator::instToString(uint64_t instruction) const
{
    std::ostringstream sstr;
    const uint32_t opcode = getOpcode(instruction);

    switch (opcode)
    {
    case 0x0:
        sstr << "nop";
        break;
    case 0x1:
    {
        const uint32_t subop = getSubop(instruction);
        const uint32_t subsubop = getSubsubop(instruction);

        if (subop == 0x0)
        {
            const uint32_t rd = getRd(instruction);
            const uint32_t rs1 = getRs1(instruction);
            const uint32_t rs2 = getRs2(instruction);

            if (subsubop == 0x0)
            {
                sstr << "add x" << rd << ", x" << rs1 << ", x" << rs2;
            }
            else if (subsubop == 0x1)
            {
                sstr << "sub x" << rd << ", x" << rs1 << ", x" << rs2;
            }
            else if (subsubop == 0x2)
            {
                const uint32_t shamt = getShamt(instruction);
                sstr << "slli x" << rd << ", x" << rs1 << ", " << shamt;
            }
            else if (subsubop == 0x3)
            {
                const uint32_t shamt = getShamt(instruction);
                sstr << "srli x" << rd << ", x" << rs1 << ", " << shamt;
            }
        }
        else if (subop == 0x1)
        {
            const uint32_t rd = getRd(instruction);
            const uint32_t rs1 = getRs1(instruction);
            int32_t imm = getOffset6_8(instruction);
            if ((imm >> 13) & 1)
            {
                imm -= 1 << 14;
            }
            sstr << "addi x" << rd << ", x" << rs1 << ", " << imm;
        }
        else if (subop == 0x2)
        {
            const uint32_t rd = getRd(instruction);
            int32_t imm = ((((instruction >> 20) & 0xFFF) << 8) | ((instruction >> 6) & 0xFF)) << 12;
            sstr << "lui x" << rd << ", " << std::hex << (imm >> 12) << std::dec;
        }
        else if (subop == 0x3)
        {
            if (subsubop == 0x0)
            {
                const uint32_t rd = getRd(instruction);
                sstr << "in x" << rd;
            }
            else if (subsubop == 0x1)
            {
                const uint32_t rd = getRd(instruction);
                sstr << "fin fp" << rd;
            }
            else if (subsubop == 0x2)
            {
                const uint32_t rs1 = getRs1(instruction);
                sstr << "out x" << rs1;
            }
        }
        break;
    }
    case 0x2:
    {
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);

        if (branchop == 0x0)
        {
            sstr << "beq x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        else if (branchop == 0x1)
        {
            sstr << "bne x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        break;
    }
    case 0x6:
    {
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);

        if (branchop == 0x0)
        {
            sstr << "blt x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        else if (branchop == 0x1)
        {
            sstr << "bge x" << rs1 << ", x" << rs2 << ", " << imm;
        }
        break;
    }
    case 0xA:
    {
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        const uint32_t m1 = getM1(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t imm = getImmediate(instruction);

        std::string rs1Str = formatFpRegWithModifier(rs1, m1);
        std::string rs2Str = formatFpRegWithModifier(rs2, m2);

        if (branchop == 0x0)
        {
            sstr << "bflt " << rs1Str << ", " << rs2Str << ", " << imm;
        }
        else if (branchop == 0x1)
        {
            sstr << "bfge " << rs1Str << ", " << rs2Str << ", " << imm;
        }
        break;
    }
    case 0xE:
    {
        const uint32_t rd = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        sstr << "jal x" << rd << ", " << imm;
        break;
    }
    case 0xF:
    {
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        sstr << "jalr x" << rd << ", x" << rs1;
        break;
    }
    case 0x3:
    {
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = getOffset6_8(instruction);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            sstr << "lw x" << rd << ", " << offset << "(x" << rs1 << ")";
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            sstr << "lwr x" << rd << ", x" << rs2 << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x4:
    {
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t offset = getOffset14(instruction);
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }
        sstr << "sw x" << rs2 << ", " << offset << "(x" << rs1 << ")";
        break;
    }
    case 0x5:
    {
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = getOffset6_8(instruction);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            sstr << "flw fp" << rd << ", " << offset << "(x" << rs1 << ")";
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            sstr << "flwr fp" << rd << ", x" << rs2 << "(x" << rs1 << ")";
        }
        break;
    }
    case 0x7:
    {
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t offset = getOffset14(instruction);
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }

        std::string rs2Str = formatFpRegWithModifier(rs2, m2);
        sstr << "fsw " << rs2Str << ", " << offset << "(x" << rs1 << ")";
        break;
    }
    case 0x9:
    {
        sstr << "ebreak";
        break;
    }
    case 0xC:
    {
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t m1 = getM1(instruction);

        if (fpuop == 0x4)
        {
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            sstr << "ftoi x" << rd << ", " << rs1Str;
        }
        else if (fpuop == 0x0)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t m2 = getM2(instruction);
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            sstr << "flt x" << rd << ", " << rs1Str << ", " << rs2Str;
        }
        else if (fpuop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t m2 = getM2(instruction);
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            sstr << "feq x" << rd << ", " << rs1Str << ", " << rs2Str;
        }
        break;
    }
    case 0xD:
    {
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t m1 = getM1(instruction);

        if (fpuop == 0x7)
        {
            sstr << "itof fp" << rd << ", x" << rs1;
        }
        else if (fpuop == 0x0)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t m2 = getM2(instruction);
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            sstr << "fadd fp" << rd << ", " << rs1Str << ", " << rs2Str;
        }
        else if (fpuop == 0x2)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t m2 = getM2(instruction);
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            sstr << "fmul fp" << rd << ", " << rs1Str << ", " << rs2Str;
        }
        else if (fpuop == 0x3)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t m2 = getM2(instruction);
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            sstr << "fdiv fp" << rd << ", " << rs1Str << ", " << rs2Str;
        }
        else if (fpuop == 0x4)
        {
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            sstr << "fmv fp" << rd << ", " << rs1Str;
        }
        else if (fpuop == 0x5)
        {
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            sstr << "fsqrt fp" << rd << ", " << rs1Str;
        }
        else if (fpuop == 0x6)
        {
            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            sstr << "ffloor fp" << rd << ", " << rs1Str;
        }
        else if (fpuop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            const uint32_t rs3 = getRs3(instruction);
            const uint32_t m2 = getM2(instruction);
            const uint32_t m3 = getM3(instruction);

            std::string rs1Str = formatFpRegWithModifier(rs1, m1);
            std::string rs2Str = formatFpRegWithModifier(rs2, m2);
            std::string rs3Str;
            if (m3)
            {
                rs3Str = "-fp" + std::to_string(rs3);
            }
            else
            {
                rs3Str = "fp" + std::to_string(rs3);
            }

            sstr << "fmadd fp" << rd << ", " << rs1Str << ", " << rs2Str << ", " << rs3Str;
        }
        break;
    }
    default:
        sstr << "Unknown instruction " << std::hex << instruction;
        break;
    }

    return sstr.str();
}

std::string Simulator::formatFpRegWithModifier(uint32_t reg, uint32_t modifier) const
{
    std::string result;

    switch (modifier)
    {
    case 0:
        result = "fp" + std::to_string(reg);
        break;
    case 1:
        result = "-fp" + std::to_string(reg);
        break;
    case 2:
        result = "abs(fp" + std::to_string(reg) + ")";
        break;
    case 3:
        result = "-abs(fp" + std::to_string(reg) + ")";
        break;
    }

    return result;
}

void Simulator::loadMemoryFromBinary(const std::string &filename)
{
    std::cerr << "Load memory from binary file..." << std::endl;
    std::ifstream file(filename, std::ios::binary);
    if (!file) [[unlikely]]
    {
        throw std::runtime_error("Could not open binary file");
    }
#ifdef DEBUG

    std::cerr << "DEBUG MODE INSTRUCTION LIST" << std::endl;

#endif // DEBUG
    uint64_t instruction;
    uint32_t data;
    uint64_t address;
    file.read(reinterpret_cast<char *>(&dataSectionSize), sizeof(dataSectionSize));
    address = 64;
    for (unsigned int i = 0; i < dataSectionSize / sizeof(dataSectionSize); ++i)
    {
        file.read(reinterpret_cast<char *>(&data), sizeof(data));
        if (address < 0 || address >= DMEMORY_SIZE >> 2) [[unlikely]]
        {
            throw std::out_of_range("dMemory access out of bounds");
        }
        dMemory.isInitialized[address] = true;
        dMemory.mainMemory[address] = data;
        ++address;
        if (address >= DMEMORY_SIZE >> 2) [[unlikely]]
        {
            throw std::out_of_range("Program size exceeds dMemory limits");
        }
    }

    address = 0;
    while (file.read(reinterpret_cast<char *>(&instruction), sizeof(instruction)))
    {
        storeInstruction(address, instruction);
        ++address;
        if (address >= IMEMORY_SIZE >> 2) [[unlikely]]
        {
            throw std::out_of_range("Program size exceeds iMemory limits");
        }
    }
    instructionSize = address;
    instAddrCounts.resize(instructionSize, 0);
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
        const uint64_t instruction = iMemory[i];
        std::cerr << std::setw(15) << std::hex << i << std::dec
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
    std::cerr << std::left << std::setw(17) << "Number of `fmul`" << std::right << std::setw(28) << " with 0.5 (fhalf): " << fhalfCount << std::endl;
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
                const uint64_t instruction = iMemory[address];
                std::cerr << address << ": " << instToString(instruction);
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
            const uint64_t instruction = iMemory[i];
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
    if (!file) [[unlikely]]
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
        for (int i = 0; i < REG_COUNT; ++i)
        {
            std::cerr << std::setw(8) << ("x" + std::to_string(i) + ":")
                      << std::setw(15) << std::hex << getRegister(i)
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<int32_t>(getRegister(i))) + ")";

            std::cerr << std::endl;
        }
    }

    if (regType == FPREG || regType == ALLREG)
    {
        std::cerr << "________FpRegisters state________" << std::endl;
        for (int i = 0; i < FPREG_COUNT; ++i)
        {
            std::cerr << std::setw(8) << ("fp" + std::to_string(i) + ":")
                      << std::setw(15) << std::hex << getFpRegister(i)
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<float>(getFpRegister(i))) + ")";

            std::cerr << std::endl;
        }
    }
}

// 分岐予測
void Simulator::branchPrediction(int32_t imm, bool isTaken)
{
    int32_t pc = getPC();
    bool predictedTaken = predictor.predict(pc);
    uint8_t prediction = predictor.getPrediction(pc);
    logBranchPrediction();
    if (availableLog) [[unlikely]]
        std::cerr << "PC: " << pc
                  << ", Prediction: " << (prediction >= 2 ? (prediction == 3 ? "Strongly Taken" : "Weakly Taken") : (prediction == 0 ? "Strongly UnTaken" : "Weakly UnTaken"))
                  << ", Actual: " << (isTaken ? "Taken" : "UnTaken") << std::endl;

    // フラッシュ
    if (predictedTaken != isTaken) [[unlikely]]
    {
        if (availableLog) [[unlikely]]
            std::cerr << "Pipeline flushed due to misprediction." << std::endl;
        logFlush();
    }
    else
    {
        if (availableLog) [[unlikely]]
            std::cerr << "Prediction matched!" << std::endl;
    }
    predictor.update(pc, isTaken);
    if (isTaken)
    {
        setPC(imm);
    }
    else
    {
        setPC(pc + 1);
    }
}

bool Simulator::simulateCacheAccess(int32_t address, bool isStore)
{
    // キャッシュアクセスをシミュレート（値の読み書きは行わない）
    if (isStore)
    {
        return dMemory.checkCacheHit(address);
    }
    else
    {
        return dMemory.checkCacheHit(address);
    }
}

int Simulator::getCacheMissPenalty() const
{
    return dMemory.stallCycles;
}

void Simulator::executeInstructionInPipeline(uint64_t instruction, int32_t pc, int32_t rs1Value, int32_t rs2Value)
{
    // 分岐予測ミスとキャッシュミスのフラグをリセット
    branchMispredicted = false;
    instructionCacheMiss = false;

    uint32_t opcode = getOpcode(instruction);
    if (opcode != 0x8 && opcode != 0x9 && opcode != 0xA && opcode != 0xB)
    {
        // メモリ命令でない場合のみ実行
        if (opcode == 0x3 || opcode == 0x4 || opcode == 0x5 || opcode == 0xE || opcode == 0xF)
        {
            int32_t initialPc = this->pc;

            int32_t originalRs1 = 0, originalRs2 = 0;
            uint32_t rs1 = getRs1(instruction);
            uint32_t rs2 = getRs2(instruction);

            if (rs1 != 0)
            {
                originalRs1 = registers[rs1];
                registers[rs1] = rs1Value;
            }

            if (rs2 != 0)
            {
                originalRs2 = registers[rs2];
                registers[rs2] = rs2Value;
            }

            executeInstruction(instruction);

            // レジスタの値を元に戻す
            if (rs1 != 0)
            {
                registers[rs1] = originalRs1;
            }

            if (rs2 != 0)
            {
                registers[rs2] = originalRs2;
            }

            if (initialPc != this->pc)
            {
                branchMispredicted = true;
            }
        }
        else
        {
            // 分岐命令以外の場合は元の実装を使用
            executeInstruction(instruction);
        }
    }
}

void Simulator::printInstruction(uint64_t instruction) const
{
    if (availableLog) [[unlikely]]
        std::cerr << "Executing: " << instToString(instruction) << std::endl;
}

void Simulator::executeInstruction(uint64_t instruction)
{
    logInstAddr(getPC());
    int currLoadReg = NULLREG;
    if (enableICache && !enablePipeline)
    {
        bool hit = iCache.fetch(getPC());
        if (availableLog)
        {
            if (hit)
            {

                std::cerr << "Instruction Cache hit!" << std::endl;
            }
            else
            {
                std::cerr << "instruction Cache miss detected." << std::endl;
            }
        }
    }
    const uint32_t opcode = getOpcode(instruction);
    switch (opcode)
    {
    case 0x0:
        logInstruction(NOP);
        break;
    case 0x1:
    {
        // R-type (add, sub, slli, srli, addi, lui)
        const uint32_t subop = getSubop(instruction);
        const uint32_t subsubop = getSubsubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);

        detectPrevLoad(rs1, rs2);

        if (subop == 0x0)
        {
            if (subsubop == 0x0)
            {
                logInstruction(ADD);
                if (rs1 == 0 || rs2 == 0)
                {
                    ++mvCount;
                }
                setRegister(rd, getRegister(rs1) + getRegister(rs2));
            }
            else if (subsubop == 0x1)
            {
                logInstruction(SUB);
                setRegister(rd, getRegister(rs1) - getRegister(rs2));
            }
            else if (subsubop == 0x2)
            {
                detectPrevLoad(rs1, NOLOADREG);

                const int32_t shamt = getShamt(instruction);
                if (!(shamt >= 0 && shamt <= 3)) [[unlikely]]
                {
                    throw std::runtime_error("Error: shamt is not between 0 and 3");
                }
                logInstruction(SLLI);
                setRegister(rd, getRegister(rs1) << shamt);
            }
            else if (subsubop == 0x3)
            {
                detectPrevLoad(rs1, NOLOADREG);

                const int32_t shamt = getShamt(instruction);
                if (!(shamt >= 0 && shamt <= 3)) [[unlikely]]
                {
                    throw std::runtime_error("Error: shamt is not between 0 and 3");
                }
                logInstruction(SRLI);
                setRegister(rd, getRegister(rs1) >> shamt);
            }
            else [[unlikely]]
            {
                std::stringstream ss;
                ss << "Unknown instruction " << std::hex << instruction;
                throw std::runtime_error(ss.str());
            }
        }
        else if (subop == 0x1)
        {
            detectPrevLoad(rs1, NOLOADREG);

            int32_t imm = getOffset6_8(instruction);
            if ((imm >> 13) & 1)
            {
                imm -= 1 << 14;
            }
            logInstruction(ADDI);
            if (rs1 == 0)
            {
                // mvi
                ++mviCount;
            }
            setRegister(rd, getRegister(rs1) + imm);
        }
        else if (subop == 0x2)
        {
            // ?-type (lui)
            const int32_t imm = ((((instruction >> 20) & 0xFFF) << 8) | ((instruction >> 6) & 0xFF)) << 12;
            logInstruction(LUI);
            setRegister(rd, imm);
        }
        else if (subop == 0x3)
        {
            if (subsubop == 0x0)
            {
                // ?-type (in)
                logInstruction(INST_IN);
                setRegister(rd, dMemory.loadWord(INPUT_ADDRESS * 4, true));
            }
            else if (subsubop == 0x1)
            {
                // ?-type (fin)
                logInstruction(INST_FIN);
                setFpRegister(rd, dMemory.loadWord(INPUT_ADDRESS * 4, false));
            }
            else if (subsubop == 0x2)
            {
                // ?-type (out)
                detectPrevLoad(rs1, NOLOADREG);
                logInstruction(INST_OUT);
                dMemory.storeWord(OUTPUT_ADDRESS * 4, getRegister(rs1));
            }
            else [[unlikely]]
            {
                std::stringstream ss;
                ss << "Unknown instruction " << std::hex << instruction;
                throw std::runtime_error(ss.str());
            }
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        break;
    }
    case 0x2:
    {
        // B-type (beq, bne)
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        bool isTaken = false;
        detectPrevLoad(rs1, rs2);
        if (branchop == 0x0)
        {
            logInstruction(BEQ);
            isTaken = (getRegister(rs1) == getRegister(rs2));
        }
        else if (branchop == 0x1)
        {
            logInstruction(BNE);
            isTaken = (getRegister(rs1) != getRegister(rs2));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        branchPrediction(imm, isTaken); // 分岐予測の実行
        break;
    }
    case 0x6:
    {
        // B-type (blt, bge)
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        bool isTaken = false;
        detectPrevLoad(rs1, rs2);
        if (branchop == 0x0)
        {
            logInstruction(BLT);
            isTaken = (getRegister(rs1) < getRegister(rs2));
        }
        else if (branchop == 0x1)
        {
            logInstruction(BGE);
            isTaken = (getRegister(rs1) >= getRegister(rs2));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        branchPrediction(imm, isTaken); // 分岐予測の実行
        break;
    }
    case 0xA:
    {
        // B-type (bflt, bfge)
        const uint32_t branchop = getBranchop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        const uint32_t m1 = getM1(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t fprs1 = fpu.applyFpModifier(getFpRegister(rs1), m1);
        int32_t fprs2 = fpu.applyFpModifier(getFpRegister(rs2), m2);
        int32_t imm = getImmediate(instruction);
        bool isTaken = false;
        detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
        if (branchop == 0x0)
        {
            logInstruction(BFLT);
            isTaken = fpu.flt(fprs1, fprs2);
        }
        else if (branchop == 0x1)
        {
            logInstruction(BFGE);
            isTaken = !fpu.flt(fprs1, fprs2);
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        branchPrediction(imm, isTaken); // 分岐予測の実行
        break;
    }
    case 0xE:
    {
        // J-type (jal)
        const uint32_t rd = getRs2(instruction); // jalは特例でrs2の位置にrd
        int32_t imm = getImmediate(instruction);
        logInstruction(JAL);
        setRegister(rd, getPC() + 1);
        setPC(imm);
        break;
    }
    case 0xF:
    {
        // I-type (jalr)
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        detectPrevLoad(rs1, NOLOADREG);

        logInstruction(JALR);
        setRegister(rd, getPC() + 1);
        setPC(getRegister(rs1));
        break;
    }
    case 0x3:
    {
        // I-type (lw), R-type (lwr)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            detectPrevLoad(rs1, NOLOADREG);

            int32_t offset = getOffset6_8(instruction);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            const int64_t address = getRegister(rs1) + offset;
            logInstruction(LW);
            if (offset >= 0)
            {
                ++lwNonNegativeCount;
            }
            else
            {
                ++lwNegativeCount;
            }
            setRegister(rd, dMemory.loadWord(address * 4, true));
            if (prevInstIsLoadOrStore)
            {
                ++loadStoreSequence;
            }
            currentInstIsLoadOrStore = true;
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);
            detectPrevLoad(rs1, rs2);

            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction(LWR);
            setRegister(rd, dMemory.loadWord(address * 4, true));
            if (prevInstIsLoadOrStore)
            {
                ++loadStoreSequence;
            }
            currentInstIsLoadOrStore = true;
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        currLoadReg = rd;
        break;
    }
    case 0x4:
    {
        // S-type (sw)
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t offset = getOffset14(instruction);
        // 符号ビットを処理
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }

        detectPrevLoad(rs1, rs2);
        const int64_t address = getRegister(rs1) + offset;
        logInstruction(SW); // 命令の記録
        if (offset >= 0)
        {
            ++swNonNegativeCount;
        }
        else
        {
            ++swNegativeCount;
        }
        dMemory.storeWord(address * 4, getRegister(rs2));
        if (prevInstIsLoadOrStore)
        {
            ++loadStoreSequence;
        }
        currentInstIsLoadOrStore = true;
        break;
    }
    case 0x5:
    {
        // I-type, (flw), R-type (flwr)
        const uint32_t subop = getSubop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);

        if (subop == 0x0)
        {
            int32_t offset = getOffset6_8(instruction);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            detectPrevLoad(rs1, NOLOADREG);

            const int64_t address = getRegister(rs1) + offset;
            logInstruction(FLW); // 命令の記録
            if (rs1 == 0 && offset < 128 && offset >= 64)
            {
                ++flwImmCount;
            }
            if (offset >= 0)
            {
                ++flwNonNegativeCount;
            }
            else
            {
                ++flwNegativeCount;
            }
            setFpRegister(rd, dMemory.loadWord(address * 4, false));
            if (prevInstIsLoadOrStore)
            {
                ++loadStoreSequence;
            }
            currentInstIsLoadOrStore = true;
        }
        else if (subop == 0x1)
        {
            const uint32_t rs2 = getRs2(instruction);

            detectPrevLoad(rs1, rs2);

            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction(FLWR); // 命令の記録
            setFpRegister(rd, dMemory.loadWord(address * 4, false));
            if (prevInstIsLoadOrStore)
            {
                ++loadStoreSequence;
            }
            currentInstIsLoadOrStore = true;
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        currLoadReg = rd + REG_COUNT; // Register:0~REG_COUNT-1, fpRegister: REG_COUNT~REG_COUNT+FPREG_COUNT-1
        break;
    }
    case 0x7:
    {
        // S-type (fsw)
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getOffset14(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t fprs2 = fpu.applyFpModifier(getFpRegister(rs2), m2);

        // 符号ビットを処理
        if ((imm >> 13) & 1)
        {
            imm -= 1 << 14;
        }
        detectPrevLoad(rs1, rs2 + REG_COUNT);

        const int64_t address = getRegister(rs1) + imm;
        logInstruction(FSW); // 命令の記録
        if (imm >= 0)
        {
            ++fswNonNegativeCount;
        }
        else
        {
            ++fswNegativeCount;
        }
        dMemory.storeWord(address * 4, fprs2);
        if (prevInstIsLoadOrStore)
        {
            ++loadStoreSequence;
        }
        currentInstIsLoadOrStore = true;
        break;
    }
    case 0xC:
    {
        // ftoi, flt, feq
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        const uint32_t m1 = getM1(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t fprs1 = fpu.applyFpModifier(getFpRegister(rs1), m1);
        int32_t fprs2 = fpu.applyFpModifier(getFpRegister(rs2), m2);
        if (fpuop == 0x4)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction(FTOI);
            setRegister(rd, fpu.ftoi(fprs1));
        }
        else if (fpuop == 0x0)
        {

            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction(FLT);
            setRegister(rd, fpu.flt(fprs1, fprs2));
        }
        else if (fpuop == 0x1)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction(FEQ);
            setRegister(rd, fpu.feq(fprs1, fprs2));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        break;
    }
    case 0xD:
    {
        // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor, fmadd
        const uint32_t fpuop = getFpuop(instruction);
        const uint32_t rd = getRd(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        const uint32_t m1 = getM1(instruction);
        const uint32_t m2 = getM2(instruction);
        int32_t fprs1 = fpu.applyFpModifier(getFpRegister(rs1), m1);
        int32_t fprs2 = fpu.applyFpModifier(getFpRegister(rs2), m2);
        if (fpuop == 0x7)
        {
            detectPrevLoad(rs1, NOLOADREG);
            logInstruction(ITOF);
            setFpRegister(rd, fpu.itof(getRegister(rs1)));
        }
        else if (fpuop == 0x0)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            if ((m2 & 1) == 1)
                logInstruction(FSUB);
            else
                logInstruction(FADD);
            setFpRegister(rd, fpu.fadd(fprs1, fprs2));
        }
        else if (fpuop == 0x2)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction(FMUL);
            setFpRegister(rd, fpu.fmul(fprs1, fprs2));
        }
        else if (fpuop == 0x3)
        {
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            logInstruction(FDIV);
            setFpRegister(rd, fpu.fdiv(fprs1, fprs2));
        }
        else if (fpuop == 0x4)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction(FMV);
            setFpRegister(rd, fprs1);
        }
        else if (fpuop == 0x5)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction(FSQRT);
            setFpRegister(rd, fpu.fsqrt(fprs1));
        }
        else if (fpuop == 0x6)
        {
            detectPrevLoad(rs1 + REG_COUNT, NOLOADREG);
            logInstruction(FFLOOR);
            setFpRegister(rd, fpu.ffloor(fprs1));
        }
        else if (fpuop == 0x1)
        {
            const uint32_t rs3 = getRs3(instruction);
            const uint32_t m3 = getM3(instruction);
            int32_t fprs3 = fpu.applyFpModifier(getFpRegister(rs3), m3);
            detectPrevLoad(rs1 + REG_COUNT, rs2 + REG_COUNT);
            detectPrevLoad(rs3 + REG_COUNT, NOLOADREG);
            logInstruction(FMADD);
            setFpRegister(rd, fpu.fadd(fpu.fmul(fprs1, fprs2), fprs3));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction " << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        break;
    }
    case 0x9:
    {
        logInstruction(EBREAK);
        setBreakpoint(true);
        break;
    }
    [[unlikely]] default:
        std::stringstream ss;
        ss << "Unknown instruction " << std::hex << instruction;
        throw std::runtime_error(ss.str());
    }
    prevInstIsLoadOrStore = currentInstIsLoadOrStore;
    currentInstIsLoadOrStore = false;
    updatePrevLoadReg(currLoadReg);
}

void Simulator::printCacheHitMissCounts() const
{
    const uint64_t l1HitCount = dMemory.getL1HitCount();
    const uint64_t l2HitCount = dMemory.getL2HitCount();
    const uint64_t missCount = dMemory.getMissCount();
    const uint64_t totalAccesses = l1HitCount + l2HitCount + missCount;

    std::cerr << "L1 Cache Hit Count: " << l1HitCount << std::endl;
    std::cerr << "L2 Cache Hit Count: " << l2HitCount << std::endl;
    std::cerr << "Cache Miss Count: " << missCount << std::endl;
    std::cerr << "Total Cache Hit Rate: " << (double)(l1HitCount + l2HitCount) / (double)totalAccesses << std::endl;
    std::cerr << "L1 Cache Hit Rate: " << (double)l1HitCount / (double)totalAccesses << std::endl;
    std::cerr << "L2 Cache Hit Rate: " << (double)l2HitCount / (double)(l2HitCount + missCount) << " (of L1 misses)" << std::endl;
}

void Simulator::printLog()
{
    if (enablePipeline)
    {
        printPipelineLog();
    }
    else
    {
        printNonPipelineLog();
    }
}

bool Simulator::fetchInstruction(int32_t address)
{
    return iCache.fetch(address);
}

bool Simulator::fetch2Instruction(int32_t address1, int32_t address2)
{
    bool hit1 = iCache.fetch(address1);
    bool hit2 = iCache.fetch(address2);
    bool hit = (hit1 || hit2);
    if (!hit)
    {
        --iCache.missCount;
    }
    return hit;
}

// ログの出力
void Simulator::printNonPipelineLog()
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
    std::cerr << "Sequencial load and store: " << loadStoreSequence << std::endl;
    std::cerr << "Cache miss without write back: " << dMemory.getNonWbCount() << std::endl;
    std::cerr << "Cache miss with different range write back: " << dMemory.getDiffRangeWbCount() << std::endl;
    std::cerr << "Cache miss with same range write back: " << dMemory.getSameRangeWbCount() << std::endl;
    std::cerr << "Instruction cache miss: " << iCache.getMissCount() << std::endl;

    std::cerr << "________Stall prediction________" << std::endl;
    estimatedClock += 4;
    estimatedClock += totalInstructions;
    std::cerr << "Stall from..." << std::endl;
    std::cerr << "  RAW hazard: " << hazardRAW << std::endl;
    estimatedClock += hazardRAW;
    std::cerr << "  number of JALR*2: " << instructionCounts[JALR] * 2 << std::endl;
    estimatedClock += (instructionCounts[JALR]) * 2;
    std::cerr << "  branch prediction miss*2: " << flushCount * 2 << std::endl;
    estimatedClock += flushCount * 2;
    std::cerr << "  Sequencial load and store: " << loadStoreSequence << std::endl;
    estimatedClock += loadStoreSequence;
    uint64_t floatingStall = 0;
    floatingStall += (instructionCounts[FADD]) * 2;
    floatingStall += (instructionCounts[FSUB]) * 2;
    floatingStall += (instructionCounts[FMADD]) * 3;
    floatingStall += (instructionCounts[FMUL]) * 1;
    floatingStall += (instructionCounts[FDIV]) * 4;
    floatingStall += (instructionCounts[FSQRT]) * 2;
    floatingStall += (instructionCounts[FFLOOR]) * 4;
    floatingStall += (instructionCounts[ITOF]) * 2;
    floatingStall += (instructionCounts[FTOI]) * 1;
    estimatedClock += floatingStall;
    std::cerr << "  floating instructions: " << floatingStall << std::endl;
    uint64_t cacheStall = 0;
    cacheStall += dMemory.getHitCount() * 1;
    cacheStall += dMemory.getNonWbCount() * 52.5;
    cacheStall += dMemory.getDiffRangeWbCount() * 56.2;
    cacheStall += dMemory.getSameRangeWbCount() * 64.5;
    cacheStall += iCache.getMissCount() * 5;
    estimatedClock += cacheStall;
    std::cerr << "  cache access: " << cacheStall << std::endl;
    std::cerr << "________Estimation from data________" << std::endl;
    std::cerr << "Estimated clock: " << (uint64_t)estimatedClock << std::endl;
    double estimatedTime = estimatedClock / CPUFREQUENCY;
    std::cerr << "Estimated execution time: " << estimatedTime << "sec" << std::endl;
    std::cerr << "Estimated IPC: " << totalInstructions / (double)estimatedClock << std::endl;
    std::cerr << "Estimated CPI: " << (double)estimatedClock / totalInstructions << std::endl;
    std::cerr << "Estimated instruction per sec: " << totalInstructions / estimatedTime << std::endl;
}

void Simulator::printPipelineLog()
{
    // パイプライン版の統計情報出力
    uint64_t totalSteps = getStep();
    uint64_t totalCycles = pipeline->getTotalCycles();
    uint64_t branchMisses = flushCount;
    uint64_t icacheMisses = iCache.getMissCount();

    std::cerr << "\n________Superscalar Statistics________" << std::endl;
    std::cerr << "Total instructions: " << std::hex << totalSteps << std::dec
              << " (" << totalSteps << ")" << std::endl;

    std::cerr << "Simulated cycles: " << std::hex << totalCycles << std::dec
              << " (" << totalCycles << ")" << std::endl;
    // 分岐予測ミス
    double branchMissRatio = (double)branchMisses / totalSteps * 100.0;
    std::cerr << "Branch prediction misses: " << branchMisses
              << " (" << branchMissRatio << "% of instructions)" << std::endl;

    // 命令キャッシュミス
    double icacheMissRatio = (double)icacheMisses / totalSteps * 100.0;
    std::cerr << "Instruction cache misses: " << icacheMisses
              << " (" << icacheMissRatio << "% of instructions)" << std::endl;
    uint64_t additionalStall = branchMisses * 2 + icacheMisses * 5;
    totalCycles += additionalStall;
    double cpi = (double)totalCycles / totalSteps;

    std::cerr << "Total stalls: " << pipeline->getStallCount() + additionalStall << std::endl;
    std::cerr << "Total cycles: " << std::hex << totalCycles << std::dec
              << " (" << totalCycles << ")" << std::endl;

    std::cerr << "CPI: " << cpi << std::endl;

    uint64_t superscalarAttempts = pipeline->getSuperscalarAttempts();
    uint64_t superscalarSuccess = pipeline->getSuperscalarSuccess();
    uint64_t singleIssueAttempts = pipeline->getSingleIssueAttempts();
    uint64_t singleIssueSuccess = pipeline->getSingleIssueSuccess();
    std::cerr << "Superscalar issue attempts: " << superscalarAttempts << std::endl;
    std::cerr << "Successful superscalar issues: " << superscalarSuccess
              << " (" << (superscalarAttempts > 0 ? (static_cast<float>(superscalarSuccess) / superscalarAttempts * 100.0) : 0.0)
              << "% success rate)" << std::endl;

    std::cerr << "Single issue attempts: " << singleIssueAttempts << std::endl;
    std::cerr << "Successful single issues: " << singleIssueSuccess
              << " (" << (singleIssueAttempts > 0 ? (static_cast<float>(singleIssueSuccess) / singleIssueAttempts * 100.0) : 0.0)
              << "% success rate)" << std::endl;
    float avgInstructionsPerCycle = static_cast<float>(totalSteps) / totalCycles;
    float theoreticalMaxIPC = 2.0f; // Superscalar with 2-wide issue
    float ipcEfficiency = avgInstructionsPerCycle / theoreticalMaxIPC * 100.0f;

    std::cerr << "Superscalar efficiency: " << pipeline->getSuperscalarEfficiency() * 100.0f
              << "% (percentage of superscalar attempts that succeeded)" << std::endl;
    std::cerr << "IPC efficiency: " << ipcEfficiency
              << "% (percentage of theoretical max IPC achieved)" << std::endl;

    Log::printLog();

    // 分岐予測ミス
    std::cerr << "Branch prediction misses: " << branchMisses
              << " (" << branchMissRatio << "% of instructions)" << std::endl;

    // 命令キャッシュミス
    std::cerr << "Instruction cache misses: " << icacheMisses
              << " (" << icacheMissRatio << "% of instructions)" << std::endl;

    // WB衝突（int/fp間）
    uint64_t wbCollisionIntFp = pipeline->getWbCollisionIntFpCount();
    double wbCollisionIntFpRatio = (double)wbCollisionIntFp / totalSteps * 100.0;
    std::cerr << "WB collisions (int/fp): " << wbCollisionIntFp
              << " (" << wbCollisionIntFpRatio << "% of instructions)" << std::endl;

    // WB衝突（メモリ命令）
    uint64_t wbCollisionMem = pipeline->getWbCollisionMemCount();
    double wbCollisionMemRatio = (double)wbCollisionMem / totalSteps * 100.0;
    std::cerr << "WB collisions (memory): " << wbCollisionMem
              << " (" << wbCollisionMemRatio << "% of instructions)" << std::endl;

    // 分岐追い越し防止ストール
    uint64_t branchBypassStall = pipeline->getBranchBypassStallCount();
    double branchBypassStallRatio = (double)branchBypassStall / totalSteps * 100.0;
    std::cerr << "Branch bypass stalls: " << branchBypassStall
              << " (" << branchBypassStallRatio << "% of instructions)" << std::endl;

    // メモリストールサイクル
    uint64_t memoryStallCycles = pipeline->getMemoryStallCycles();
    double memoryStallRatio = (double)memoryStallCycles / totalSteps;
    std::cerr << "Memory stall cycles: " << memoryStallCycles
              << " (" << memoryStallRatio << " per instruction)" << std::endl;

    // FPU RAWストール
    uint64_t fpuRawStalls = pipeline->getFpuRawStallCount();
    double fpuRawRatio = (double)fpuRawStalls / totalSteps * 100.0;
    std::cerr << "FPU RAW stalls: " << fpuRawStalls
              << " (" << fpuRawRatio << "% of instructions)" << std::endl;

    // Load RAWストール
    uint64_t loadRawStalls = pipeline->getLoadRawStallCount();
    double loadRawRatio = (double)loadRawStalls / totalSteps * 100.0;
    std::cerr << "Load RAW stalls: " << loadRawStalls
              << " (" << loadRawRatio << "% of instructions)" << std::endl;

    // キャッシュ統計
    if (enableCache)
    {
        printCacheHitMissCounts();
        std::cerr << "Cache miss without write back: " << dMemory.getNonWbCount() << std::endl;
        std::cerr << "Cache miss with different range write back: " << dMemory.getDiffRangeWbCount() << std::endl;
        std::cerr << "Cache miss with same range write back: " << dMemory.getSameRangeWbCount() << std::endl;
        std::cerr << "Instruction cache miss: " << iCache.getMissCount() << std::endl;

        if (enableDebug)
        {
            dMemory.printCacheState();
        }
    }

    // CPIの内訳
    std::cerr << "________CPI Breakdown________" << std::endl;
    std::cerr << "Total CPI: " << cpi * 100.0 << "%" << std::endl;
    std::cerr << "* Base: 100%" << std::endl;

    // メモリストールのCPI貢献
    double memoryStallCpi = memoryStallRatio * 100.0;
    std::cerr << "* Memory stalls: " << memoryStallCpi << "%" << std::endl;

    // FPU RAWのCPI貢献
    double fpuRawCpi = fpuRawRatio;
    std::cerr << "* FPU RAW hazards: " << fpuRawCpi << "%" << std::endl;

    // Load RAWのCPI貢献
    double loadRawCpi = loadRawRatio;
    std::cerr << "* Load RAW hazards: " << loadRawCpi << "%" << std::endl;

    // WB衝突のCPI貢献
    double wbCollisionCpi = wbCollisionIntFpRatio + wbCollisionMemRatio;
    std::cerr << "* WB collision stalls: " << wbCollisionCpi << "%" << std::endl;

    // 分岐予測ミスのCPI貢献
    double branchMissCpi = branchMissRatio * 2.0; // ミスごとに2サイクルのペナルティ
    std::cerr << "* Branch prediction misses: " << branchMissCpi << "%" << std::endl;

    std::cerr << "________Additional Statistics________" << std::endl;
    double estimatedTime = totalCycles / CPUFREQUENCY;
    std::cerr << "Estimated execution time: " << estimatedTime << "sec" << std::endl;
    std::cerr << "IPC: " << 1.0 / cpi << std::endl;
    std::cerr << "Estimated instruction per sec: " << totalSteps / estimatedTime << std::endl;

    // FPU演算
    uint64_t fpuOps = instructionCounts[FADD] +
                      instructionCounts[FMUL] + instructionCounts[FDIV] +
                      instructionCounts[FSQRT] + instructionCounts[FFLOOR] + instructionCounts[FMADD];
    ;
    double fpuOpsRatio = (double)fpuOps / totalSteps * 100.0;
    std::cerr << "FPU operations: " << fpuOps << " (" << fpuOpsRatio << "% of instructions)" << std::endl;

    // メモリ命令
    uint64_t memOps = instructionCounts[LW] + instructionCounts[LWR] + instructionCounts[SW] +
                      instructionCounts[FLW] + instructionCounts[FLWR] + instructionCounts[FSW];
    double memOpsRatio = (double)memOps / totalSteps * 100.0;
    std::cerr << "Memory operations: " << memOps << " (" << memOpsRatio << "% of instructions)" << std::endl;

    // 分岐命令
    uint64_t branchOps = instructionCounts[BEQ] + instructionCounts[BNE] + instructionCounts[BLT] +
                         instructionCounts[BGE] + instructionCounts[JAL] + instructionCounts[JALR] + instructionCounts[BFLT] + instructionCounts[BFGE];
    ;
    double branchOpsRatio = (double)branchOps / totalSteps * 100.0;
    std::cerr << "Branch operations: " << branchOps << " (" << branchOpsRatio << "% of instructions)" << std::endl;
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
    if (enablePipeline)
    {
        // パイプラインモード
        runPipelineProgram(outputRegNum);
    }
    else
    {
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
                                else if (gdbCommand == "m" || gdbCommand == "mem")
                                {
                                    if (rep == 1)
                                    {
                                        dMemory.printCacheState();
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
                                    const uint64_t instruction = loadInstruction(pc);

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
                                    if (!isBranchInst(instruction))
                                    {
                                        setPC(getPC() + 1);
                                    }
                                    ++step;
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
                            const uint64_t instruction = loadInstruction(pc);

                            executeInstruction(instruction);
                            if (!isBranchInst(instruction))
                            {
                                setPC(getPC() + 1);
                            }
                            ++step;
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
                            setBreakpoint(false);
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
                    const uint64_t instruction = loadInstruction(pc);
                    if (enableDebug)
                    {
                        printInstruction(instruction);
                    }
                    executeInstruction(instruction);
                    if (!isBranchInst(instruction))
                    {
                        setPC(getPC() + 1);
                    }
                    ++step;
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
                    const uint64_t instruction = loadInstruction(pc);
                    if (enableDebug)
                    {
                        printInstruction(instruction);
                    }
                    executeInstruction(instruction);
                    if (!isBranchInst(instruction))
                    {
                        setPC(getPC() + 1);
                    }
                    ++step;

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
}

void Simulator::runPipelineProgram(int outputRegNum)
{
    if (enableGDB)
    {
        runPipelineProgramGDB(outputRegNum);
    }
    else
    {
        runPipelineProgramNormal(outputRegNum);
    }
}
void Simulator::runPipelineProgramGDB(int outputRegNum)
{
    // GDBモードでパイプライン実行
    bool isQuit = false;
    std::ostringstream buffer;
    bool breakMode = false;
    bool isUnknownCommand = false;
    std::stringstream gdbCommandLine;
    std::string gdbCommand;
    std::string prevGdbCommand;
    int rep = 0;

    uint64_t cycleCount = 0;

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
                        // リセット
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
                        else if (gdbCommand == "m" || gdbCommand == "mem")
                        {
                            if (rep == 1)
                            {
                                dMemory.printCacheState();
                                std::cerr << std::endl;
                            }
                        }
                        else if (gdbCommand == "p" || gdbCommand == "pipeline")
                        {
                            // パイプラインの状態を表示
                            if (rep == 1)
                            {
                                std::cerr << pipeline->getPipelineStateString() << std::endl;
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
                                    printRegisters(FPREG);
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
                            if (pc >= instructionSize)
                            {
                                std::cerr << "End of program reached." << std::endl;
                                break;
                            }

                            if (rep == 1)
                            {
                                std::cerr << "Step : " << step << std::endl;
                                std::cerr << "Cycle: " << cycleCount << std::endl;
                                printProgram(true);

                                // 現在のパイプライン状態を表示
                                std::cerr << pipeline->getPipelineStateString() << std::endl;
                            }

                            pipeline->advance();
                            cycleCount++;
                            // 命令を発行しようとする

                            // スーパースカラ発行の判断
                            int32_t currentPC = pc;
                            bool pairIssued = false;

                            if (currentPC % 2 == 0 && currentPC + 1 < instructionSize)
                            {
                                const uint64_t instruction1 = loadInstruction(currentPC);
                                const uint64_t instruction2 = loadInstruction(currentPC + 1);

                                // 同時実行可能なペアかチェック
                                if (pipeline->canIssueInPair(instruction1, instruction2))
                                {
                                    // ペアとして発行を試みる
                                    pairIssued = pipeline->tryIssuePair(instruction1, currentPC, instruction2, currentPC + 1);

                                    if (pairIssued)
                                    {
                                        // 発行成功：ステップカウントとPCを2つ進める
                                        if (isBranchInst(instruction1))
                                        {
                                            step += 1;
                                            // Untaken
                                            if (getPC() != getImmediate(instruction1) && !isJalrInstruction(instruction1))
                                            {
                                                step += 1;
                                                if (!isBranchInst(instruction2))
                                                {
                                                    setPC(getPC() + 1);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            step += 2;
                                            if (!isBranchInst(instruction2))
                                            {
                                                setPC(getPC() + 2);
                                            }
                                        }
                                        if (rep == 1)
                                        {
                                            std::cerr << "Issued pair: " << instToString(instruction1)
                                                      << " and " << instToString(instruction2) << std::endl;
                                        }
                                    }
                                }
                            }

                            if (!pairIssued)
                            {
                                // 単独命令の発行
                                const uint64_t instruction = loadInstruction(currentPC);

                                // 命令を発行しようとする
                                bool issued = pipeline->tryIssue(instruction, currentPC);

                                if (issued)
                                {
                                    if (rep == 1)
                                    {
                                        std::cerr << "Issued: " << instToString(instruction) << std::endl;
                                    }
                                    // 発行成功：ステップカウントを増やす
                                    step++;

                                    // PCを更新
                                    if (!isBranchInst(instruction))
                                    {
                                        // 分岐/ジャンプ/ebreak命令以外はPCを1増やす
                                        setPC(currentPC + 1);
                                    }
                                }
                                else
                                {
                                    if (rep == 1)
                                    {
                                        std::cerr << "Instruction stalled: " << instToString(instruction) << std::endl;
                                    }
                                }
                            }
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

                    if (pc >= instructionSize)
                    {
                        std::cerr << "End of program reached." << std::endl;
                        breakMode = false;
                        break;
                    }

                    pipeline->advance();
                    cycleCount++;
                    // 命令を発行しようとする
                    // サイクル開始時のPCを保存
                    int32_t currentPC = pc;

                    // スーパースカラ発行の判断
                    bool pairIssued = false;

                    if (currentPC % 2 == 0 && currentPC + 1 < instructionSize)
                    {
                        const uint64_t instruction1 = loadInstruction(currentPC);
                        const uint64_t instruction2 = loadInstruction(currentPC + 1);

                        // 同時実行可能なペアかチェック
                        if (pipeline->canIssueInPair(instruction1, instruction2))
                        {
                            // ペアとして発行を試みる
                            pairIssued = pipeline->tryIssuePair(instruction1, currentPC, instruction2, currentPC + 1);

                            if (pairIssued)
                            {
                                if (isBranchInst(instruction1))
                                {
                                    step += 1;
                                    // Untaken
                                    if (getPC() != getImmediate(instruction1) && !isJalrInstruction(instruction1))
                                    {
                                        step += 1;
                                        if (!isBranchInst(instruction2))
                                        {
                                            setPC(getPC() + 1);
                                        }
                                    }
                                }
                                else
                                {
                                    step += 2;
                                    if (!isBranchInst(instruction2))
                                    {
                                        setPC(getPC() + 2);
                                    }
                                }
                            }
                        }
                    }

                    if (!pairIssued)
                    {
                        // 単独命令の発行
                        const uint64_t instruction = loadInstruction(currentPC);

                        // 命令を発行しようとする
                        bool issued = pipeline->tryIssue(instruction, currentPC);

                        if (issued)
                        {
                            // 発行成功：ステップカウントを増やす
                            step++;

                            if (!isBranchInst(instruction))
                            {
                                setPC(getPC() + 1);
                            }
                        }
                    }
                    // パイプラインを1サイクル進める
                }

                if (isBreakpoint)
                {
                    if (rep == 0)
                    {
                        std::cerr << buffer.str();
                        std::cerr << "Program reached ebreak at Step: " << step - 1 << std::endl;
                        std::cerr << "Cycle: " << cycleCount << std::endl;
                        std::cerr << pipeline->getPipelineStateString() << std::endl;
                        std::cerr << std::endl;
                    }
                    breakMode = false;
                    if (isBreakpoint)
                    {
                        // パイプライン内の残りの命令を処理
                        finishPipelineExecution(cycleCount);
                    }
                    setBreakpoint(false);
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

    // 結果表示
    std::cerr << "________Simulation Ended________" << std::endl;
    std::cerr << "Simulated cycles: " << cycleCount << std::endl;
    std::cerr << "Total instructions: " << step << std::endl;

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

void Simulator::runPipelineProgramNormal(int outputRegNum)
{
    std::cerr << "________Using Pipeline Mode________" << std::endl;

    uint64_t cycleCount = 0;

    if (outputSize <= 2)
    {
        // 出力サイズが小さい場合、プログレスバーなし
        while (maxStep > step && !isBreakpoint)
        {
            if (pc >= instructionSize)
            {
                break;
            }

            const uint64_t instruction = loadInstruction(pc);

            if (enableDebug)
            {
                std::cerr << "Cycle: " << cycleCount
                          << ", PC: " << pc
                          << ", Instruction: " << instToString(instruction)
                          << std::endl;
            }
            // パイプラインを1サイクル進める
            pipeline->advance();
            cycleCount++;
            // スーパースカラ発行の判断

            int32_t currentPC = pc;
            bool pairIssued = false;

            if (currentPC % 2 == 0 && currentPC + 1 < instructionSize)
            {
                const uint64_t instruction1 = loadInstruction(currentPC);
                const uint64_t instruction2 = loadInstruction(currentPC + 1);

                // 同時実行可能なペアかチェック
                if (pipeline->canIssueInPair(instruction1, instruction2))
                {

                    // ペアとして発行を試みる
                    pairIssued = pipeline->tryIssuePair(instruction1, currentPC, instruction2, currentPC + 1);

                    if (pairIssued)
                    {
                        // 発行成功：ステップカウントとPCを2つ進める
                        if (isBranchInst(instruction1))
                        {
                            step += 1;
                            // Untaken
                            if (getPC() != getImmediate(instruction1) && !isJalrInstruction(instruction1))
                            {
                                step += 1;
                                if (!isBranchInst(instruction2))
                                {
                                    setPC(getPC() + 1);
                                }
                            }
                        }
                        else
                        {
                            step += 2;
                            if (!isBranchInst(instruction2))
                            {
                                setPC(getPC() + 2);
                            }
                        }
                        if (enableDebug)
                        {
                            std::cerr << "Issued pair: " << instToString(instruction1)
                                      << " and " << instToString(instruction2) << std::endl;
                        }
                    }
                }
            }

            if (!pairIssued)
            {
                // 単独命令の発行
                const uint64_t instruction = loadInstruction(currentPC);

                if (enableDebug)
                {
                    std::cerr << "Instruction: " << instToString(instruction) << std::endl;
                }

                // 命令を発行しようとする
                bool issued = pipeline->tryIssue(instruction, currentPC);

                if (issued)
                {
                    // 発行成功：ステップカウントを増やす
                    step++;

                    if (!isBranchInst(instruction))
                    {
                        setPC(getPC() + 1);
                    }
                    if (enableDebug)
                    {
                        std::cerr << "Issued: " << instToString(instruction) << std::endl;
                    }
                }
                else if (enableDebug)
                {
                    std::cerr << "Stalled" << std::endl;
                }
            }
        }
    }
    else
    {
        // プログレスバー表示
        pbar::pbar bar(outputSize, 100);
        bar.set_description("[Simulation with Pipeline]");
        bar.init();
        bar.enable_recalc_console_width(1);
        uint64_t prevLineOutputCount = 0;

        while (maxStep > step && !isBreakpoint)
        {
            if (pc >= instructionSize)
            {
                break;
            }

            const uint64_t instruction = loadInstruction(pc);

            if (enableDebug)
            {
                std::cerr << "Cycle: " << cycleCount
                          << ", PC: " << pc
                          << ", Instruction: " << instToString(instruction)
                          << std::endl;
            }
            // パイプラインを1サイクル進める
            pipeline->advance();
            cycleCount++;

            int32_t currentPC = pc;
            bool pairIssued = false;

            if (currentPC % 2 == 0 && currentPC + 1 < instructionSize)
            {
                const uint64_t instruction1 = loadInstruction(currentPC);
                const uint64_t instruction2 = loadInstruction(currentPC + 1);

                // 同時実行可能なペアかチェック
                if (pipeline->canIssueInPair(instruction1, instruction2))
                {

                    // ペアとして発行を試みる
                    pairIssued = pipeline->tryIssuePair(instruction1, currentPC, instruction2, currentPC + 1);

                    if (pairIssued)
                    {
                        // 発行成功：ステップカウントとPCを2つ進める
                        if (isBranchInst(instruction1))
                        {
                            step += 1;
                            // Untaken
                            if (getPC() != getImmediate(instruction1) && !isJalrInstruction(instruction1))
                            {
                                step += 1;
                                if (!isBranchInst(instruction2))
                                {
                                    setPC(getPC() + 1);
                                }
                            }
                        }
                        else
                        {
                            step += 2;
                            if (!isBranchInst(instruction2))
                            {
                                setPC(getPC() + 2);
                            }
                        }
                        if (enableDebug)
                        {
                            std::cerr << "Issued pair: " << instToString(instruction1)
                                      << " and " << instToString(instruction2) << std::endl;
                        }
                    }
                }
            }

            if (!pairIssued)
            {
                // 単独命令の発行
                const uint64_t instruction = loadInstruction(currentPC);

                if (enableDebug)
                {
                    std::cerr << "Instruction: " << instToString(instruction) << std::endl;
                }

                // 命令を発行しようとする
                bool issued = pipeline->tryIssue(instruction, currentPC);

                if (issued)
                {
                    // 発行成功：ステップカウントを増やす
                    step++;

                    if (!isBranchInst(instruction))
                    {
                        setPC(getPC() + 1);
                    }
                    if (enableDebug)
                    {
                        std::cerr << "Issued: " << instToString(instruction) << std::endl;
                    }
                }
                else if (enableDebug)
                {
                    std::cerr << "Stalled" << std::endl;
                }
            }

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
    std::cerr << "Simulated cycles: " << cycleCount << std::endl;
    std::cerr << "Total instructions: " << step << std::endl;

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

void Simulator::finishPipelineExecution(uint64_t &cycleCount)
{
    // パイプライン内の命令が全て完了するまでadvanceを呼び続ける
    bool pipelineEmpty = false;

    while (!pipelineEmpty)
    {
        pipeline->advance();
        cycleCount++;

        pipelineEmpty = pipeline->isEmpty();

        if (enableDebug)
        {
            std::cerr << "Finishing pipeline execution, cycle: " << cycleCount << std::endl;
            std::cerr << pipeline->getPipelineStateString() << std::endl;
        }
    }
}