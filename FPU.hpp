#ifndef FPU_HPP
#define FPU_HPP
#include "Util.hpp"
#include <cstdint>

class FPU
{
public:
    uint32_t fadd(uint32_t x1, uint32_t x2);

    uint32_t fsub(uint32_t x1, uint32_t x2);

    uint32_t fmul(uint32_t x1, uint32_t x2);

private:
    uint32_t addOrSub(uint32_t x1, uint32_t x2, bool isSubtraction);
};

#endif