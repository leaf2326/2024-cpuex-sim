#include <iostream>
#include <iomanip>
#include <bit>
#include <string>
#include <unordered_map>
#include <functional>
#include "FPU.hpp"

void testFPU()
{
    FPU fpu;
    std::string instruction;
    float input1, input2;
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
    testFPU();
    return 0;
}