#include "Pipeline.hpp"
#include "Simulator.hpp"
#include "Util.hpp"
#include <iostream>
#include <algorithm>
#include <utility>

Pipeline::Pipeline(Simulator &sim) : simulator(sim)
{
    for (int i = 0; i < 3; i++)
    {
        decoded_int[i].clear();
    }
    for (int i = 0; i < 6; i++)
    {
        decoded_fp[i] = std::nullopt;
    }
    decoded_mem = std::nullopt;
    executed_int.clear();
    executed_fp = std::nullopt;
    executed_mem = std::nullopt;
    MAed_int.clear();
    MAed_fp = std::nullopt;
}

bool Pipeline::tryIssue(uint64_t instruction, int32_t pc)
{
    // 発行可能性をチェック
    PipelineInstruction inst;
    decodeInstruction(instruction, pc, inst);

    singleIssueAttempts++;

    // ebreak命令
    if (inst.isEbreak)
    {
        // パイプライン内の命令が完了するまで待つ
        simulator.setBreakpoint(true);
        simulator.fetchInstruction(pc);

        logIssue(instruction, pc);
        singleIssueSuccess++;
        return true;
    }

    // 分岐命令のハザードと依存関係と競合をチェック
    bool branchStall = checkBranchHazard(inst);
    bool depStall = checkDependencies(inst);
    bool contentionStall = checkContention(inst);

    if (depStall || branchStall || contentionStall)
    {
        stallCount++;
        // ストールの種類をカウント
        if (branchStall)
        {
            branchBypassStallCount++;
        }
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

    simulator.fetchInstruction(pc);

    // 分岐命令（jalr, b*）は即座に実行してPCを更新
    if (inst.isBranch || inst.isJalr)
    {
        // Simulatorのメソッドを使用し分岐命令を実行
        simulator.executeInstruction(instruction);
        singleIssueSuccess++;
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
        decoded_int[slot].emplace_back(inst);
    }
    else if (inst.isOut)
    {
        simulator.storeWord(simulator.OUTPUT_ADDRESS * 4, inst.rs1Value);
    }

    logIssue(instruction, pc);
    singleIssueSuccess++;
    return true;
}

bool Pipeline::tryIssuePair(uint64_t instruction1, int32_t pc1, uint64_t instruction2, int32_t pc2)
{
    // 発行可能性をチェック
    PipelineInstruction inst1, inst2;
    decodeInstruction(instruction1, pc1, inst1);
    decodeInstruction(instruction2, pc2, inst2);
    superscalarAttempts++;
    // ebreak命令のチェック
    if (inst1.isEbreak)
    {
        // ebreakがある場合は単独で発行
        simulator.setBreakpoint(true);
        simulator.fetch2Instruction(pc1, pc2);

        logIssue(instruction1, pc1);
        superscalarSuccess++;
        return true;
    }

    // 両方の命令の発行可能性をチェック

    bool branchStall1 = checkBranchHazard(inst1);
    bool branchStall2 = checkBranchHazard(inst2);
    bool depStall1 = checkDependencies(inst1);
    bool depStall2 = checkDependencies(inst2);
    bool contentionStall1 = checkContention(inst1);
    bool contentionStall2 = checkContention(inst2);
    // 両方の命令の発行可能性をチェック
    bool stall1 = branchStall1 || depStall1 || contentionStall1;
    bool stall2 = branchStall2 || depStall2 || contentionStall2;

    if (stall1 || stall2)
    {
        stallCount++;
        // ストールの種類をカウント
        if (branchStall1)
        {
            branchBypassStallCount++;
        }
        if (contentionStall1)
        {
            // WB衝突の種類を判別
            if (inst1.isMemory)
            {
                wbCollisionMemCount++;
            }
            else
            {
                wbCollisionIntFpCount++;
            }
        }

        if (branchStall2)
        {
            branchBypassStallCount++;
        }
        if (contentionStall2)
        {
            // WB衝突の種類を判別
            if (inst1.isMemory)
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
    handleWAWHazards(inst1);
    if (!inst1.isBranch && !inst1.isJalr) // 分岐命令でなければ2つ目もWAWチェック
    {
        handleWAWHazards(inst2);
    }

    simulator.fetch2Instruction(pc1, pc2);

    // 1つ目の命令の処理
    if (inst1.isBranch || inst1.isJalr)
    {
        // 分岐命令は即座に実行
        simulator.executeInstruction(instruction1);

        // 分岐が取られたかチェック
        bool isBranchTaken = (simulator.getPC() != pc1 + 1);

        if (!isBranchTaken)
        {

            // 分岐が取られなければ2番目の命令も発行

            if (isJalInstruction(inst2.raw))
            {
                simulator.executeInstruction(instruction2);
            }
            else
            {

                logIssue(instruction2, pc2);
                if (inst2.isEbreak)
                {
                    simulator.setBreakpoint(true);
                }
                else if (inst2.isMemory)
                {
                    decoded_mem = inst2;
                }
                else if (inst2.isFpRd)
                {
                    int latency = getFpLatency(instruction2);
                    int slot = latency - 1;
                    if (slot >= 0 && slot < 6)
                    {
                        decoded_fp[slot] = inst2;
                    }
                }
                else if (inst2.rd != -1 && !inst2.isFpRd)
                {
                    int latency = getIntLatency(instruction2);
                    int slot = latency - 1;
                    decoded_int[slot].push_back(inst2);
                }
            }
        }
        superscalarSuccess++;
        return true;
    }

    // 命令キャッシュアクセスと統計の記録

    logIssue(instruction1, pc1);

    // 1つ目が通常命令の場合
    if (inst1.isMemory)
    {
        decoded_mem = inst1;
    }
    else if (inst1.isFpRd)
    {
        int latency = getFpLatency(instruction1);
        int slot = latency - 1;
        if (slot >= 0 && slot < 6)
        {
            decoded_fp[slot] = inst1;
        }
    }
    else if (inst1.rd != -1 && !inst1.isFpRd)
    {
        int latency = getIntLatency(instruction1);
        int slot = latency - 1;
        decoded_int[slot].push_back(inst1);
    }
    else if (inst1.isOut)
    {
        simulator.storeWord(simulator.OUTPUT_ADDRESS * 4, inst1.rs1Value);
    }

    // 2つ目の命令も処理
    if (inst2.isEbreak)
    {
        logIssue(instruction2, pc2);
        simulator.setBreakpoint(true);
    }
    else if (inst2.isBranch || inst2.isJalr)
    {
        // 2つ目が分岐命令の場合は即座に実行
        simulator.pc++;
        simulator.executeInstruction(instruction2);
    }
    else
    {
        logIssue(instruction2, pc2);
        // 2つ目が通常命令の場合
        if (inst2.isMemory)
        {
            decoded_mem = inst2;
        }
        else if (inst2.isFpRd)
        {
            int latency = getFpLatency(instruction2);
            int slot = latency - 1;
            if (slot >= 0 && slot < 6)
            {
                decoded_fp[slot] = inst2;
            }
        }
        else if (inst2.rd != -1 && !inst2.isFpRd)
        {
            int latency = getIntLatency(instruction2);
            int slot = latency - 1;
            decoded_int[slot].push_back(inst2);
        }
    }
    superscalarSuccess++;
    return true;
}

void Pipeline::advance()
{
    totalCycles++;

    // 次のサイクルでの各スロットの状態
    std::vector<PipelineInstruction> next_MAed_int;
    InstSlot next_MAed_fp = std::nullopt;
    std::vector<PipelineInstruction> next_executed_int;
    InstSlot next_executed_fp = std::nullopt;
    InstSlot next_executed_mem = std::nullopt;
    InstSlot next_decoded_mem = std::nullopt;
    std::array<std::vector<PipelineInstruction>, 3> next_decoded_int;
    std::array<InstSlot, 6> next_decoded_fp = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};

    // executed_intからMAed_intへ
    for (auto &inst : executed_int)
    {
        // すべての命令をMAed_intに移動
        next_MAed_int.push_back(inst);
    }

    // executed_fpからMAed_fpへ
    if (executed_fp)
    {
        next_MAed_fp = executed_fp;
    }

    // executed_memの処理
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
                // int型のロード命令はMAed_intに追加可能（複数対応）
                executeAtExecutedStage(*executed_mem);
                next_MAed_int.push_back(*executed_mem);
            }
        }
    }

    // decoded_memの処理
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
                        int32_t offset = getOffset6_8(decoded_mem->raw);
                        if ((offset >> 13) & 1)
                        {
                            offset -= 1 << 14;
                        }
                        address += offset;
                    }

                    // キャッシュアクセスをシミュレート
                    simulator.simulateCacheAccess(address * 4, false);
                    decoded_mem->cacheWaitCycles = simulator.getCacheMissPenalty() - 1;
                    countMemoryStall(decoded_mem->cacheWaitCycles);
                }
                else if (decoded_mem->isStore)
                {
                    // sw/fsw
                    int32_t offset = getOffset14(decoded_mem->raw);
                    if ((offset >> 13) & 1)
                    {
                        offset -= 1 << 14;
                    }
                    address += offset;

                    // キャッシュアクセスをシミュレート
                    simulator.simulateCacheAccess(address * 4, true);
                    decoded_mem->cacheWaitCycles = simulator.getCacheMissPenalty() - 1;
                    countMemoryStall(decoded_mem->cacheWaitCycles);
                }
            }
            next_executed_mem = decoded_mem;
        }
    }

    // decoded_intの処理 - すべて実行ステージに進む
    for (auto &inst : decoded_int[0])
    {
        executeAtExecutedStage(inst);
        next_executed_int.push_back(inst);
    }

    // decoded_int[1]とdecoded_int[2]をシフト
    next_decoded_int[0] = decoded_int[1];
    next_decoded_int[1] = decoded_int[2];
    next_decoded_int[2].clear();

    // decoded_fpの処理
    if (decoded_fp[0])
    {
        executeAtExecutedStage(*decoded_fp[0]);
        next_executed_fp = decoded_fp[0];
    }

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
void Pipeline::decodeInstruction(uint64_t raw, int32_t pc, PipelineInstruction &inst)
{
    // 命令をデコードして、種類や依存関係を決定
    inst.raw = raw;
    inst.pc = pc;

    uint32_t opcode = getOpcode(raw);
    uint32_t subop = getSubop(raw);
    uint32_t subsubop = getSubsubop(raw);
    uint32_t fpuop = getFpuop(raw);
    uint32_t rd = getRd(raw);
    uint32_t rs1 = getRs1(raw);
    uint32_t rs2 = getRs2(raw);
    uint32_t m1 = getM1(raw);
    uint32_t m2 = getM2(raw);

    // 初期化
    inst.rd = -1;
    inst.isFpRd = false;
    inst.rs1 = -1;
    inst.rs2 = -1;
    inst.rs3 = -1;
    inst.isFpRs1 = inst.isFpRs2 = inst.isFpRs3 = false;
    inst.isEbreak = false;
    inst.isOut = false;
    inst.isMemory = inst.isLoad = inst.isStore = false;
    inst.isBranch = inst.isJalr = false;
    inst.cacheWaitCycles = 0;
    inst.shouldDisappear = false;
    inst.rs1Value = 0;
    inst.rs2Value = 0;
    inst.rs3Value = 0;

    // 命令タイプの判別
    switch (opcode)
    {
    case 0x0: // nop
        // nopは何も設定しない
        break;

    case 0x1: // ALU命令と入出力命令
        if (subop == 0x0)
        { // add, sub, slli, srli
            inst.rd = rd;
            inst.rs1 = rs1;

            if (subsubop == 0x0 || subsubop == 0x1)
            { // add, sub
                inst.rs2 = rs2;
                inst.rs1Value = simulator.getRegister(rs1);
                inst.rs2Value = simulator.getRegister(rs2);
            }
            else if (subsubop == 0x2 || subsubop == 0x3)
            { // slli, srli
                inst.rs1Value = simulator.getRegister(rs1);
            }
        }
        else if (subop == 0x1)
        { // addi
            inst.rd = rd;
            inst.rs1 = rs1;
            inst.rs1Value = simulator.getRegister(rs1);
        }
        else if (subop == 0x2)
        { // lui
            inst.rd = rd;
        }
        else if (subop == 0x3)
        { // in, fin, out
            if (subsubop == 0x0)
            { // in
                inst.rd = rd;
            }
            else if (subsubop == 0x1)
            { // fin
                inst.rd = rd;
                inst.isFpRd = true;
            }
            else if (subsubop == 0x2)
            { // out
                inst.rs1 = rs1;
                inst.isOut = true;
                inst.rs1Value = simulator.getRegister(rs1);
            }
        }
        break;

    case 0x2: // beq, bne
    case 0x6: // blt, bge
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isBranch = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getRegister(rs2);
        break;

    case 0xA: // bflt, bfge
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isFpRs1 = true;
        inst.isFpRs2 = true;
        inst.isBranch = true;
        inst.rs1Value = simulator.applyFpModifier(simulator.getFpRegister(rs1), m1);
        inst.rs2Value = simulator.applyFpModifier(simulator.getFpRegister(rs2), m2);
        break;

    case 0xE:          // jal
        inst.rd = rs2; // jalは特例でrs2の位置にrd
        inst.isBranch = true;
        break;

    case 0xF: // jalr
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.isJalr = true;
        inst.rs1Value = simulator.getRegister(rs1);
        break;

    case 0x3: // lw, lwr
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

    case 0x4: // sw
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isMemory = true;
        inst.isStore = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.getRegister(rs2);
        break;

    case 0x5: // flw, flwr
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

    case 0x7: // fsw
        inst.rs1 = rs1;
        inst.rs2 = rs2;
        inst.isFpRs2 = true;
        inst.isMemory = true;
        inst.isStore = true;
        inst.rs1Value = simulator.getRegister(rs1);
        inst.rs2Value = simulator.applyFpModifier(simulator.getFpRegister(rs2), m2);
        break;

    case 0x9: // ebreak
        inst.isEbreak = true;
        break;

    case 0xC: // ftoi, flt, feq
        inst.rd = rd;
        inst.rs1 = rs1;
        inst.isFpRs1 = true;
        inst.rs1Value = simulator.applyFpModifier(simulator.getFpRegister(rs1), m1);

        if (fpuop != 0x4)
        { // flt, feq
            inst.rs2 = rs2;
            inst.isFpRs2 = true;
            inst.rs2Value = simulator.applyFpModifier(simulator.getFpRegister(rs2), m2);
        }
        break;

    case 0xD: // itof, fadd, fmul, fdiv, fmv, fsqrt, ffloor, fmadd
        inst.rd = rd;
        inst.isFpRd = true;
        inst.rs1 = rs1;

        if (fpuop == 0x7)
        { // itof
            inst.rs1Value = simulator.getRegister(rs1);
        }
        else
        {
            inst.isFpRs1 = true;
            inst.rs1Value = simulator.applyFpModifier(simulator.getFpRegister(rs1), m1);

            if (fpuop <= 0x3 && fpuop != 0x1)
            { // fadd, fmul, fdiv
                inst.rs2 = rs2;
                inst.isFpRs2 = true;
                inst.rs2Value = simulator.applyFpModifier(simulator.getFpRegister(rs2), m2);
            }
            else if (fpuop == 0x1)
            { // fmadd
                inst.rs2 = rs2;
                inst.isFpRs2 = true;
                inst.rs2Value = simulator.applyFpModifier(simulator.getFpRegister(rs2), m2);
                uint32_t rs3 = getRs3(raw);
                uint32_t m3 = getM3(raw);
                // rs3の依存関係を追加（実際には別のフィールドを用意する必要があるかも）
                inst.rs3 = rs3;
                inst.isFpRs3 = true;
                inst.rs3Value = simulator.applyFpModifier(simulator.getFpRegister(rs3), m3);
            }
        }
        break;
    }
}

bool Pipeline::checkDependencies(const PipelineInstruction &inst) const
{
    // パイプライン内の命令への依存関係をチェック
    auto checkDep = [&inst](const PipelineInstruction &slotInst) -> std::pair<bool, const PipelineInstruction *>
    {
        if (slotInst.rd == -1)
            return {false, nullptr};

        // rs1/rs2が前命令のrdに依存するかチェック
        bool hasDep = (inst.rs1 == slotInst.rd && inst.isFpRs1 == slotInst.isFpRd) ||
                      (inst.rs2 == slotInst.rd && inst.isFpRs2 == slotInst.isFpRd) ||
                      (inst.rs3 == slotInst.rd && inst.isFpRs3 == slotInst.isFpRd);

        return {hasDep, hasDep ? &slotInst : nullptr};
    };

    for (int i = 0; i < 3; i++)
    {
        for (const auto &slotInst : decoded_int[i])
        {
            auto [hasDep, source] = checkDep(slotInst);
            if (hasDep && source)
            {
                const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
                return true;
            }
        }
    }

    for (int i = 0; i < 6; i++)
    {
        if (decoded_fp[i])
        {
            auto [hasDep, source] = checkDep(*decoded_fp[i]);
            if (hasDep && source)
            {
                const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
                return true;
            }
        }
    }

    if (decoded_mem)
    {
        auto [hasDep, source] = checkDep(*decoded_mem);
        if (hasDep && source)
        {
            const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
            return true;
        }
    }

    if (executed_mem && !(executed_mem->isLoad && executed_mem->shouldDisappear))
    {
        auto [hasDep, source] = checkDep(*executed_mem);
        if (hasDep && source)
        {
            const_cast<Pipeline *>(this)->countRawHazard(inst, *source);
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

    // INT命令は複数発行可能
    return false;
}

bool Pipeline::checkBranchHazard(const PipelineInstruction &inst) const
{
    // 分岐命令が他の命令を追い越すか
    if ((inst.isBranch || inst.isJalr) && !isJalInstruction(inst.raw))
    {
        return !decoded_int[1].empty() ||
               !decoded_int[2].empty() ||
               decoded_fp[1].has_value() ||
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
                // 各スロットの命令をチェック
                auto &slot = decoded_int[i];
                for (auto it = slot.begin(); it != slot.end();)
                {
                    if (it->rd == inst.rd && !it->isFpRd)
                    {
                        // WAWハザードがある命令を削除
                        it = slot.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
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
    // executed ステージにおける命令の実行
    bool logEnabled = simulator.isLogEnabled(); // Simulatorからログ有効フラグを取得

    if (inst.isMemory)
    {
        // メモリアクセスを実行
        int32_t address;

        // アドレス計算 - デコード時に取得した値を使用
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
                int32_t offset = getOffset6_8(inst.raw);
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
            int32_t offset = getOffset14(inst.raw);
            if ((offset >> 13) & 1)
            {
                offset -= 1 << 14;
            }
            address += offset;

            // キャッシュアクセスがまだの場合
            if (inst.cacheWaitCycles == 0)
            {
                // ストア値を取得
                int32_t value;
                value = inst.rs2Value;

                // Memory::storeWordを直接呼び出し
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
        uint32_t subsubop = getSubsubop(inst.raw);
        uint32_t fpuop = getFpuop(inst.raw);

        switch (opcode)
        {
        case 0x0: // nop
            // 何もしない
            break;

        case 0x1: // ALU命令と入出力命令
            if (subop == 0x0)
            {
                if (subsubop == 0x0)
                { // add
                    simulator.setRegister(inst.rd, inst.rs1Value + inst.rs2Value);
                }
                else if (subsubop == 0x1)
                { // sub
                    simulator.setRegister(inst.rd, inst.rs1Value - inst.rs2Value);
                }
                else if (subsubop == 0x2)
                { // slli
                    const uint32_t shamt = getShamt(inst.raw);
                    simulator.setRegister(inst.rd, inst.rs1Value << shamt);
                }
                else if (subsubop == 0x3)
                { // srli
                    const uint32_t shamt = getShamt(inst.raw);
                    simulator.setRegister(inst.rd, inst.rs1Value >> shamt);
                }
            }
            else if (subop == 0x1)
            { // addi
                int32_t imm = getOffset6_8(inst.raw);
                if ((imm >> 13) & 1)
                {
                    imm -= 1 << 14;
                }
                simulator.setRegister(inst.rd, inst.rs1Value + imm);
            }
            else if (subop == 0x2)
            { // lui
                int32_t imm = ((((inst.raw >> 20) & 0xFFF) << 8) | ((inst.raw >> 6) & 0xFF)) << 12;
                simulator.setRegister(inst.rd, imm);
            }
            else if (subop == 0x3)
            { // 入出力命令
                if (subsubop == 0x0)
                { // in
                    int32_t value = simulator.loadWord(simulator.INPUT_ADDRESS * 4, true);
                    simulator.setRegister(inst.rd, value);
                }
                else if (subsubop == 0x1)
                { // fin
                    int32_t value = simulator.loadWord(simulator.INPUT_ADDRESS * 4, false);
                    simulator.setFpRegister(inst.rd, value);
                }
                else if (subsubop == 0x2)
                { // out
                    simulator.storeWord(simulator.OUTPUT_ADDRESS * 4, inst.rs1Value);
                }
            }
            break;

        case 0xC: // ftoi, flt, feq
            if (fpuop == 0x4)
            { // ftoi
                int32_t value = inst.rs1Value;
                simulator.setRegister(inst.rd, simulator.fpu.ftoi(value));
            }
            else if (fpuop == 0x0)
            { // flt
                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                simulator.setRegister(inst.rd, simulator.fpu.flt(value1, value2));
            }
            else if (fpuop == 0x1)
            { // feq
                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                simulator.setRegister(inst.rd, simulator.fpu.feq(value1, value2));
            }
            break;

        case 0xD: // itof, fadd, fmul, fdiv, fmv, fsqrt, ffloor, fmadd
            if (fpuop == 0x7)
            { // itof
                simulator.setFpRegister(inst.rd, simulator.fpu.itof(inst.rs1Value));
            }
            else if (fpuop == 0x0)
            { // fadd
                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                simulator.setFpRegister(inst.rd, simulator.fpu.fadd(value1, value2));
            }
            else if (fpuop == 0x2)
            { // fmul
                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                simulator.setFpRegister(inst.rd, simulator.fpu.fmul(value1, value2));
            }
            else if (fpuop == 0x3)
            { // fdiv
                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                simulator.setFpRegister(inst.rd, simulator.fpu.fdiv(value1, value2));
            }
            else if (fpuop == 0x4)
            { // fmv
                int32_t value = inst.rs1Value;
                simulator.setFpRegister(inst.rd, value);
            }
            else if (fpuop == 0x5)
            { // fsqrt
                int32_t value = inst.rs1Value;
                simulator.setFpRegister(inst.rd, simulator.fpu.fsqrt(value));
            }
            else if (fpuop == 0x6)
            { // ffloor
                int32_t value = inst.rs1Value;
                simulator.setFpRegister(inst.rd, simulator.fpu.ffloor(value));
            }
            else if (fpuop == 0x1)
            { // fmadd

                int32_t value1 = inst.rs1Value;
                int32_t value2 = inst.rs2Value;
                int32_t value3 = inst.rs3Value;

                // FMA演算: value1 * value2 + value3
                int32_t result = simulator.fpu.fadd(simulator.fpu.fmul(value1, value2), value3);
                simulator.setFpRegister(inst.rd, result);
            }
            break;
        // 分岐命令やその他の命令はexecuteInstructionInPipelineを使用
        default:
            simulator.executeInstructionInPipeline(inst.raw, inst.pc, inst.rs1Value, inst.rs2Value);
            break;
        }
    }
}

int Pipeline::getIntLatency(uint64_t instruction) const
{
    // INT命令のレイテンシを判定
    uint32_t opcode = getOpcode(instruction);
    uint32_t subop = getSubop(instruction);
    uint32_t fpuop = getFpuop(instruction);

    switch (opcode)
    {
    case 0x1: // ALU命令と入出力
        if (subop == 0x3)
        {
            return 1; // in, fin, out
        }
        return 1;

    case 0xC: // ftoi, flt, feq
        if (fpuop == 0x4)
        {
            return 2; // ftoiはレイテンシ2
        }
        return 1;
    case 0xF: // jalr
        return 1;
    default:
        return 1;
    }
}

int Pipeline::getFpLatency(uint64_t instruction) const
{
    // FP命令のレイテンシを判定
    uint32_t opcode = getOpcode(instruction);
    uint32_t fpuop = getFpuop(instruction);

    if (opcode == 0xD)
    {
        switch (fpuop)
        {
        case 0x0: // fadd
            return 3;
        case 0x2: // fmul
            return 2;
        case 0x3: // fdiv
            return 5;
        case 0x4: // fmv
            return 1;
        case 0x5: // fsqrt
            return 3;
        case 0x6: // ffloor
            return 5;
        case 0x7: // itof
            return 3;
        case 0x1: // fmadd
            return 4;
        }
    }
    else if (opcode == 0x1 && getSubop(instruction) == 0x3 && getSubsubop(instruction) == 0x1)
    {
        return 1; // finはレイテンシ1
    }
    return 1;
}

std::string Pipeline::getPipelineStateString() const
{
    std::stringstream ss;
    ss << "Pipeline State:\n";

    // Decoded int ステージ
    ss << "Decoded int: ";
    for (int i = 2; i >= 0; i--)
    {
        ss << "int" << i << ":[";
        if (decoded_int[i].empty())
        {
            ss << "empty";
        }
        else
        {
            for (size_t j = 0; j < decoded_int[i].size(); j++)
            {
                if (j > 0)
                    ss << ", ";
                ss << simulator.instToString(decoded_int[i][j].raw);
            }
        }
        ss << "] ";
    }
    ss << "\n";

    // Decoded fp ステージ
    ss << "Decoded fp: ";
    for (int i = 5; i >= 0; i--)
    {
        ss << "fp" << i << ":[";
        if (decoded_fp[i])
        {
            ss << simulator.instToString(decoded_fp[i]->raw);
        }
        else
        {
            ss << "empty";
        }
        ss << "] ";
    }
    ss << "\n";

    // Decoded mem ステージ
    ss << "Decoded mem: ";
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
    ss << "Executed int: [";
    if (executed_int.empty())
    {
        ss << "empty";
    }
    else
    {
        for (size_t i = 0; i < executed_int.size(); i++)
        {
            if (i > 0)
                ss << ", ";
            ss << simulator.instToString(executed_int[i].raw);
        }
    }
    ss << "] ";

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
    ss << "MAed int: [";
    if (MAed_int.empty())
    {
        ss << "empty";
    }
    else
    {
        for (size_t i = 0; i < MAed_int.size(); i++)
        {
            if (i > 0)
                ss << ", ";
            ss << simulator.instToString(MAed_int[i].raw);
        }
    }
    ss << "] ";

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
        for (size_t j = 0; j < decoded_int[i].size(); j++)
        {
            result.push_back({"decoded_int" + std::to_string(i) + "_" + std::to_string(j),
                              const_cast<PipelineInstruction *>(&decoded_int[i][j])});
        }
    }

    for (int i = 0; i < 6; i++)
    {
        if (decoded_fp[i])
        {
            result.push_back({"decoded_fp" + std::to_string(i),
                              const_cast<PipelineInstruction *>(&decoded_fp[i].value())});
        }
    }

    if (decoded_mem)
    {
        result.push_back({"decoded_mem", const_cast<PipelineInstruction *>(&decoded_mem.value())});
    }

    for (size_t i = 0; i < executed_int.size(); i++)
    {
        result.push_back({"executed_int_" + std::to_string(i),
                          const_cast<PipelineInstruction *>(&executed_int[i])});
    }

    if (executed_fp)
    {
        result.push_back({"executed_fp", const_cast<PipelineInstruction *>(&executed_fp.value())});
    }

    if (executed_mem)
    {
        result.push_back({"executed_mem", const_cast<PipelineInstruction *>(&executed_mem.value())});
    }

    for (size_t i = 0; i < MAed_int.size(); i++)
    {
        result.push_back({"MAed_int_" + std::to_string(i),
                          const_cast<PipelineInstruction *>(&MAed_int[i])});
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
    if (opcode == 0x3 || opcode == 0x5)
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
    // パイプライン内の全命令をクリア
    for (int i = 0; i < 3; i++)
    {
        decoded_int[i].clear();
    }
    for (int i = 0; i < 6; i++)
    {
        decoded_fp[i] = std::nullopt;
    }
    decoded_mem = std::nullopt;

    // executed_mem, MAed_int, MAed_fpはそのまま残す
    // 実行中の命令は終わらせる必要がある
}

bool Pipeline::isEmpty() const
{
    // 全てのスロットが空かチェック
    for (int i = 0; i < 3; i++)
    {
        if (!decoded_int[i].empty())
            return false;
    }
    for (int i = 0; i < 6; i++)
    {
        if (decoded_fp[i].has_value())
            return false;
    }
    if (decoded_mem.has_value())
        return false;
    if (!executed_int.empty())
        return false;
    if (executed_fp.has_value())
        return false;
    if (executed_mem.has_value())
        return false;
    if (!MAed_int.empty())
        return false;
    if (MAed_fp.has_value())
        return false;

    return true;
}

int Pipeline::getInstructionType(uint64_t instruction)
{
    uint32_t opcode = getOpcode(instruction);
    uint32_t subop = getSubop(instruction);
    uint32_t subsubop = getSubsubop(instruction);
    uint32_t fpuop = getFpuop(instruction);
    uint32_t branchop = getBranchop(instruction);

    switch (opcode)
    {
    case 0x0:
        return Simulator::NOP;

    case 0x1:
        if (subop == 0x0)
        {
            if (subsubop == 0x0)
                return Simulator::ADD;
            else if (subsubop == 0x1)
                return Simulator::SUB;
            else if (subsubop == 0x2)
                return Simulator::SLLI;
            else if (subsubop == 0x3)
                return Simulator::SRLI;
        }
        else if (subop == 0x1)
        {
            return Simulator::ADDI;
        }
        else if (subop == 0x2)
        {
            return Simulator::LUI;
        }
        else if (subop == 0x3)
        {
            if (subsubop == 0x0)
                return Simulator::INST_IN;
            else if (subsubop == 0x1)
                return Simulator::INST_FIN;
            else if (subsubop == 0x2)
                return Simulator::INST_OUT;
        }
        break;

    case 0x2:
        if (branchop == 0x0)
            return Simulator::BEQ;
        else if (branchop == 0x1)
            return Simulator::BNE;
        break;

    case 0x6:
        if (branchop == 0x0)
            return Simulator::BLT;
        else if (branchop == 0x1)
            return Simulator::BGE;
        break;

    case 0xA:
        if (branchop == 0x0)
            return Simulator::BFLT;
        else if (branchop == 0x1)
            return Simulator::BFGE;
        break;

    case 0xE:
        return Simulator::JAL;
    case 0xF:
        return Simulator::JALR;

    case 0x3:
        if (subop == 0x0)
            return Simulator::LW;
        else if (subop == 0x1)
            return Simulator::LWR;
        break;

    case 0x4:
        return Simulator::SW;

    case 0x5:
        if (subop == 0x0)
            return Simulator::FLW;
        else if (subop == 0x1)
            return Simulator::FLWR;
        break;

    case 0x7:
        return Simulator::FSW;
    case 0x9:
        return Simulator::EBREAK;

    case 0xC:
        if (fpuop == 0x4)
            return Simulator::FTOI;
        else if (fpuop == 0x0)
            return Simulator::FLT;
        else if (fpuop == 0x1)
            return Simulator::FEQ;
        break;

    case 0xD:
        if (fpuop == 0x7)
            return Simulator::ITOF;
        else if (fpuop == 0x0)
        {
            if ((getM2(instruction) & 1) == 1)
                return Simulator::FSUB;
            else
                return Simulator::FADD;
        }
        else if (fpuop == 0x2)
            return Simulator::FMUL;
        else if (fpuop == 0x3)
            return Simulator::FDIV;
        else if (fpuop == 0x4)
            return Simulator::FMV;
        else if (fpuop == 0x5)
            return Simulator::FSQRT;
        else if (fpuop == 0x6)
            return Simulator::FFLOOR;
        else if (fpuop == 0x1)
            return Simulator::FMADD;
        break;
    }

    return -1; // Unknown instruction type
}

bool Pipeline::isJalInstruction(uint64_t instruction) const
{
    return getOpcode(instruction) == 0xE;
}

bool Pipeline::canIssueInPair(const uint64_t inst1, const uint64_t inst2)
{
    // ペア条件チェック
    uint32_t opcode1 = getOpcode(inst1);
    uint32_t opcode2 = getOpcode(inst2);

    // 後ろがnop
    if (opcode2 == 0x0)
    {
        return true; // 任意の命令とnopはペア可能
    }

    if (opcode1 != 0xE && opcode1 != 0xF)
    {                                                     // 前がjal/jalr以外
        if (opcode2 == 0xE ||                             // 後ろがjal
            (opcode2 == 0x1 && getSubop(inst2) == 0x1) || // 後ろがaddi
            (opcode2 == 0x1 && getSubop(inst2) == 0x0 && getSubsubop(inst2) == 0x0))
        { // 後ろがadd
            // データ依存がなければ同時実行可能
            return !hasDataDependency(inst1, inst2);
        }
    }

    return false;
}

bool Pipeline::hasDataDependency(const uint64_t inst1, const uint64_t inst2)
{
    // 2つの命令間のRAW/WAW依存関係をチェック
    PipelineInstruction decodedInst1, decodedInst2;
    decodeInstruction(inst1, 0, decodedInst1);
    decodeInstruction(inst2, 0, decodedInst2);

    // WAW依存
    if (decodedInst1.rd != -1 && decodedInst2.rd != -1)
    {
        if (decodedInst1.rd == decodedInst2.rd && decodedInst1.isFpRd == decodedInst2.isFpRd)
        {
            return true;
        }
    }

    // RAW依存
    if (decodedInst1.rd != -1)
    {
        // rs1依存
        if (decodedInst2.rs1 == decodedInst1.rd && decodedInst2.isFpRs1 == decodedInst1.isFpRd)
        {
            return true;
        }
        // rs2依存
        if (decodedInst2.rs2 == decodedInst1.rd && decodedInst2.isFpRs2 == decodedInst1.isFpRd)
        {
            return true;
        }
        // rs3依存 (fmadd命令用)
        if (decodedInst2.rs3 == decodedInst1.rd && decodedInst2.isFpRs3 == decodedInst1.isFpRd)
        {
            return true;
        }
    }

    return false;
}
void Pipeline::logIssue(uint64_t instruction, int32_t pc)
{
    int instType = getInstructionType(instruction);
    if (instType == Simulator::FMV)
    {
        uint32_t m1 = getM1(instruction);
        if (m1 & 0x1)
        {
            ++simulator.fmvM1Bit0Count; // 第0ビットが立っている（負符号）
        }
        if (m1 & 0x2)
        {
            ++simulator.fmvM1Bit1Count; // 第1ビットが立っている（絶対値）
        }
    }
    simulator.logInstruction(instType);
    simulator.logInstAddr(pc);
}