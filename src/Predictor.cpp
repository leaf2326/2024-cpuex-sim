#include "Predictor.hpp"

GSharePredictor::GSharePredictor()
    : prediction_table(TABLE_SIZE, 1), branch_history(0)
{
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