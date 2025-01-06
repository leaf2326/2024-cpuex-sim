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

    uint64_t product = static_cast<uint64_t>(m1_extend) * static_cast<uint64_t>(m2_extend);

    product += (1ULL << 23);

    bool carry = product & (1ULL << 47);
    uint32_t my = carry ? (product >> 24) & 0x7FFFFF : (product >> 23) & 0x7FFFFF;

    uint32_t ey_temp = e1 + e2 - 127 + carry;

    bool underflow = (e1 == 0 || e2 == 0 || ey_temp == 0);
    bool overflow = (e1 == 0xFF || e2 == 0xFF || ey_temp >= 255);

    uint32_t ey = underflow ? 0 : (overflow ? 0 : ey_temp & 0xFF);
    my = underflow || overflow ? 0 : my;

    return (s1 ^ s2) << 31 | (ey << 23) | my;
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
    uint32_t s1, e1, m1, s2, e2, m2;
    s1 = getSign(x1);
    e1 = getExponent(x1);
    m1 = getMantissa(x1);
    s2 = getSign(x2);
    e2 = getExponent(x2);
    m2 = getMantissa(x2);

    m1 |= (e1 > 0) ? (1 << 23) : 0;
    m2 |= (e2 > 0) ? (1 << 23) : 0;

    if (isSubtraction)
    {
        s2 ^= 1; // 符号反転
    }

    int32_t e1a = e1;
    int32_t e2a = e2;

    uint32_t ce = 0;
    int32_t tde = 0;
    if (e1a > e2a)
    {
        ce = 0;
        tde = e1a - e2a;
    }
    else
    {
        ce = 1;
        tde = e2a - e1a;
    }

    uint32_t de = (tde > 31) ? 31 : tde;
    uint32_t sel = (de == 0) ? (m1 > m2 ? 0 : 1) : ce;

    uint32_t ms, mi, es, ss;
    if (sel == 0)
    {
        ms = m1;
        mi = m2;
        es = e1a;
        // ei = e2a;
        ss = s1;
    }
    else
    {
        ms = m2;
        mi = m1;
        es = e2a;
        // ei = e1a;
        ss = s2;
    }

    uint64_t mie = static_cast<uint64_t>(mi) << 31;
    uint64_t mia = mie >> de;

    uint32_t tstck = (mia & 0x1FFFFFFF) != 0;

    uint64_t mye;
    if (s1 == s2)
    {
        mye = (static_cast<uint64_t>(ms) << 2) + (mia >> 29);
    }
    else
    {
        mye = (static_cast<uint64_t>(ms) << 2) - (mia >> 29);
    }

    uint32_t esi = es + 1;

    uint32_t eyd;
    uint64_t myd;
    uint32_t stck;
    if (mye & (1 << 26))
    {
        if (esi == 255)
        {
            eyd = 255;
            myd = (1ULL << 25);
            stck = 0;
        }
        else
        {
            eyd = esi;
            myd = mye >> 1;
            stck = tstck || (mye & 1);
        }
    }
    else
    {
        eyd = es;
        myd = mye;
        stck = tstck;
    }

    uint32_t se = 26;
    for (int i = 25; i >= 0; --i)
    {
        if (myd & (1 << i))
        {
            se = 25 - i;
            break;
        }
    }

    int32_t eyf = eyd - se;
    uint64_t myf;
    uint32_t eyr;
    if (eyf > 0)
    {
        myf = myd << se;
        eyr = eyf & 0xFF;
    }
    else
    {
        myf = myd << ((eyd & 0x1F) - 1);
        eyr = 0;
    }

    uint64_t myr;
    if (((myf & 0x2) && !(myf & 0x1) && !stck && (myf & 0x4)) || ((myf & 0x2) && (!(myf & 0x1)) && (s1 == s2) && stck) || ((myf & 0x2) && (myf & 0x1)))
    {
        myr = (myf >> 2) + 1;
    }
    else
    {
        myr = myf >> 2;
    }

    uint32_t eyri = eyr + 1;

    uint32_t ey;
    uint32_t my;
    if (myr & (1 << 24))
    {
        ey = eyri;
        my = 0;
    }
    else if ((myr & 0xFFFFFF) == 0)
    {
        ey = 0;
        my = 0;
    }
    else
    {
        ey = eyr;
        my = myr & 0x7FFFFF;
    }

    uint32_t sy = (ey == 0 && my == 0) ? (s1 && s2) : ss;

    return (sy << 31) | (ey << 23) | my;
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