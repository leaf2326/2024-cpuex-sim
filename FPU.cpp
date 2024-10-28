#include "FPU.hpp"
#include <bit>
#include <iostream>
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
    uint32_t s1, e1, m1, s2, e2, m2;
    s1 = getSign(x1);
    e1 = getExponent(x1);
    m1 = getMantissa(x1);
    s2 = getSign(x2);
    e2 = getExponent(x2);
    m2 = getMantissa(x2);

    uint32_t h1, l1, h2, l2;
    h1 = ((m1 >> 11) & 0xFFF) | (1 << 12);
    l1 = m1 & 0x7FF;
    h2 = ((m2 >> 11) & 0xFFF) | (1 << 12);
    l2 = m2 & 0x7FF;

    uint32_t sy, ey, my;
    // Stage1
    uint32_t hh, hl, lh;
    hh = h1 * h2;
    hl = h1 * l2;
    lh = h2 * l1;
    ey = e1 + e2 + 128;
    sy = s1 ^ s2;

    // Stage2
    my = hh + (hl >> 11) + (lh >> 11) + 2;

    // Stage3
    uint32_t underflowbit = (ey >> 8) & 0x1;
    uint32_t ms = 8 - std::countl_zero(my);
    if (!underflowbit)
    {
        ey = 0;
    }
    else
    {
        ey += ms;
    }
    my = (my >> ms) & 0x7FFFFF;

    return (sy << 31) | ((ey & 0xFF) << 23) | my;
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