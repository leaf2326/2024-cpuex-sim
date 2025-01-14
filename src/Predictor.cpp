#include "Predictor.hpp"

GSharePredictor::GSharePredictor()
    : prediction_table(TABLE_SIZE, 1), branch_history(0)
{
}

[[nodiscard]]
uint16_t GSharePredictor::calculateIndex(uint32_t pc)
{
    uint16_t pc_upper = (pc >> 13) & 0x3; // PC[14:13]
    uint16_t pc_lower = pc & 0x1FFF;      // PC[12:0]
    uint16_t history = branch_history;
    return (static_cast<uint16_t>(pc_upper) ^ pc_lower ^ history) & 0x1FFF;
}
[[nodiscard]]
bool GSharePredictor::predict(uint32_t pc)
{
    uint16_t index = calculateIndex(pc);
    return prediction_table[index] >= 2;
}

void GSharePredictor::update(uint32_t pc, bool taken)
{
    uint16_t index = calculateIndex(pc);

    if (taken)
    {
        if (prediction_table[index] < 3)
            prediction_table[index]++;
    }
    else
    {
        if (prediction_table[index] > 0)
            prediction_table[index]--;
    }

    branch_history = ((branch_history << 1) | (taken ? 1 : 0)) & 0x1FFF;
}