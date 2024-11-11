#include "Util.hpp"
#include <iostream>
uint32_t getOpcode(uint32_t instruction)
{
    return instruction & 0x7F;
}

uint32_t getRd(uint32_t instruction)
{
    return (instruction >> 7) & 0x1F;
}

uint32_t getFunct3(uint32_t instruction)
{
    return (instruction >> 12) & 0x7;
}

uint32_t getRs1(uint32_t instruction)
{
    return (instruction >> 15) & 0x1F;
}

uint32_t getRs2(uint32_t instruction)
{
    return (instruction >> 20) & 0x1F;
}

uint32_t getFunct7(uint32_t instruction)
{
    return (instruction >> 25) & 0x7F;
}

int32_t getImmediate(uint32_t instruction)
{
    return (instruction >> 20);
}
uint32_t getSign(uint32_t x)
{
    return (x >> 31) & 0x1;
}
uint32_t getExponent(uint32_t x)
{
    return (x >> 23) & 0xFF;
}
uint32_t getMantissa(uint32_t x)
{
    return x & 0x7FFFFF;
}
void printBoundary(){
    std::cout << "--------------------------------------------" << std::endl;
}