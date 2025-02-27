#ifndef UTIL_HPP
#define UTIL_HPP
#include <cstdint>
#include <streambuf>
#include <iostream>

[[nodiscard]]
inline uint32_t getOpcode(uint64_t instruction) noexcept
{
    return instruction & 0xF;
}

[[nodiscard]]
inline uint32_t getSubop(uint64_t instruction) noexcept
{
    return (instruction >> 4) & 0x3;
}

inline uint32_t getSubsubop(uint64_t instruction) noexcept
{
    return (instruction >> 6) & 0x3;
}

[[nodiscard]]
inline uint32_t getShamt(uint64_t instruction) noexcept
{
    return (instruction >> 8) & 0x3;
}

[[nodiscard]]
inline uint32_t getBranchop(uint64_t instruction) noexcept
{
    return (instruction >> 4) & 0x1;
}

[[nodiscard]]
inline uint32_t getM1(uint64_t instruction) noexcept
{
    return (instruction >> 34) & 0x3;
}

[[nodiscard]]
inline uint32_t getM2(uint64_t instruction) noexcept
{
    return (instruction >> 32) & 0x3;
}

[[nodiscard]]
inline uint32_t getM3(uint64_t instruction) noexcept
{
    return (instruction >> 10) & 0x1;
}

[[nodiscard]]
inline uint32_t getFpuop(uint64_t instruction) noexcept
{
    return (instruction >> 11) & 0x7;
}

[[nodiscard]]
inline uint32_t getRd(uint64_t instruction) noexcept
{
    return (instruction >> 14) & 0x3F;
}

[[nodiscard]]
inline uint32_t getRs1(uint64_t instruction) noexcept
{
    return (instruction >> 20) & 0x3F;
}

[[nodiscard]]
inline uint32_t getRs2(uint64_t instruction) noexcept
{
    return (instruction >> 26) & 0x3F;
}

[[nodiscard]]
inline uint32_t getRs3(uint64_t instruction) noexcept
{
    return (instruction >> 4) & 0x3F;
}

[[nodiscard]]
inline int32_t getImmediate(uint64_t instruction) noexcept
{
    return (instruction >> 5) & 0x7FFF;
}

[[nodiscard]]
inline int32_t getOffset14(uint64_t instruction) noexcept
{
    return (instruction >> 6) & 0x3FFF;
}

[[nodiscard]]
inline int32_t getOffset6_8(uint64_t instruction) noexcept
{
    return ((((instruction >> 26) & 0x3F) << 8) | ((instruction >> 6) & 0xFF));
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

class CerrRedirect
{
public:
    CerrRedirect(std::ostream &newStream);
    ~CerrRedirect();

private:
    std::streambuf *oldBuffer;
};

#endif