#ifndef UTIL_HPP
#define UTIL_HPP
#include <cstdint>
#include <streambuf>
#include <iostream>

[[nodiscard]]
inline uint32_t getOpcode(uint32_t instruction) noexcept
{
    return instruction & 0xF;
}

[[nodiscard]]
inline uint32_t getSubop(uint32_t instruction) noexcept
{
    return (instruction >> 4) & 0x3;
}

[[nodiscard]]
inline uint32_t getFpuop(uint32_t instruction) noexcept
{
    return (instruction >> 4) & 0xF;
}

[[nodiscard]]
inline uint32_t getRd(uint32_t instruction) noexcept
{
    return (instruction >> 14) & 0x3F;
}

[[nodiscard]]
inline uint32_t getRs1(uint32_t instruction) noexcept
{
    return (instruction >> 20) & 0x3F;
}

[[nodiscard]]
inline uint32_t getRs2(uint32_t instruction) noexcept
{
    return (instruction >> 26) & 0x3F;
}

[[nodiscard]]
inline int32_t getImmediate(uint32_t instruction) noexcept
{
    return (instruction >> 6) & 0x3FFF ;
}

[[nodiscard]]
inline uint32_t getSign(uint32_t x) noexcept
{
    return (x >> 31) & 0x1;
}

[[nodiscard]]
inline uint32_t getExponent(uint32_t x) noexcept
{
    return (x >> 23) & 0xFF;
}

[[nodiscard]]
inline uint32_t getMantissa(uint32_t x) noexcept
{
    return x & 0x7FFFFF;
}

void printBoundary();

class CerrRedirect {
public:
    CerrRedirect(std::ostream& newStream);
    ~CerrRedirect();

private:
    std::streambuf* oldBuffer;
};


#endif