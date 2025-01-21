#include "FPU.hpp"
#include "ftable.hpp"
#include <bit>
#include <iostream>
#include <cmath>

uint32_t FPU::fadd(uint32_t x1, uint32_t x2)
{
    return addOrSub(x1, x2, false);
}

uint32_t FPU::fsub(uint32_t x1, uint32_t x2)
{
    return addOrSub(x1, x2, true);
}

uint32_t FPU::fmul(uint32_t x1, uint32_t x2)
{
    uint32_t s1 = getSign(x1);
    uint32_t e1 = getExponent(x1);
    uint32_t m1 = getMantissa(x1);
    uint32_t s2 = getSign(x2);
    uint32_t e2 = getExponent(x2);
    uint32_t m2 = getMantissa(x2);

    uint32_t m1_extend = (1 << 23) | m1;
    uint32_t m2_extend = (1 << 23) | m2;

    uint64_t my_temp1 = static_cast<uint64_t>(m1_extend) * static_cast<uint64_t>(m2_extend) + (1ULL << 23);

    bool carry = (my_temp1 >> 47) & 1;

    uint32_t my_temp2 = carry ? (my_temp1 >> 24) & 0x7FFFFF : (my_temp1 >> 23) & 0x7FFFFF;

    uint32_t sy = s1 ^ s2;

    int32_t ey_temp1 = static_cast<int32_t>(e1) + static_cast<int32_t>(e2) - 127;
    int32_t ey_temp2 = carry ? ey_temp1 + 1 : ey_temp1;

    bool underflow = (e1 == 0 || e2 == 0 || ey_temp2 <= 0);
    bool overflow = (e1 == 255 || e2 == 255 || ey_temp2 >= 255);

    uint32_t ey = underflow ? 0 : (overflow ? 0 : static_cast<uint32_t>(ey_temp2 & 0xFF));
    uint32_t my = (underflow || overflow) ? 0 : my_temp2;

    return (sy << 31) | (ey << 23) | my;
}

uint32_t FPU::finv(uint32_t xm)
{
    uint32_t key = xm >> 13;

    uint32_t a = finv_table[key].a;
    uint32_t c = finv_table[key].b;

    uint32_t d = xm & 0x1FFF;
    uint32_t ad = a * d;
    uint32_t ad_shifted = static_cast<uint32_t>(ad >> 12);

    uint32_t my = c - ad_shifted;

    return (0 << 31) | (126 << 23) | (my & 0x7FFFFF);
}

uint32_t FPU::fsqrt(uint32_t x)
{
    uint32_t s = getSign(x);
    uint32_t e = getExponent(x);
    uint32_t m = getMantissa(x);

    if (e == 0)
        return 0;

    bool x_in_2_4 = (e & 1) == 0;
    uint32_t key = (x_in_2_4 << 9) | (m >> 14);

    uint32_t a = fsqrt_table[key].a | (1 << 13);
    uint32_t c = fsqrt_table[key].b;

    uint32_t d = m & 0x3FFF;
    uint32_t ad = a * d;
    uint32_t ad_shifted = x_in_2_4 ? (ad >> 14) : (ad >> 15);

    uint32_t my = c + ad_shifted;

    uint32_t ey = (e >> 1) + 63 + (!x_in_2_4);

    return (s << 31) | ((ey & 0xFF) << 23) | (my & 0x7FFFFF);
}

uint32_t FPU::fdiv(uint32_t x1, uint32_t x2)
{
    uint32_t s1, e1, m1, s2, e2, m2;
    s1 = getSign(x1);
    e1 = getExponent(x1);
    m1 = getMantissa(x1);
    s2 = getSign(x2);
    e2 = getExponent(x2);
    m2 = getMantissa(x2);
    uint32_t sdiv = s1 ^ s2;
    uint32_t m1inv = finv(m2);
    uint32_t m2ex = (0x0 << 31) | (0x7f << 23) | m1;
    uint32_t mdiv = fmul(m2ex, m1inv);

    uint32_t ediv = (((mdiv >> 23) & 1) == 0) ? (e1 - e2 + 126) : (e1 - e2 + 127);

    return (((ediv >> 8) & 1) || e1 == 0) ? 0 : ((sdiv & 1) << 31) | ((ediv & 0xFF) << 23) | (mdiv & 0x7FFFFF);
}

uint32_t FPU::addOrSub(uint32_t x1, uint32_t x2, bool isSubtraction)
{
    uint32_t s1 = getSign(x1);
    uint32_t e1 = getExponent(x1);
    uint32_t m1 = getMantissa(x1);
    uint32_t s2 = getSign(x2);
    uint32_t e2 = getExponent(x2);
    uint32_t m2 = getMantissa(x2);

    if (isSubtraction)
    {
        s2 ^= 1;
    }

    uint32_t m1a = (e1 == 0) ? m1 : (1 << 23) | m1;
    uint32_t m2a = (e2 == 0) ? m2 : (1 << 23) | m2;

    uint32_t e1a = (e1 == 0) ? 1 : e1;
    uint32_t e2a = (e2 == 0) ? 1 : e2;

    bool ce = e1a <= e2a;
    uint32_t tde = ce ? (e2a - e1a) : (e1a - e2a);
    uint32_t de = (tde > 31) ? 31 : tde & 0x1F;

    bool sel = (de == 0) ? (m1a <= m2a) : ce;
    uint32_t m_big = sel ? m2a : m1a;
    uint32_t m_small_prev = sel ? m1a : m2a;
    uint32_t e_big = sel ? e2a : e1a;
    uint32_t s_big = sel ? s2 : s1;

    uint64_t m_small = static_cast<uint64_t>(m_small_prev) << 31;
    m_small >>= de;
    bool tstck = (m_small & 0x1FFFFFFF) > 0;
    uint64_t my1 = (s1 == s2) ? (static_cast<uint64_t>(m_big) << 2) + (m_small >> 29)
                              : (static_cast<uint64_t>(m_big) << 2) - (m_small >> 29);

    uint32_t e_big_plus = e_big + 1;
    uint32_t eyd = (my1 & (1ULL << 26)) ? e_big_plus : e_big;
    uint64_t myd = (my1 & (1ULL << 26)) ? (my1 >> 1) : my1;
    bool stck = (my1 & (1ULL << 26)) ? ((tstck) || (my1 & 1)) : (tstck);

    uint32_t se = 26;
    for (int i = 25; i >= 0; --i)
    {
        if (myd & (1 << i))
        {
            se = 25 - i;
            break;
        }
    }

    int32_t eyf = static_cast<int32_t>(eyd) - se;
    uint64_t myf = (eyf > 0) ? (myd << se) : (myd << (eyd - 1));
    uint32_t eyr = (eyf > 0) ? eyf : 0;

    uint32_t myr = ((myf & 2) && !(myf & 1) && !stck && (myf & 4)) ||
                           ((myf & 2) && !(myf & 1) && (s1 == s2) && stck) ||
                           ((myf & 2) && (myf & 1))
                       ? (myf >> 2) + 1
                       : (myf >> 2);

    uint32_t eyri = eyr + 1;

    uint32_t ey = (myr & (1 << 24)) ? eyri : ((myr & 0xFFFFFF) == 0 ? 0 : eyr);
    uint32_t my = (myr & (1 << 24)) ? 0 : ((myr & 0xFFFFFF) == 0 ? 0 : (myr & 0x7FFFFF));

    return (s_big << 31) | (ey << 23) | my;
}

uint32_t FPU::ftoi(uint32_t x)
{
    bool s = getSign(x);
    uint32_t e = getExponent(x);
    uint32_t m = getMantissa(x);

    uint32_t m_ex = (1 << 23) | m;
    m_ex <<= 8;

    uint32_t m_ex_shift = std::abs((int32_t)e - 157) >= 32 ? 0 : ((e > 157) ? (m_ex << (e - 157)) : (m_ex >> (157 - e)));

    if (m_ex_shift & 1)
    {
        m_ex_shift += 1;
    }

    return s ? ~(m_ex_shift >> 1) + 1 : (m_ex_shift >> 1);
}

uint32_t FPU::itof(uint32_t x)
{
    if (x == 0)
        return 0;

    bool sx = getSign(x);
    uint32_t xabs = sx ? ~x + 1 : x;

    int shifts = std::countl_zero(xabs) + 1;

    uint32_t xshift = xabs << shifts;
    uint32_t r = (xshift >> 8) & 1;
    uint32_t my = (xshift >> 9) + r;

    uint32_t ey;
    if (shifts == 0)
    {
        ey = 0;
    }
    else if ((xshift >> 9) == 1 && r)
    {
        ey = 160 - shifts;
    }
    else
    {
        ey = 159 - shifts;
    }

    return (sx << 31) | (ey << 23) | (my & 0x7FFFFF);
}

uint32_t FPU::ffloor(uint32_t x)
{
    int32_t xint = std::bit_cast<int32_t>(ftoi(x));
    int32_t xint_minus = xint - 1;

    uint32_t xfloat = itof(std::bit_cast<uint32_t>(xint));
    uint32_t xfloat_minus = itof(std::bit_cast<uint32_t>(xint_minus));

    bool flag = flt(x, xfloat);

    uint32_t e = getExponent(x);

    if (e > 150)
    {
        return x;
    }
    else if (flag)
    {
        return xfloat_minus;
    }
    else
    {
        return xfloat;
    }
}

uint32_t FPU::fneg(uint32_t x)
{
    return x ^ (1 << 31);
}
uint32_t FPU::fabs(uint32_t x)
{
    return x & (~(1 << 31));
}
bool FPU::flt(uint32_t x1, uint32_t x2)
{
    uint32_t s1, e1, m1, s2, e2, m2;
    s1 = getSign(x1);
    e1 = getExponent(x1);
    m1 = getMantissa(x1);
    s2 = getSign(x2);
    e2 = getExponent(x2);
    m2 = getMantissa(x2);

    if (s1 == 0 && s2 == 0)
    {
        // 両方正
        return (e1 == e2) ? (m1 < m2) : (e1 < e2);
    }
    else if (s1 == 0 && s2 == 1)
    {
        // 正 vs 負
        return false;
    }
    else if (s1 == 1 && s2 == 0)
    {
        // 負 vs 正
        return true;
    }
    else
    {
        // 両方負
        return (e1 == e2) ? (m1 > m2) : (e1 > e2);
    }
}

bool FPU::feq(uint32_t x1, uint32_t x2)
{
    uint32_t e1 = getExponent(x1);
    uint32_t e2 = getExponent(x2);

    bool is_zero1 = (e1 == 0);
    bool is_zero2 = (e2 == 0);
    return (is_zero1 && is_zero2) ? true : (x1 == x2);
}