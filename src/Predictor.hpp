#ifndef PREDICTOR_HPP
#define PREDICTOR_HPP

#include <vector>
#include <cstdint>

class GSharePredictor
{
private:
    static constexpr size_t TABLE_SIZE = 1 << 13; // 2^13
    std::vector<uint8_t> prediction_table;
    uint16_t branch_history;

    [[nodiscard]]
    inline uint16_t calculateIndex(uint32_t pc) const noexcept
    {
        return (((pc >> 13) & 0x3) ^ (pc & 0x1FFF) ^ branch_history) & 0x1FFF;
    }

public:
    GSharePredictor();
    [[nodiscard]]
    inline bool predict(uint32_t pc) const
    {
        return prediction_table[calculateIndex(pc)] >= 2;
    }
    void update(uint32_t pc, bool taken);
    [[nodiscard]] inline uint8_t getPrediction(uint32_t pc)
    {
        return prediction_table[calculateIndex(pc)];
    }
};

#endif
