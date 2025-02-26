#include "Pipeline.hpp"
#include "Simulator.hpp"
#include "Util.hpp"
#include <iostream>
#include <algorithm>
#include <utility>

Pipeline::Pipeline(Simulator &sim) : simulator(sim)
{
    // すべてのスロットを空に初期化
    for (int i = 0; i < 3; i++)
    {
        decoded_int[i] = std::nullopt;
    }
    for (int i = 0; i < 6; i++)
    {
        decoded_fp[i] = std::nullopt;
    }
    decoded_mem = std::nullopt;
    executed_int = executed_fp = executed_mem = std::nullopt;
    MAed_int = MAed_fp = std::nullopt;
}

bool Pipeline::tryIssue(uint32_t instruction, int32_t pc)
{
    // 発行可能性をチェック
    PipelineInstruction inst;
    decodeInstruction(instruction, pc, inst);

    // ebreak命令
    if (inst.isEbreak)
    {
        // パイプライン内の命令が完了するまで待つ
        simulator.setBreakpoint(true);
        simulator.fetchInstruction(pc);

        int instType = getInstructionType(instruction);
        simulator.logInstruction(instType);
        simulator.logInstAddr(pc);
        return true;
    }

    // 分岐命令のハザードをチェック
    if ((inst.isBranch || inst.isJalr) &&
        (decoded_int[0].has_value() ||
         decoded_int[1].has_value() ||
         decoded_int[2].has_value() ||
         decoded_fp[0].has_value() ||
         decoded_fp[1].has_value() ||
         decoded_fp[2].has_value() ||
         decoded_fp[3].has_value() ||
         decoded_fp[4].has_value() ||
         decoded_fp[5].has_value() || decoded_mem.has_value()))
    {
        stallCount++;
        branchBypassStallCount++;
        return false;
    }

    // 依存関係と競合をチェック
    bool depStall = checkDependencies(inst);
    bool contentionStall = checkContention(inst);

    if (depStall || contentionStall)
    {
        stallCount++;

        // ストールの種類をカウント
        if (contentionStall)
        {
            // WB衝突の種類を判別
            if (inst.isMemory)
            {
                wbCollisionMemCount++;
            }
            else
            {
                wbCollisionIntFpCount++;
            }
        }

        return false;
    }

    // WAWハザードの処理
    handleWAWHazards(inst);

    // 分岐命令（jal, jalr, b*）は特別扱い - 即座に実行してPCを更新
    if (inst.isBranch || inst.isJalr)
    {
        simulator.fetchInstruction(pc);

        // Simulatorのメソッドを使用し分岐命令を実行
        simulator.executeInstruction(instruction);

        flushPipeline();

        int instType = getInstructionType(instruction);
        simulator.logInstruction(instType);
        simulator.logInstAddr(pc);
        return true;
    }

    // 非分岐命令は通常通り処理
    if (inst.isMemory)
    {
        decoded_mem = inst;
    }
    else if (inst.isFpRd)
    {
        // FP命令のレイテンシに基づいてスロットを決定
        int latency = getFpLatency(instruction);
        int slot = latency - 1;
        if (slot >= 0 && slot < 6)
        {
            decoded_fp[slot] = inst;
        }
    }
    else if (inst.rd != -1 && !inst.isFpRd)
    {
        // INT命令のレイテンシに基づいてスロットを決定
        int latency = getIntLatency(instruction);
        int slot = latency - 1;
        if (slot >= 0 && slot < 3)
        {
            decoded_int[slot] = inst;
        }
    }
    simulator.fetchInstruction(pc);
    int instType = getInstructionType(instruction);
    simulator.logInstruction(instType);
    simulator.logInstAddr(pc);
    return true;
}

void Pipeline::advance()
{
    totalCycles++;
    // 次のサイクルでの各スロットの状態
    InstSlot next_MAed_int = std::nullopt;
    InstSlot next_MAed_fp = std::nullopt;
    InstSlot next_executed_int = std::nullopt;
    InstSlot next_executed_fp = std::nullopt;
    InstSlot next_executed_mem = std::nullopt;
    InstSlot next_decoded_mem = std::nullopt;
    std::array<InstSlot, 3> next_decoded_int = {std::nullopt, std::nullopt, std::nullopt};
    std::array<InstSlot, 6> next_decoded_fp = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};

    if (executed_int)
    {
        next_MAed_int = executed_int;
        executeAtExecutedStage(*executed_int);
    }

    if (executed_fp)
    {
        next_MAed_fp = executed_fp;
        executeAtExecutedStage(*executed_fp);
    }

    bool executed_mem_stalled = false;
    if (executed_mem)
    {
        if (executed_mem->cacheWaitCycles > 0)
        {
            // キャッシュミスで待機中
            executed_mem->cacheWaitCycles--;
            next_executed_mem = executed_mem;
            executed_mem_stalled = true;
        }
        else if (executed_mem->isStore || executed_mem->shouldDisappear)
        {
            // ストア命令または消滅する命令
            executeAtExecutedStage(*executed_mem);
        }
        else
        {
            // ロード命令がMAed_intまたはMAed_fpに進む
            if (executed_mem->isFpRd)
            {
                if (next_MAed_fp)
                {
                    // MAed_fpに競合
                    next_executed_mem = executed_mem;
                    executed_mem_stalled = true;
                }
                else
                {
                    next_MAed_fp = executed_mem;
                    executeAtExecutedStage(*executed_mem);
                }
            }
            else
            {
                if (next_MAed_int)
                {
                    // MAed_intに競合
                    next_executed_mem = executed_mem;
                    executed_mem_stalled = true;
                }
                else
                {
                    next_MAed_int = executed_mem;
                    executeAtExecutedStage(*executed_mem);
                }
            }
        }
    }

    if (decoded_mem)
    {
        if (executed_mem_stalled)
        {
            next_decoded_mem = decoded_mem;
        }
        else
        {
            if (decoded_mem->isMemory)
            {
                // メモリアクセス
                int32_t address;

                address = decoded_mem->rs1Value;
                if (decoded_mem->isLoad)
                {
                    if (decoded_mem->rs2 != -1)
                    {
                        // lwr/flwr
                        address += decoded_mem->rs2Value;
                    }
                    else
                    {
                        // lw/flw
                        int32_t offset = ((((decoded_mem->raw >> 26) & 0x3F) << 8) | ((decoded_mem->raw >> 6) & 0xFF));
                        if ((offset >> 13) & 1)
                        {
                            offset -= 1 << 14;
                        }
                        address += offset;
                    }

                    // キャッシュアクセスをシミュレート
                    bool isHit = simulator.simulateCacheAccess(address * 4, false);
                    if (!isHit)
                    {
                        // キャッシュミスなら待機サイクル数を設定
                        decoded_mem->cacheWaitCycles = simulator.getCacheMissPenalty();
                        countMemoryStall(decoded_mem->cacheWaitCycles);
                    }
                }
                else if (decoded_mem->isStore)
                {
                    // sw/fsw
                    int32_t offset = getImmediate(decoded_mem->raw);
                    if ((offset >> 13) & 1)
                    {
                        offset -= 1 << 14;
                    }
                    address += offset;

                    // キャッシュアクセスをシミュレート
                    bool isHit = simulator.simulateCacheAccess(address * 4, true);
                    if (!isHit)
                    {
                        // キャッシュミスなら待機サイクル数を設定
                        decoded_mem->cacheWaitCycles = simulator.getCacheMissPenalty();
                        countMemoryStall(decoded_mem->cacheWaitCycles);
                    }
                }
            }

            next_executed_mem = decoded_mem;
        }
    }

    next_executed_int = decoded_int[0];
    for (int i = 0; i < 2; i++)
    {
        next_decoded_int[i] = decoded_int[i + 1];
    }

    next_executed_fp = decoded_fp[0];
    for (int i = 0; i < 5; i++)
    {
        next_decoded_fp[i] = decoded_fp[i + 1];
    }

    // すべてのスロットを更新
    MAed_int = next_MAed_int;
    MAed_fp = next_MAed_fp;
    executed_int = next_executed_int;
    executed_fp = next_executed_fp;
    executed_mem = next_executed_mem;
    decoded_mem = next_decoded_mem;
    for (int i = 0; i < 3; i++)
    {
        decoded_int[i] = next_decoded_int[i];
    }
    for (int i = 0; i < 6; i++)
    {
        decoded_fp[i] = next_decoded_fp[i];
    }
}
void Pipeline::decodeInstruction(uint32_t raw, int32_t pc, PipelineInstruction &inst)
{
    // 命令をデコードし、種類や依存関係を決定
    inst.raw = raw;
    inst.pc = pc;

    uint32_t opcode = getOpcode(raw);
    uint32_t subop = getSubop(raw);
    uint32_t rd = getRd(raw);
    uint32_t rs1 = getRs1(raw);
    uint32_t rs2 = getRs2(raw);

    inst.rd = -1;
    inst.isFpRd = false;
    inst.rs1 = -1;
    inst.rs2 = -1;
    inst.isFpRs1 = inst.isFpRs2 = false;
    inst.isMemory = inst.isLoad = inst.isStore = false;
    inst.isBranch = inst.isJalr = false;
    inst.isEbreak = false;
    inst.cacheWaitCycles = 0;
    inst.shouldDisappear = false;
    inst.rs1Value = 0;
    inst.rs2Value = 0;

    // 命令タイプの判別及びソースレジスタの設定
    switch (opcode)
    {
    case 0x1: // add, sub
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getRegister(rs2);
        break;
    case 0x2: // addi, lui, slli, srli
        inst.rd = rd;
        if (subop != 0x1)
        { // luiでなければrs1を使用
            inst.rs1 = rs1;
            inst.rs1Value = simulator.getRegister(rs1);
        }
        break;
    case 0x3:
    case 0xE: // beq, bne, blt, bge
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isBranch = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getRegister(rs2);
        break;
    case 0x4:
    case 0xF:          // jal
        inst.rd = rs2; // jalは特例でrs2の位置にrd
        inst.isBranch = true;
        break;
    case 0x5: // jalr
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.isJalr = true;
        inst.rs1Value = simulator.getRegister(rs1);
        break;
    case 0x8: // lw, lwr
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.isMemory = true;
        inst.isLoad = true;
        inst.rs1Value = simulator.getRegister(rs1);
        if (subop == 0x1)
        { // lwr
            inst.rs2 = rs2;
            inst.rs2Value = simulator.getRegister(rs2);
        }
        break;
    case 0x9: // sw
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isMemory = true;
        inst.isStore = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getRegister(rs2);
        break;
    case 0xA: // flw, flwr
        inst.rd = rd;
        inst.isFpRd = true;
        inst.rs1 = rs1;
        inst.isMemory = true;
        inst.isLoad = true;
        inst.rs1Value = simulator.getRegister(rs1);
        if (subop == 0x1)
        { // flwr
            inst.rs2 = rs2;
            inst.rs2Value = simulator.getRegister(rs2);
        }
        break;
    case 0xB: // fsw
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isFpRs2 = true;
        inst.isMemory = true;
        inst.isStore = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getFpRegister(rs2);
        break;
    case 0xC: // ftoi, flt, feq
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.isFpRs1 = true;
        inst.rs1Value = simulator.getFpRegister(rs1);
        if (getFpuop(raw) != 0x4)
        { // flt, feq
            inst.rs2 = rs2;
            inst.isFpRs2 = true;
            inst.rs2Value = simulator.getFpRegister(rs2);
        }
        break;
    case 0xD: // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor
        inst.rd = rd;
        inst.isFpRd = true;
        inst.rs1 = rs1;
        if (getFpuop(raw) == 0x9)
        { // itof
            // rs1はint
            inst.rs1Value = simulator.getRegister(rs1);
        }
        else
        {
            inst.isFpRs1 = true;
            inst.rs1Value = simulator.getFpRegister(rs1);
            if (getFpuop(raw) <= 0x3)
            { // fadd, fsub, fmul, fdiv
                inst.rs2 = rs2;
                inst.isFpRs2 = true;
                inst.rs2Value = simulator.getFpRegister(rs2);
            }
        }
        break;
    case 0x6: // ebreak
        inst.isEbreak = true;
        break;
    }
}

bool Pipeline::checkDependencies(const PipelineInstruction &inst) const
{
    // パイプライン内の命令への依存関係をチェック
    auto checkDep = [&inst](const InstSlot &slot) -> std::pair<bool, const PipelineInstruction *>
    {
        if (!slot)
            return {false, nullptr};

        const auto &slotInst = *slot;
        if (slotInst.rd == -1)
            return {false, nullptr};

        // rs1/rs2が前命令のrdに依存するかチェック
        bool hasDep = (inst.rs1 == slotInst.rd && inst.isFpRs1 == slotInst.isFpRd) ||
                      (inst.rs2 == slotInst.rd && inst.isFpRs2 == slotInst.isFpRd);

        return {hasDep, hasDep ? &slotInst : nullptr};
    };

    for (const auto &slot : decoded_int)
    {
        auto [hasDep, source] = checkDep(slot);
        if (hasDep && source)
        {
            const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
            return true;
        }
    }

    for (const auto &slot : decoded_fp)
    {
        auto [hasDep, source] = checkDep(slot);
        if (hasDep && source)
        {
            const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
            return true;
        }
    }

    auto [hasDep1, source1] = checkDep(decoded_mem);
    if (hasDep1 && source1)
    {
        const_cast<Pipeline *>(this)->countRawHazard(inst, *source1);
        return true;
    }

    auto [hasDep2, source2] = checkDep(executed_int);
    if (hasDep2 && source2)
    {
        const_cast<Pipeline *>(this)->countRawHazard(inst, *source2);
        return true;
    }

    auto [hasDep3, source3] = checkDep(executed_fp);
    if (hasDep3 && source3)
    {
        const_cast<Pipeline *>(this)->countRawHazard(inst, *source3);
        return true;
    }

    if (executed_mem && !(executed_mem->isLoad && executed_mem->shouldDisappear))
    {
        auto [hasDep4, source4] = checkDep(executed_mem);
        if (hasDep4 && source4)
        {
            const_cast<Pipeline *>(this)->countRawHazard(inst, *source4);
            return true;
        }
    }

    return false;
}

bool Pipeline::checkContention(const PipelineInstruction &inst) const
{
    // 発行先スロットへの競合をチェック
    if (inst.isMemory)
    {
        return decoded_mem.has_value();
    }
    else if (inst.isFpRd)
    {
        int latency = getFpLatency(inst.raw);
        int slot = latency - 1;
        return (slot >= 0 && slot < 6) ? decoded_fp[slot].has_value() : true;
    }
    else if (inst.rd != -1)
    {
        int latency = getIntLatency(inst.raw);
        int slot = latency - 1;
        return (slot >= 0 && slot < 3) ? decoded_int[slot].has_value() : true;
    }

    return false;
}

bool Pipeline::checkBranchHazard(const PipelineInstruction &inst) const
{
    // 分岐命令が他の命令を追い越すか
    if (inst.isBranch || inst.isJalr)
    {
        return decoded_int[2].has_value() ||
               decoded_fp[2].has_value() ||
               decoded_fp[3].has_value() ||
               decoded_fp[4].has_value() ||
               decoded_fp[5].has_value();
    }
    return false;
}

void Pipeline::handleWAWHazards(const PipelineInstruction &inst)
{
    // WAWハザードの解消
    if (inst.rd != -1)
    {
        if (inst.isFpRd)
        {
            // FPレジスタに書き込む命令
            for (int i = 2; i < 6; i++)
            {
                if (decoded_fp[i] && decoded_fp[i]->rd == inst.rd && decoded_fp[i]->isFpRd)
                {
                    decoded_fp[i] = std::nullopt; // 消滅
                }
            }
        }
        else
        {
            // INTレジスタに書き込む命令
            for (int i = 2; i < 3; i++)
            {
                if (decoded_int[i] && decoded_int[i]->rd == inst.rd && !decoded_int[i]->isFpRd)
                {
                    decoded_int[i] = std::nullopt; // 消滅
                }
            }
        }

        // メモリ以外の命令がメモリ命令と競合する場合
        if (!inst.isMemory)
        {
            if (decoded_mem && decoded_mem->isLoad &&
                decoded_mem->rd == inst.rd && decoded_mem->isFpRd == inst.isFpRd)
            {
                decoded_mem->shouldDisappear = true;
            }

            if (executed_mem && executed_mem->isLoad &&
                executed_mem->rd == inst.rd && executed_mem->isFpRd == inst.isFpRd)
            {
                executed_mem->shouldDisappear = true;
            }
        }
    }
}
void Pipeline::executeAtExecutedStage(PipelineInstruction &inst)
{
    bool logEnabled = simulator.isLogEnabled();

    if (inst.isMemory)
    {
        // メモリアクセス
        int32_t address;

        address = inst.rs1Value;
        if (inst.isLoad)
        {
            if (inst.rs2 != -1)
            {
                // lwr/flwr
                address += inst.rs2Value;
            }
            else
            {
                // lw/flw
                int32_t offset = ((((inst.raw >> 26) & 0x3F) << 8) | ((inst.raw >> 6) & 0xFF));
                if ((offset >> 13) & 1)
                {
                    offset -= 1 << 14;
                }
                address += offset;
            }

            // キャッシュアクセスがまだの場合
            if (inst.cacheWaitCycles == 0)
            {
                int32_t value;
                if (inst.isFpRd)
                {
                    if (logEnabled)
                    {
                        std::cerr << "Executing load: " << simulator.instToString(inst.raw) << std::endl;
                    }
                    value = simulator.loadWord(address * 4, false); // flw/flwr
                }
                else
                {
                    if (logEnabled)
                    {
                        std::cerr << "Executing load: " << simulator.instToString(inst.raw) << std::endl;
                    }
                    value = simulator.loadWord(address * 4, true); // lw/lwr
                }

                // 結果を設定（shouldDisappearでない場合のみ）
                if (!inst.shouldDisappear)
                {
                    if (inst.isFpRd)
                    {
                        if (logEnabled)
                        {
                            std::cerr << "Setting fp" << inst.rd << " from " << simulator.instToString(inst.raw) << std::endl;
                        }
                        simulator.setFpRegister(inst.rd, value);
                    }
                    else
                    {
                        if (logEnabled)
                        {
                            std::cerr << "Setting x" << inst.rd << " from " << simulator.instToString(inst.raw) << std::endl;
                        }
                        simulator.setRegister(inst.rd, value);
                    }
                }
            }
        }
        else if (inst.isStore)
        {
            // sw/fsw
            int32_t offset = getImmediate(inst.raw);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            address += offset;

            if (inst.cacheWaitCycles == 0)
            {
                // ストア値を取得
                int32_t value = inst.rs2Value;

                if (logEnabled)
                {
                    std::cerr << "Executing store: " << simulator.instToString(inst.raw) << std::endl;
                }
                simulator.storeWord(address * 4, value);
            }
        }
    }
    else
    {
        // 非メモリ命令の実行
        if (logEnabled)
        {
            std::cerr << "Executing: " << simulator.instToString(inst.raw) << std::endl;
        }

        uint32_t opcode = getOpcode(inst.raw);
        uint32_t subop = getSubop(inst.raw);

        switch (opcode)
        {
        case 0x1: // add, sub
            if (subop == 0x0)
            {
                // add
                simulator.setRegister(inst.rd, inst.rs1Value + inst.rs2Value);
            }
            else if (subop == 0x1)
            {
                // sub
                simulator.setRegister(inst.rd, inst.rs1Value - inst.rs2Value);
            }
            break;
        case 0x2: // addi, lui, slli, srli
            if (subop == 0x0)
            {
                // addi
                int32_t imm = ((((inst.raw >> 26) & 0x3F) << 8) | ((inst.raw >> 6) & 0xFF));
                if ((imm >> 13) & 1)
                {
                    imm -= 1 << 14;
                }
                simulator.setRegister(inst.rd, inst.rs1Value + imm);
            }
            else if (subop == 0x2)
            {
                // slli
                const int32_t shamt = (inst.raw >> 6) & 0x3;
                simulator.setRegister(inst.rd, inst.rs1Value << shamt);
            }
            else if (subop == 0x3)
            {
                // srli
                const int32_t shamt = (inst.raw >> 6) & 0x3;
                simulator.setRegister(inst.rd, inst.rs1Value >> shamt);
            }
            else if (subop == 0x1)
            {
                // lui
                const int32_t imm = ((((inst.raw >> 20) & 0xFFF) << 8) | ((inst.raw >> 6) & 0xFF)) << 12;
                simulator.setRegister(inst.rd, imm);
            }
            break;
        case 0xC: // ftoi, flt, feq
        {
            uint32_t fpuop = getFpuop(inst.raw);
            if (fpuop == 0x4)
            {
                // ftoi
                simulator.setRegister(inst.rd, simulator.fpu.ftoi(inst.rs1Value));
            }
            else if (fpuop == 0x0)
            {
                // flt
                simulator.setRegister(inst.rd, simulator.fpu.flt(inst.rs1Value, inst.rs2Value));
            }
            else if (fpuop == 0x1)
            {
                // feq
                simulator.setRegister(inst.rd, simulator.fpu.feq(inst.rs1Value, inst.rs2Value));
            }
        }
        break;

        case 0xD: // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor
        {
            uint32_t fpuop = getFpuop(inst.raw);
            if (fpuop == 0x9)
            {
                // itof
                simulator.setFpRegister(inst.rd, simulator.fpu.itof(inst.rs1Value));
            }
            else if (fpuop == 0x0)
            {
                // fadd
                simulator.setFpRegister(inst.rd, simulator.fpu.fadd(inst.rs1Value, inst.rs2Value));
            }
            else if (fpuop == 0x1)
            {
                // fsub
                simulator.setFpRegister(inst.rd, simulator.fpu.fsub(inst.rs1Value, inst.rs2Value));
            }
            else if (fpuop == 0x2)
            {
                // fmul
                simulator.setFpRegister(inst.rd, simulator.fpu.fmul(inst.rs1Value, inst.rs2Value));
            }
            else if (fpuop == 0x3)
            {
                // fdiv
                simulator.setFpRegister(inst.rd, simulator.fpu.fdiv(inst.rs1Value, inst.rs2Value));
            }
            else if (fpuop == 0x4)
            {
                // fmv
                simulator.setFpRegister(inst.rd, inst.rs1Value);
            }
            else if (fpuop == 0x5)
            {
                // fneg
                simulator.setFpRegister(inst.rd, simulator.fpu.fneg(inst.rs1Value));
            }
            else if (fpuop == 0x6)
            {
                // fabs
                simulator.setFpRegister(inst.rd, simulator.fpu.fabs(inst.rs1Value));
            }
            else if (fpuop == 0x7)
            {
                // fsqrt
                simulator.setFpRegister(inst.rd, simulator.fpu.fsqrt(inst.rs1Value));
            }
            else if (fpuop == 0x8)
            {
                // ffloor
                simulator.setFpRegister(inst.rd, simulator.fpu.ffloor(inst.rs1Value));
            }
        }
        break;

        // 分岐命令やその他の命令はexecuteInstructionInPipelineを使用
        default:
            simulator.executeInstructionInPipeline(inst.raw, inst.pc, inst.rs1Value, inst.rs2Value);
            break;
        }
    }
}

int Pipeline::getIntLatency(uint32_t instruction) const
{
    // INT命令のレイテンシを判定
    uint32_t opcode = getOpcode(instruction);

    switch (opcode)
    {
    case 0x1:     // add, sub
    case 0x2:     // addi, lui, slli, srli
        return 1; // レイテンシ1
    case 0x5:     // jalr
        return 1; // レイテンシ1
    case 0xC:     // ftoi, flt, feq
        return 2; // レイテンシ2
    default:
        return 1; // デフォルトはレイテンシ1
    }
}

int Pipeline::getFpLatency(uint32_t instruction) const
{
    // FP命令のレイテンシを判定
    uint32_t opcode = getOpcode(instruction);

    if (opcode != 0xD)
        return 1; // 非FP命令はレイテンシ1

    uint32_t fpuop = getFpuop(instruction);
    switch (fpuop)
    {
    case 0x0:     // fadd
    case 0x1:     // fsub
        return 3; // レイテンシ3
    case 0x2:     // fmul
        return 4; // レイテンシ4
    case 0x3:     // fdiv
        return 6; // レイテンシ6
    case 0x4:     // fmv
    case 0x5:     // fneg
    case 0x6:     // fabs
        return 1; // レイテンシ1
    case 0x7:     // fsqrt
        return 6; // レイテンシ6
    case 0x8:     // ffloor
        return 3; // レイテンシ3
    case 0x9:     // itof
        return 2; // レイテンシ2
    default:
        return 1; // デフォルトはレイテンシ1
    }
}

std::string Pipeline::getPipelineStateString() const
{
    std::stringstream ss;
    ss << "Pipeline State:\n";

    // Decoded int ステージ
    ss << "* Decoded int: ";
    for (int i = 2; i >= 0; i--)
    {
        if (decoded_int[i])
        {
            ss << "int" << i << ":[" << simulator.instToString(decoded_int[i]->raw) << "] ";
        }
        else
        {
            ss << "int" << i << ":[empty] ";
        }
    }
    ss << "\n";

    // Decoded fp ステージ
    ss << "* Decoded fp: ";
    for (int i = 5; i >= 0; i--)
    {
        if (decoded_fp[i])
        {
            ss << "fp" << i << ":[" << simulator.instToString(decoded_fp[i]->raw) << "] ";
        }
        else
        {
            ss << "fp" << i << ":[empty] ";
        }
    }
    ss << "\n";

    // Decoded mem ステージ
    ss << "* Decoded mem: ";
    if (decoded_mem)
    {
        ss << "[" << simulator.instToString(decoded_mem->raw) << "]";
    }
    else
    {
        ss << "[empty]";
    }
    ss << "\n";

    // Executed ステージ
    ss << "* Executed int: ";
    if (executed_int)
    {
        ss << "[" << simulator.instToString(executed_int->raw) << "] ";
    }
    else
    {
        ss << "[empty] ";
    }

    ss << "Executed fp: ";
    if (executed_fp)
    {
        ss << "[" << simulator.instToString(executed_fp->raw) << "] ";
    }
    else
    {
        ss << "[empty] ";
    }

    ss << "Executed mem: ";
    if (executed_mem)
    {
        ss << "[" << simulator.instToString(executed_mem->raw) << "]";
        if (executed_mem->cacheWaitCycles > 0)
        {
            ss << " (waiting " << executed_mem->cacheWaitCycles << " cycles)";
        }
    }
    else
    {
        ss << "[empty]";
    }
    ss << "\n";

    // MAed ステージ
    ss << "* MAed int: ";
    if (MAed_int)
    {
        ss << "[" << simulator.instToString(MAed_int->raw) << "] ";
    }
    else
    {
        ss << "[empty] ";
    }

    ss << "MAed fp: ";
    if (MAed_fp)
    {
        ss << "[" << simulator.instToString(MAed_fp->raw) << "]";
    }
    else
    {
        ss << "[empty]";
    }

    return ss.str();
}

std::vector<std::pair<std::string, PipelineInstruction *>> Pipeline::getCurrentInstructions() const
{
    std::vector<std::pair<std::string, PipelineInstruction *>> result;

    // 各ステージの命令を収集（実際に命令が存在するステージのみ）
    for (int i = 0; i < 3; i++)
    {
        if (decoded_int[i])
        {
            result.push_back({"decoded_int" + std::to_string(i), const_cast<PipelineInstruction *>(&decoded_int[i].value())});
        }
    }

    for (int i = 0; i < 6; i++)
    {
        if (decoded_fp[i])
        {
            result.push_back({"decoded_fp" + std::to_string(i), const_cast<PipelineInstruction *>(&decoded_fp[i].value())});
        }
    }

    if (decoded_mem)
    {
        result.push_back({"decoded_mem", const_cast<PipelineInstruction *>(&decoded_mem.value())});
    }

    if (executed_int)
    {
        result.push_back({"executed_int", const_cast<PipelineInstruction *>(&executed_int.value())});
    }

    if (executed_fp)
    {
        result.push_back({"executed_fp", const_cast<PipelineInstruction *>(&executed_fp.value())});
    }

    if (executed_mem)
    {
        result.push_back({"executed_mem", const_cast<PipelineInstruction *>(&executed_mem.value())});
    }

    if (MAed_int)
    {
        result.push_back({"MAed_int", const_cast<PipelineInstruction *>(&MAed_int.value())});
    }

    if (MAed_fp)
    {
        result.push_back({"MAed_fp", const_cast<PipelineInstruction *>(&MAed_fp.value())});
    }

    return result;
}

std::string Pipeline::formatInstruction(const PipelineInstruction &inst) const
{
    return simulator.instToString(inst.raw);
}

// RAWハザードをカウントするメソッド
void Pipeline::countRawHazard(const PipelineInstruction &inst, const PipelineInstruction &source)
{
    // 命令のタイプに基づいてRAWをカウント
    bool isFpuInst = false;
    bool isLoadInst = false;

    uint32_t opcode = getOpcode(source.raw);

    // FPU命令かどうかチェック
    if (opcode == 0xD)
    {
        isFpuInst = true;
    }

    // ロード命令かどうかチェック
    if (opcode == 0x8 || opcode == 0xA)
    {
        isLoadInst = true;
    }

    if (isFpuInst)
    {
        fpuRawStallCount++;
    }
    else if (isLoadInst)
    {
        loadRawStallCount++;
    }
}

// メモリストールをカウントするメソッド
void Pipeline::countMemoryStall(uint64_t cycles)
{
    memoryStallCycles += cycles;
}

void Pipeline::flushPipeline()
{
    for (int i = 0; i < 3; i++)
    {
        decoded_int[i] = std::nullopt;
    }
    for (int i = 0; i < 6; i++)
    {
        decoded_fp[i] = std::nullopt;
    }
    decoded_mem = std::nullopt;

    // executed_int, executed_fp, executed_memはそのまま実行完了させるため残す
    // MAed_int, MAed_fpはそのまま実行完了させるため残す
}

bool Pipeline::isEmpty() const
{
    // 全てのスロットが空かチェック
    for (int i = 0; i < 3; i++)
    {
        if (decoded_int[i].has_value())
            return false;
    }
    for (int i = 0; i < 6; i++)
    {
        if (decoded_fp[i].has_value())
            return false;
    }
    if (decoded_mem.has_value())
        return false;
    if (executed_int.has_value())
        return false;
    if (executed_fp.has_value())
        return false;
    if (executed_mem.has_value())
        return false;
    if (MAed_int.has_value())
        return false;
    if (MAed_fp.has_value())
        return false;

    return true;
}

int Pipeline::getInstructionType(uint32_t instruction)
{
    uint32_t opcode = getOpcode(instruction);
    uint32_t subop = getSubop(instruction);
    uint32_t fpuop = getFpuop(instruction);

    switch (opcode)
    {
    case 0x1: // add, sub
        if (subop == 0x0)
            return Simulator::ADD;
        else if (subop == 0x1)
            return Simulator::SUB;
        break;
    case 0x2: // addi, lui, slli, srli
        if (subop == 0x0)
            return Simulator::ADDI;
        else if (subop == 0x1)
            return Simulator::LUI;
        else if (subop == 0x2)
            return Simulator::SLLI;
        else if (subop == 0x3)
            return Simulator::SRLI;
        break;
    case 0x3:
    case 0xE: // beq, bne, blt, bge
        if (subop == 0x0)
            return Simulator::BEQ;
        else if (subop == 0x1)
            return Simulator::BNE;
        else if (subop == 0x2)
            return Simulator::BLT;
        else if (subop == 0x3)
            return Simulator::BGE;
        break;
    case 0x4:
    case 0xF: // jal
        return Simulator::JAL;
    case 0x5: // jalr
        return Simulator::JALR;
    case 0x8: // lw, lwr
        if (subop == 0x0)
            return Simulator::LW;
        else if (subop == 0x1)
            return Simulator::LWR;
        break;
    case 0x9: // sw
        return Simulator::SW;
    case 0xA: // flw, flwr
        if (subop == 0x0)
            return Simulator::FLW;
        else if (subop == 0x1)
            return Simulator::FLWR;
        break;
    case 0xB: // fsw
        return Simulator::FSW;
    case 0x6: // ebreak
        return Simulator::EBREAK;
    case 0xC: // ftoi, flt, feq
        if (fpuop == 0x4)
            return Simulator::FTOI;
        else if (fpuop == 0x0)
            return Simulator::FLT;
        else if (fpuop == 0x1)
            return Simulator::FEQ;
        break;
    case 0xD: // itof, fadd, fsub, fmul, fdiv, fmv, fneg, fabs, fsqrt, ffloor
        if (fpuop == 0x9)
            return Simulator::ITOF;
        else if (fpuop == 0x0)
            return Simulator::FADD;
        else if (fpuop == 0x1)
            return Simulator::FSUB;
        else if (fpuop == 0x2)
            return Simulator::FMUL;
        else if (fpuop == 0x3)
            return Simulator::FDIV;
        else if (fpuop == 0x4)
            return Simulator::FMV;
        else if (fpuop == 0x5)
            return Simulator::FNEG;
        else if (fpuop == 0x6)
            return Simulator::FABS;
        else if (fpuop == 0x7)
            return Simulator::FSQRT;
        else if (fpuop == 0x8)
            return Simulator::FFLOOR;
        break;
    }

    return -1;
}