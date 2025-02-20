#include "Simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <bit>
#include <pbar.hpp>

Simulator::Simulator(OptionHandler &op)
    : iCache(iMemory), dMemory(op.memorySize, CACHE_SIZE, BLOCK_SIZE, INPUT_ADDRESS * 4, OUTPUT_ADDRESS * 4, (op.cacheNumWay == 0 ? 1 : op.cacheNumWay))
{
    DMEMORY_SIZE = op.memorySize;
    registers[0] = 0;                       // x0
    registers[2] = (DMEMORY_SIZE >> 2) - 4; // sp
    pc = 0;
    isBreakpoint = false;
    enableCache = op.enableCache;
    enableICount = op.enableICount;
    enableIStats = op.enableIStats;
    enableDebug = op.enableDebug;
    enableStdout = op.enableStdout;
    enableICache = op.enableICache;
    outputSize = op.imageSize * op.imageSize + 2;
    enableGDB = op.enableGDB;
    maxStep = op.maxStep;
    dMemory.availableCache = op.enableCache;
    availableLog = op.enableGDB || op.enableDebug;
    dMemory.availableLog = availableLog;
    outputFilePath = op.outputFilePath;
}

void Simulator::storeInstruction(int32_t address, int32_t instruction)
{
    if (address < 0 || address >= IMEMORY_SIZE >> 2) [[unlikely]]
    {
        throw std::out_of_range("iMemory access out of bounds");
    }
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
    case 0xE:
    {
        // beq, bne, blt, bge
        const uint32_t subop = getSubop(instruction);
        const uint32_t rs1 = getRs1(instruction);
        const uint32_t rs2 = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xE)
        {
            imm |= 1 << 14;
        }

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
    case 0xF:
    {
        // jal
        const uint32_t rd = getRs2(instruction);
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xF)
        {
            imm |= 1 << 14;
        }
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
    if (!file) [[unlikely]]
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
        const uint32_t instruction = iMemory[i];
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
                const uint32_t instruction = iMemory[address];
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
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<int32_t>(getRegister(i))) + ")" << std::endl;
        }
    }

    if (regType == FPREG || regType == ALLREG)
    {
        std::cerr << "________FpRegisters state________" << std::endl;
        for (int i = 0; i < FPREG_COUNT; ++i)
        {
            std::cerr << std::setw(8) << ("fp" + std::to_string(i) + ":")
                      << std::setw(15) << std::hex << getFpRegister(i)
                      << std::setw(15) << std::dec << "(" + std::to_string(std::bit_cast<float>(getFpRegister(i))) + ")" << std::endl;
        }
    }
}

// 分岐予測
int Simulator::branchPrediction(int32_t rs1, int32_t rs2, int32_t imm, bool isTaken)
{
    int stall = 0;
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
            std::cerr << "Branch misprediction detected at PC: " << getPC()
                      << ". Stalling for 2 cycles." << std::endl;
        logFlush();
        stall = 2;
    }
    else
    {
        if (availableLog) [[unlikely]]
            std::cerr << "Prediction matched!" << std::endl;
        stall = 0;
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
    return stall;
}

void Simulator::printInstruction(uint32_t instruction) const
{
    if (availableLog) [[unlikely]]
        std::cerr << "Executing: " << instToString(instruction) << std::endl;
}

size_t Simulator::getFpuLatency(uint32_t instruction) const
{
    const uint32_t opcode = getOpcode(instruction);
    const uint32_t fpuop = getFpuop(instruction);

    if (opcode == 0xC)
    {
        switch (fpuop)
        {
        case 0x4:
            return FTOI_LATENCY; // ftoi
        case 0x0:                // flt
        case 0x1:                // feq
            return 1;            // flt, feqは即時処理
        default:
            throw std::runtime_error("Unknown FPU operation in opcode 0xC: " + std::to_string(fpuop));
        }
    }
    else if (opcode == 0xD)
    {
        switch (fpuop)
        {
        case 0x0: // fadd
        case 0x1: // fsub
            return FADD_LATENCY;

        case 0x2: // fmul
            return FMUL_LATENCY;

        case 0x3: // fdiv
            return FDIV_LATENCY;

        case 0x4:     // fmv
        case 0x5:     // fneg
        case 0x6:     // fabs
            return 1; // fmv, fneg, fabsは即時処理

        case 0x7: // fsqrt
            return FSQRT_LATENCY;

        case 0x8: // ffloor
            return FFLOOR_LATENCY;

        case 0x9: // itof
            return ITOF_LATENCY;

        default:
            throw std::runtime_error("Unknown FPU operation in opcode 0xD: " + std::to_string(fpuop));
        }
    }

    throw std::runtime_error("Instruction is not an FPU operation: opcode=" + std::to_string(opcode));
}

void Simulator::executeInstruction(uint32_t instruction)
{
    logInstAddr(getPC());
    // ストール管理
    size_t stall = 0;
    // 命令キャッシュ
    if (enableICache)
    {
        bool hit = iCache.fetch(getPC());
        if (!hit)
        {
            stall += 5; // キャッシュミスで5クロックストール
            totalStalls += 5;
            if (availableLog)
            {
                std::cerr << "Instruction Cache miss detected. Stalling for 5 cycles at PC: " << getPC() << std::endl;
            }
        }
        else
        {
            if (availableLog)
            {
                std::cerr << "Instruction Cache hit!" << std::endl;
            }
        }
    }
    const uint32_t opcode = getOpcode(instruction);
    const uint32_t subop = getSubop(instruction);
    const uint32_t fpuop = getFpuop(instruction);

    int rd = -1, rs1 = -1, rs2 = -1; // 初期値-1

    switch (opcode)
    {
    case 0x1: // R-type (add, sub)
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        rs2 = getRs2(instruction);
        break;

    case 0x2: // I-type (addi, slli, srli, lui)
        rd = getRd(instruction);
        if (subop == 0x0 || subop == 0x2 || subop == 0x3)
        { // addi, slli, srli
            rs1 = getRs1(instruction);
        }
        break;

    case 0x3: // B-type (beq, bne, blt, bge)
        rs1 = getRs1(instruction);
        rs2 = getRs2(instruction);
        break;

    case 0x5: // jalr
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        break;

    case 0x8: // lw, lwr
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        if (subop == 0x1)
        {
            rs2 = getRs2(instruction);
        }
        break;

    case 0xA: // flw, flwr
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        if (subop == 0x1)
        {
            rs2 = getRs2(instruction);
        }
        break;

    case 0x9: // sw
    case 0xB: // fsw
        rs1 = getRs1(instruction);
        rs2 = getRs2(instruction);
        break;

    case 0xC: // ftoi, flt, feq
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        if (fpuop == 0x0 || fpuop == 0x1)
        { // flt, feq は rs2 あり
            rs2 = getRs2(instruction);
        }
        break;

    case 0xD: // FPU: itof, fadd, fsub, fmul, fdiv, fsqrt, ffloor, fmv, fneg, fabs
        rd = getRd(instruction);
        rs1 = getRs1(instruction);
        if (fpuop == 0x0 || fpuop == 0x1 || fpuop == 0x2 || fpuop == 0x3)
        { // fadd, fsub, fmul, fdiv
            rs2 = getRs2(instruction);
        }
        break;

    case 0x4:                     // jal
    case 0xF:                     // jal
        rd = getRs2(instruction); // jalは特別にrdレジスタの位置が他命令のrs2の位置にある
        break;

    case 0x6: // ebreak
        break;

    default:
        throw std::runtime_error("Unknown instruction");
    }

    // データ依存によるストール
    if (rs1 >= 0)
    {
        int64_t dependencyStall = static_cast<int64_t>(regReadyCycle[rs1]) - static_cast<int64_t>(clockCycle);
        stall = std::max(stall, static_cast<size_t>(std::max<int64_t>(0, dependencyStall)));
    }
    if (rs2 >= 0)
    {
        int64_t dependencyStall = static_cast<int64_t>(regReadyCycle[rs2]) - static_cast<int64_t>(clockCycle);
        stall = std::max(stall, static_cast<size_t>(std::max<int64_t>(0, dependencyStall)));
    }

    // 条件分岐によるストール
    if (opcode == 0x3 || opcode == 0xE || opcode == 0x5)
    { // beq, bne, blt, bge, jalr
        size_t lastEX = clockCycle;
        for (size_t i = 0; i < 10; ++i)
        {
            lastEX = std::max(lastEX, fpuWritebackSchedule[i]);
        }
        stall = std::max(stall, lastEX - clockCycle);
    }

    // FPUのWB競合によるストール
    if (opcode == 0xD || opcode == 0xC)
    { // FPU命令
        size_t latency = getFpuLatency(instruction);
        size_t wbTime = clockCycle + stall + latency;

        for (size_t &wbSlot : fpuWritebackSchedule)
        {
            if (wbSlot <= clockCycle)
            {
                wbSlot = 0;
            }
        }

        // WB競合チェック
        for (size_t &wbSlot : fpuWritebackSchedule)
        {
            if (wbSlot == wbTime)
            {
                stall += 1;
                wbTime += 1;
            }
        }

        // WBスケジュール更新
        for (size_t &wbSlot : fpuWritebackSchedule)
        {
            if (wbSlot == 0)
            { // 空いているWBスロットに登録
                wbSlot = wbTime;
                break;
            }
        }
    }

    // **ストール適用**
    if (stall > 0)
    {
        clockCycle += stall;
        totalStalls += stall;
        if (availableLog)
        {
            std::cerr << "Stalled for " << stall << " cycles at PC: " << getPC() << std::endl;
        }
    }

    switch (opcode)
    {
    case 0x1:
    {
        // R-type (add, sub)
        if (subop == 0x0)
        {
            logInstruction(ADD);
            if (rs1 == 0 || rs2 == 0)
            {
                ++mvCount;
            }
            setRegister(rd, getRegister(rs1) + getRegister(rs2));
        }
        else if (subop == 0x1)
        {
            logInstruction(SUB);
            setRegister(rd, getRegister(rs1) - getRegister(rs2));
        }
        else [[unlikely]]
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

        if (subop == 0x0)
        {
            int32_t imm = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((imm >> 13) & 1)
            {
                imm -= 1 << 14;
            }
            logInstruction(ADDI);
            // count mvi
            if (rs1 == 0)
            {
                ++mviCount;
            }
            setRegister(rd, getRegister(rs1) + imm);
        }
        else if (subop == 0x2)
        {
            const int32_t shamt = (instruction >> 6) & 0x3;
            if (!(shamt >= 0 && shamt <= 3)) [[unlikely]]
            {
                throw std::runtime_error("Warning: shamt is not between 0 and 3");
            }
            logInstruction(SLLI);
            setRegister(rd, getRegister(rs1) << shamt);
        }
        else if (subop == 0x3)
        {
            const int32_t shamt = (instruction >> 6) & 0x3;
            if (!(shamt >= 0 && shamt <= 3)) [[unlikely]]
            {
                throw std::runtime_error("Warning: shamt is not between 0 and 3");
            }
            logInstruction(SRLI);
            setRegister(rd, getRegister(rs1) >> shamt);
        }
        else if (subop == 0x1)
        {
            // ?-type (lui)
            const int32_t imm = ((((instruction >> 20) & 0xFFF) << 8) | ((instruction >> 6) & 0xFF)) << 12;
            logInstruction(LUI);
            setRegister(rd, imm);
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0x3:
    case 0xE:
    {
        // B-type (beq, bne, blt, bge)
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xE)
        {
            imm |= 1 << 14;
        }
        bool isTaken = false;
        if (subop == 0x0)
        {
            logInstruction(BEQ);
            isTaken = (getRegister(rs1) == getRegister(rs2));
        }
        else if (subop == 0x1)
        {
            logInstruction(BNE);
            isTaken = (getRegister(rs1) != getRegister(rs2));
        }
        else if (subop == 0x2)
        {
            logInstruction(BLT);
            isTaken = (getRegister(rs1) < getRegister(rs2));
        }
        else if (subop == 0x3)
        {
            logInstruction(BGE);
            isTaken = (getRegister(rs1) >= getRegister(rs2));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        int flush = branchPrediction(rs1, rs2, imm, isTaken); // 分岐予測の実行
        stall += flush;
        totalStalls += flush;
        break;
    }
    case 0x4:
    case 0xF:
    {
        // J-type (jal)
        int32_t imm = getImmediate(instruction);
        if (opcode == 0xF)
        {
            imm |= 1 << 14;
        }
        logInstruction(JAL);
        setRegister(rd, getPC() + 1);
        setPC(imm);
        break;
    }
    case 0x5:
    {
        // I-type (jalr)
        logInstruction(JALR);
        setRegister(rd, getPC() + 1);
        setPC(getRegister(rs1));
        stall += 2;
        totalStalls += 2;
        if (availableLog)
        {
            std::cerr << "Branch misprediction detected for JALR at PC: " << getPC()
                      << ". Stalling for 2 cycles." << std::endl;
        }
        break;
    }
    case 0x8:
    {
        // I-type (lw), R-type (lwr)
        if (subop == 0x0)
        {
            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
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
        }
        else if (subop == 0x1)
        {
            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction(LWR);
            setRegister(rd, dMemory.loadWord(address * 4, true));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0x9:
    {
        // S-type (sw)
        int32_t offset = getImmediate(instruction);
        // 符号ビットを処理
        if ((offset >> 13) & 1)
        {
            offset -= 1 << 14;
        }

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
        setPC(getPC() + 1);
        break;
    }
    case 0xA:
    {
        // I-type, (flw), R-type (flwr)
        if (subop == 0x0)
        {
            int32_t offset = ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }

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
        }
        else if (subop == 0x1)
        {
            const int64_t address = getRegister(rs1) + getRegister(rs2);
            logInstruction(FLWR); // 命令の記録
            setFpRegister(rd, dMemory.loadWord(address * 4, false));
        }
        else [[unlikely]]
        {
            std::stringstream ss;
            ss << "Unknown instruction 0x" << std::hex << instruction;
            throw std::runtime_error(ss.str());
        }
        setPC(getPC() + 1);
        break;
    }
    case 0xB:
    {
        // S-type (fsw)
        int32_t imm = getImmediate(instruction);
        // 符号ビットを処理
        if ((imm >> 13) & 1)
        {
            imm -= 1 << 14;
        }

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
        dMemory.storeWord(address * 4, getFpRegister(rs2));
        setPC(getPC() + 1);
        break;
    }
    case 0xC:
    {
        // ftoi, flt, feq
        if (fpuop == 0x4)
        {
            logInstruction(FTOI);
            setRegister(rd, fpu.ftoi(getFpRegister(rs1)));
        }
        else if (fpuop == 0x0)
        {
            logInstruction(FLT);
            setRegister(rd, fpu.flt(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x1)
        {
            logInstruction(FEQ);
            setRegister(rd, fpu.feq(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else [[unlikely]]
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
        if (fpuop == 0x9)
        {
            logInstruction(ITOF);
            setFpRegister(rd, fpu.itof(getRegister(rs1)));
        }
        else if (fpuop == 0x0)
        {
            logInstruction(FADD);
            setFpRegister(rd, fpu.fadd(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x1)
        {
            logInstruction(FSUB);
            setFpRegister(rd, fpu.fsub(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x2)
        {
            logInstruction(FMUL);
            setFpRegister(rd, fpu.fmul(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x3)
        {
            logInstruction(FDIV);
            setFpRegister(rd, fpu.fdiv(getFpRegister(rs1), getFpRegister(rs2)));
        }
        else if (fpuop == 0x4)
        {
            logInstruction(FMV);
            setFpRegister(rd, getFpRegister(rs1));
        }
        else if (fpuop == 0x5)
        {
            logInstruction(FNEG);
            setFpRegister(rd, fpu.fneg(getFpRegister(rs1)));
        }
        else if (fpuop == 0x6)
        {
            logInstruction(FABS);
            setFpRegister(rd, fpu.fabs(getFpRegister(rs1)));
        }
        else if (fpuop == 0x7)
        {
            logInstruction(FSQRT);
            setFpRegister(rd, fpu.fsqrt(getFpRegister(rs1)));
        }
        else if (fpuop == 0x8)
        {
            logInstruction(FFLOOR);
            setFpRegister(rd, fpu.ffloor(getFpRegister(rs1)));
        }
        else [[unlikely]]
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
        logInstruction(EBREAK);
        isBreakpoint = true;
        setPC(getPC() + 1);
        break;
    }
    [[unlikely]] default:
        std::stringstream ss;
        ss << "Unknown instruction 0x" << std::hex << instruction;
        throw std::runtime_error(ss.str());
    }
    // レジスタ使用可能クロックの更新
    if (rd >= 0)
    {
        if (opcode == 0x8 || opcode == 0xA || opcode == 0x9 || opcode == 0xB)
        {
            regReadyCycle[rd] = clockCycle + dMemory.stallCycles;
            dMemory.stallCycles = 0;
        }
        else if (opcode == 0xD || opcode == 0xC)
        { // FPU命令
            regReadyCycle[rd] = clockCycle + getFpuLatency(instruction);
        }
        else
        {
            regReadyCycle[rd] = clockCycle + 1;
        }
    }
    ++clockCycle;
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
    std::cerr << "Cache miss without write back: " << dMemory.getNonWbCount() << std::endl;
    std::cerr << "Cache miss with different range write back: " << dMemory.getDiffRangeWbCount() << std::endl;
    std::cerr << "Cache miss with same range write back: " << dMemory.getSameRangeWbCount() << std::endl;
    std::cerr << "Instruction cache miss: " << iCache.getMissCount() << std::endl;
    uint64_t estimatedClock = clockCycle;
    std::cerr << "________Stall prediction________" << std::endl;
    std::cerr << "Total Stalls: " << totalStalls << std::endl;
    /*
    estimatedClock += 4;
    estimatedClock += totalInstructions;
    std::cerr << "Stall from..." << std::endl;
    std::cerr << "  RAW hazard: " << hazardRAW << std::endl;
    estimatedClock += hazardRAW;
    std::cerr << "  number of JALR*2: " << instructionCounts[JALR] * 2 << std::endl;
    estimatedClock += (instructionCounts[JALR]) * 2;
    std::cerr << "  branch prediction miss*2: " << flushCount * 2 << std::endl;
    estimatedClock += flushCount * 2;
    uint64_t floatingStall = 0;
    floatingStall += ((instructionCounts[FADD]) + (instructionCounts[FSUB])) * 4;
    floatingStall += (instructionCounts[FMUL]) * 1;
    floatingStall += (instructionCounts[FDIV]) * 4;
    floatingStall += (instructionCounts[FSQRT]) * 2;
    floatingStall += (instructionCounts[FFLOOR]) * 4;
    floatingStall += (instructionCounts[ITOF]) * 2;
    floatingStall += (instructionCounts[FTOI]) * 2;
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
    */
    std::cerr << "________Estimation from data________" << std::endl;
    std::cerr << "Estimated clock: " << (uint64_t)estimatedClock << std::endl;
    double estimatedTime = estimatedClock / CPUFREQUENCY;
    std::cerr << "Estimated execution time: " << estimatedTime << "sec" << std::endl;
    std::cerr << "Estimated IPC: " << totalInstructions / (double)estimatedClock << std::endl;
    std::cerr << "Estimated CPI: " << (double)estimatedClock / totalInstructions << std::endl;
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
                                    std::cerr << "Clock : " << clockCycle << std::endl;
#ifdef DEBUG
                                    std::cerr << "instruction : 0b" << std::bitset<32>(instruction) << std::endl;
#endif // DEBUG

                                    printProgram(true);
                                    printInstruction(instruction);
                                }
                                executeInstruction(instruction);
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
                        const uint32_t instruction = loadInstruction(pc);

                        executeInstruction(instruction);
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
                const uint32_t instruction = loadInstruction(pc);
                if (enableDebug)
                {
                    printInstruction(instruction);
                }
                executeInstruction(instruction);
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
