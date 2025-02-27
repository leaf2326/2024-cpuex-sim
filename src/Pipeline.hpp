#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <string>

class Simulator;

// パイプラインに入る命令情報
struct PipelineInstruction
{
    uint64_t raw;          // 命令の生データ
    int32_t pc;
    int rd;                // rdレジスタ（なければ-1）
    bool isFpRd;
    int rs1, rs2;          // ソースレジスタ（なければ-1）
    bool isFpRs1, isFpRs2;
    bool isMemory;
    bool isLoad;
    bool isStore;
    bool isBranch;
    bool isJalr;
    bool isEbreak;
    int cacheWaitCycles;  // キャッシュ待機サイクル数
    bool shouldDisappear; // executed_memから消滅するべきか

    int32_t rs1Value;
    int32_t rs2Value;
};

using InstSlot = std::optional<PipelineInstruction>;

class Pipeline
{
public:
    Pipeline(Simulator &sim);

    // 命令を発行しようとする（成功したらtrue）
    bool tryIssue(uint64_t instruction, int32_t pc);

    void advance();

    std::string getPipelineStateString() const;

    std::vector<std::pair<std::string, PipelineInstruction *>> getCurrentInstructions() const;

    std::string formatInstruction(const PipelineInstruction &inst) const;

    // 統計情報
    [[nodiscard]] inline uint64_t getStallCount() const { return stallCount; }
    [[nodiscard]] inline uint64_t getWbCollisionIntFpCount() const { return wbCollisionIntFpCount; }
    [[nodiscard]] inline uint64_t getWbCollisionMemCount() const { return wbCollisionMemCount; }
    [[nodiscard]] inline uint64_t getBranchBypassStallCount() const { return branchBypassStallCount; }
    [[nodiscard]] inline uint64_t getFpuRawStallCount() const { return fpuRawStallCount; }
    [[nodiscard]] inline uint64_t getLoadRawStallCount() const { return loadRawStallCount; }
    [[nodiscard]] inline uint64_t getMemoryStallCycles() const { return memoryStallCycles; }
    [[nodiscard]] inline uint64_t getTotalCycles() const { return totalCycles; }

    void countRawHazard(const PipelineInstruction &inst, const PipelineInstruction &source);

    void countMemoryStall(uint64_t cycles);

    bool isEmpty() const;

private:
    Simulator &simulator;

    uint64_t stallCount = 0;             // 全体ストール数
    uint64_t wbCollisionIntFpCount = 0;  // WB衝突(int/fp)によるストール数
    uint64_t wbCollisionMemCount = 0;    // WB衝突(メモリ)によるストール数
    uint64_t branchBypassStallCount = 0; // 分岐命令の追い越し防止ストール数
    uint64_t fpuRawStallCount = 0;       // FPU RAWストール数
    uint64_t loadRawStallCount = 0;      // Load RAWストール数
    uint64_t memoryStallCycles = 0;      // メモリストールサイクル数
    uint64_t totalCycles = 0;            // 合計サイクル数

    // パイプラインステージ
    InstSlot decoded_int[3]; // int2, int1, int0
    InstSlot decoded_fp[6];  // fp5, fp4, fp3, fp2, fp1, fp0
    InstSlot decoded_mem;
    InstSlot executed_int, executed_fp, executed_mem;
    InstSlot MAed_int, MAed_fp;

    bool fetchInstruction(int32_t address);

    void decodeInstruction(uint64_t raw, int32_t pc, PipelineInstruction &inst);
    bool checkDependencies(const PipelineInstruction &inst) const;
    bool checkContention(const PipelineInstruction &inst) const;
    bool checkBranchHazard(const PipelineInstruction &inst) const;
    void handleWAWHazards(const PipelineInstruction &inst);
    void executeAtExecutedStage(PipelineInstruction &inst);
    int getIntLatency(uint64_t instruction) const;
    int getFpLatency(uint64_t instruction) const;
    void flushPipeline();
    int getInstructionType(uint64_t instruction);
    bool isJalInstruction(uint64_t instruction) const;
};
#endif // PIPELINE_HPP