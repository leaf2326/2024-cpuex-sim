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

    [[nodiscard]] uint16_t calculateIndex(uint32_t pc);

public:
    GSharePredictor();
    [[nodiscard]] bool predict(uint32_t pc);
    void update(uint32_t pc, bool taken);
    [[nodiscard]] inline uint8_t getPrediction(uint32_t pc) {
        return prediction_table[calculateIndex(pc)];
    }
};

#endif
