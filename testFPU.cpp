#include <iostream>
#include <iomanip>
#include <bit>
#include <string>
#include <unordered_map>
#include <functional>
#include "FPU.hpp"
void testFtoi(FPU &fpu)
{
    std::cout << "Testing ftoi..." << std::endl;

    struct
    {
        uint32_t floatBits;
        int32_t expectedInt;
    } testCases[] = {
        {0x3F800000, 1},          // 1.0 -> 1
        {0x40000000, 2},          // 2.0 -> 2
        {0x40400000, 3},          // 3.0 -> 3
        {0xC0000000, -2},         // -2.0 -> -2
        {0xBF800000, -1},         // -1.0 -> -1
        {0x41200000, 10},         // 10.0 -> 10
        {0xC1200000, -10},        // -10.0 -> -10
        {0x7F7FFFFF, 2147483647}, // 最大正のfloat -> INT_MAX
        {0xFF7FFFFF, -2147483648} // 最小負のfloat -> INT_MIN
    };

    for (const auto &testCase : testCases)
    {
        int32_t result = fpu.ftoi(testCase.floatBits);
        std::cout << "ftoi(" << std::hex << "0x" << testCase.floatBits << ") = " << std::dec << result
                  << " (expected: " << testCase.expectedInt << ")" << std::endl;
    }
}

void testItof(FPU &fpu)
{
    std::cout << "\nTesting itof..." << std::endl;
    struct
    {
        int32_t intVal;
        uint32_t expectedFloatBits;
    } testCases[] = {
        {1, 0x3F800000},          // 1 -> 1.0
        {2, 0x40000000},          // 2 -> 2.0
        {3, 0x40400000},          // 3 -> 3.0
        {-2, 0xC0000000},         // -2 -> -2.0
        {-1, 0xBF800000},         // -1 -> -1.0
        {10, 0x41200000},         // 10 -> 10.0
        {-10, 0xC1200000},        // -10 -> -10.0
        {2147483647, 0x4F000000}, // INT_MAX -> 大きい正のfloat
        {-2147483648, 0xCF000000} // INT_MIN -> 大きい負のfloat
    };

    for (const auto &testCase : testCases)
    {
        uint32_t result = fpu.itof(testCase.intVal);
        std::cout << "itof(" << testCase.intVal << ") = " << std::hex << "0x" << result
                  << " (expected: 0x" << testCase.expectedFloatBits << ")" <<std::dec<< std::endl;
    }
}
void testFPU(FPU &fpu)
{
    
    std::string instruction;
    float input1, input2;
    int32_t iinput;
    const std::unordered_map<std::string, std::function<void()>> dispatchTable = {
        {"fadd", [&]
         {
             std::cin >> input1;
             std::cin >> input2;
             float result =
                 std::bit_cast<float>(
                     fpu.fadd(std::bit_cast<uint32_t>(input1), std::bit_cast<uint32_t>(input2)));
             std::cout << "fadd (" << input1 << " + " << input2 << ") = " << result << std::endl;
         }},
        {"fsub", [&]
         {
             std::cin >> input1;
             std::cin >> input2;
             float result =
                 std::bit_cast<float>(
                     fpu.fsub(std::bit_cast<uint32_t>(input1), std::bit_cast<uint32_t>(input2)));
             std::cout << "fsub (" << input1 << " - " << input2 << ") = " << result << std::endl;
         }},
        {"fmul", [&]
         {
             std::cin >> input1;
             std::cin >> input2;
             float result =
                 std::bit_cast<float>(
                     fpu.fmul(std::bit_cast<uint32_t>(input1), std::bit_cast<uint32_t>(input2)));
             std::cout << "fmul (" << input1 << " * " << input2 << ") = " << result << std::endl;
         }},
        {"fdiv", [&]
         {
             std::cin >> input1;
             std::cin >> input2;
             float result =
                 std::bit_cast<float>(
                     fpu.fdiv(std::bit_cast<uint32_t>(input1), std::bit_cast<uint32_t>(input2)));
             std::cout << "fdiv (" << input1 << " / " << input2 << ") = " << result << std::endl;
         }},
        {"finv", [&]
         {
             std::cin >> input1;
             uint32_t m = std::bit_cast<uint32_t>(input1) & 0x7FFFFF;
             float result =
                 std::bit_cast<float>(fpu.finv(m));

             std::cout << "finv (" << input1 << ") = " << result << std::endl;
         }},
        {"fsqrt", [&]
         {
             std::cin >> input1;
             float result =
                 std::bit_cast<float>(
                     fpu.fsqrt(std::bit_cast<uint32_t>(input1)));
             std::cout << "fsqrt (" << input1 << ") = " << result << std::endl;
         }},
         {"ftoi", [&]
         {
             std::cin >> input1;
             float result =
                 std::bit_cast<float>(fpu.ftoi(std::bit_cast<uint32_t>(input1)));

             std::cout << "ftoi (" << input1 << ") = " << result << std::endl;
         }},
         {"itof", [&]
         {
             std::cin >> iinput;
             float result =
                 std::bit_cast<float>(fpu.itof(iinput));

             std::cout << "itof (" << iinput << ") = " << result << std::endl;
         }}};
    while (1)
    {
        std::cin >> instruction;

        if (dispatchTable.find(instruction) != dispatchTable.end())
        {
            dispatchTable.at(instruction)();
        }
        else
        {
            throw std::runtime_error("Unknown instruction");
        }
    }
}

int main()
{
    FPU fpu;
    testFtoi(fpu);
    testItof(fpu);
    testFPU(fpu);
    return 0;
}