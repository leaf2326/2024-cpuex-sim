#include <iostream>
#include <iomanip>
#include <bit>
#include "FPU.hpp"

void testFPU() {
    FPU fpu;
    float input1, input2;
    std::cin >> input1;
    std::cin >> input2;
    uint32_t x1 = std::bit_cast<uint32_t>(input1);
    uint32_t x2 = std::bit_cast<uint32_t>(input2);
    uint32_t result_add = fpu.fadd(x1, x2);
    uint32_t result_sub = fpu.fsub(x1, x2);
    uint32_t result_mul = fpu.fmul(x1, x2);
    float add_result = std::bit_cast<float>(result_add);
    float sub_result = std::bit_cast<float>(result_sub);
    float mul_result = std::bit_cast<float>(result_mul);
    std::cout << std::fixed << std::setprecision(7);
    std::cout << "fadd (" << input1 << " + " << input2 << ") = " << add_result << std::endl;
    std::cout << "fsub (" << input1 << " - " << input2 << ") = " << sub_result << std::endl;
    std::cout << "fmul (" << input1 << " * " << input2 << ") = " << mul_result << std::endl;
}

int main() {
    testFPU();
    return 0;
}