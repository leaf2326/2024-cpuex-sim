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

    //finvはfdiv用の組み込み関数。仮数部[1.0, 2.0)を受け取り仮数部の逆数(0.5, 1.0]を返す
    uint32_t finv(uint32_t xm);

    uint32_t fsqrt(uint32_t x);

    uint32_t fdiv(uint32_t x1, uint32_t x2);

private:
    uint32_t addOrSub(uint32_t x1, uint32_t x2, bool isSubtraction);
};

#endif