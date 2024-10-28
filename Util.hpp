#ifndef UTIL_HPP
#define UTIL_HPP
#include <cstdint>

uint32_t getOpcode(uint32_t instruction);
uint32_t getRd(uint32_t instruction);
uint32_t getFunct3(uint32_t instruction);
uint32_t getRs1(uint32_t instruction);
uint32_t getRs2(uint32_t instruction);
uint32_t getFunct7(uint32_t instruction);
int32_t getImmediate(uint32_t instruction);
uint32_t getSign(uint32_t x);
uint32_t getExponent(uint32_t x);
uint32_t getMantissa(uint32_t x);

#endif