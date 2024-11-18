#include <iostream>
#include <iomanip>
#include <bit>
#include <string>
#include <unordered_map>
#include <functional>
#include "FPU.hpp"
#include <cmath>
#include <limits>
#include <xmmintrin.h>
#include <emmintrin.h>
#include "FPU.hpp"

inline int32_t Round(const float &x)
{
    return _mm_cvtss_si32(_mm_load_ss(&x));
}

// 許容誤差
const float EPSILON = std::pow(2, -126);
const float TOLERANCE_ADD = std::pow(2, -23);
const float TOLERANCE_MUL = std::pow(2, -22);
const float TOLERANCE_DIV = std::pow(2, -20);
const float TOLERANCE_SQRT = std::pow(2, -20);

// テスト用に固定する値
const float testValues[] = {0.0f, 1.0f, -1.0f}; //, std::numeric_limits<float>::max(), std::numeric_limits<float>::min()

// FADD シングルループテスト
void singleLoopTestFadd(FPU &fpu)
{
    std::cout << "Single Loop Testing FADD..." << std::endl;
    for (uint32_t i = 0; i <= 0xFFFFFFFF; ++i)
    {
        float a = std::bit_cast<float>(i);
        for (float b : testValues)
        {
            float expected = a + b;
            float result = std::bit_cast<float>(fpu.fadd(i, std::bit_cast<uint32_t>(b)));
            float error = std::fabs(result - expected);
            float tolerance = std::max({std::fabs(a) * TOLERANCE_ADD, std::fabs(b) * TOLERANCE_ADD, std::fabs(expected) * TOLERANCE_ADD, EPSILON});
            if (error > tolerance)
            {
                std::cout << "FADD(" << a << ", " << b << ") = " << result << " (expected: " << expected << ", error: " << error << ", tolerance: " << tolerance << ")" << std::endl;
            }
        }
        if (i == 0x10000000 || i == 0x20000000 || i == 0x30000000 || i == 0x40000000 || i == 0x50000000 || i == 0x60000000 || i == 0x70000000 || i == 0x80000000 || i == 0x90000000 || i == 0xA0000000 || i == 0xB0000000 || i == 0xC0000000 || i == 0xD0000000 || i == 0xE0000000 || i == 0xF0000000)
            std::cout << "Testing FADD with i > " << i << "..." << std::endl;
        if (i == 0xFFFFFFFF)
            break;
    }
}

// FSUB シングルループテスト
void singleLoopTestFsub(FPU &fpu)
{
    std::cout << "Single Loop Testing FSUB..." << std::endl;
    for (uint32_t i = 0; i <= 0xFFFFFFFF; ++i)
    {
        float a = std::bit_cast<float>(i);
        for (float b : testValues)
        {
            float expected = a - b;
            float result = std::bit_cast<float>(fpu.fsub(i, std::bit_cast<uint32_t>(b)));
            float error = std::fabs(result - expected);
            float tolerance = std::max({std::fabs(a) * TOLERANCE_ADD, std::fabs(b) * TOLERANCE_ADD, std::fabs(expected) * TOLERANCE_ADD, EPSILON});
            if (error > tolerance)
            {
                std::cout << "FSUB(" << a << ", " << b << ") = " << result << " (expected: " << expected << ", error: " << error << ", tolerance: " << tolerance << ")" << std::endl;
            }
        }
        if (i == 0x10000000 || i == 0x20000000 || i == 0x30000000 || i == 0x40000000 || i == 0x50000000 || i == 0x60000000 || i == 0x70000000 || i == 0x80000000 || i == 0x90000000 || i == 0xA0000000 || i == 0xB0000000 || i == 0xC0000000 || i == 0xD0000000 || i == 0xE0000000 || i == 0xF0000000)
            std::cout << "Testing FSUB with i > " << i << "..." << std::endl;
        if (i == 0xFFFFFFFF)
            break;
    }
}

// FMUL シングルループテスト
void singleLoopTestFmul(FPU &fpu)
{
    std::cout << "Single Loop Testing FMUL..." << std::endl;
    for (uint32_t i = 0; i <= 0xFFFFFFFF; ++i)
    {
        float a = std::bit_cast<float>(i);
        for (float b : testValues)
        {
            float expected = a * b;
            float result = std::bit_cast<float>(fpu.fmul(i, std::bit_cast<uint32_t>(b)));
            float error = std::fabs(result - expected);
            float tolerance = std::max(std::fabs(expected) * TOLERANCE_MUL, EPSILON);
            if (error > tolerance)
            {
                std::cout << "FMUL(" << a << ", " << b << ") = " << result << " (expected: " << expected << ", error: " << error << ", tolerance: " << tolerance << ")" << std::endl;
            }
        }
        if (i == 0x10000000 || i == 0x20000000 || i == 0x30000000 || i == 0x40000000 || i == 0x50000000 || i == 0x60000000 || i == 0x70000000 || i == 0x80000000 || i == 0x90000000 || i == 0xA0000000 || i == 0xB0000000 || i == 0xC0000000 || i == 0xD0000000 || i == 0xE0000000 || i == 0xF0000000)
        {
            std::cout << "Testing FMUL with i > " << i << "..." << std::endl;
        }
        if (i == 0xFFFFFFFF)
        {
            return;
        }
    }
}

// FDIV シングルループテスト
void singleLoopTestFdiv(FPU &fpu)
{
    std::cout << "Single Loop Testing FDIV..." << std::endl;
    for (uint32_t i = 0; i <= 0xFFFFFFFF; ++i)
    {
        float a = std::bit_cast<float>(i);
        for (float b : testValues)
        {
            if (b == 0.0f)
                continue; // Division by zero check
            float expected = a / b;
            float result = std::bit_cast<float>(fpu.fdiv(i, std::bit_cast<uint32_t>(b)));
            float error = std::fabs(result - expected);
            float tolerance = std::max(std::fabs(expected) * TOLERANCE_DIV, EPSILON);
            if (error > tolerance)
            {
                std::cout << "FDIV(" << a << ", " << b << ") = " << result << " (expected: " << expected << ", error: " << error << ", tolerance: " << tolerance << ")" << std::endl;
            }
        }
        if (i == 0x10000000 || i == 0x20000000 || i == 0x30000000 || i == 0x40000000 || i == 0x50000000 || i == 0x60000000 || i == 0x70000000 || i == 0x80000000 || i == 0x90000000 || i == 0xA0000000 || i == 0xB0000000 || i == 0xC0000000 || i == 0xD0000000 || i == 0xE0000000 || i == 0xF0000000)
            std::cout << "Testing FDIV with i > " << i << "..." << std::endl;
        if (i == 0xFFFFFFFF)
            break;
    }
}

// FSQRT シングルループテスト
void singleLoopTestFsqrt(FPU &fpu)
{
    std::cout << "Single Loop Testing FSQRT..." << std::endl;
    for (uint32_t i = 0; i <= 0xFFFFFFFF; ++i)
    {
        float a = std::bit_cast<float>(i);
        if (a < 0.0f)
            continue; // Skip negative inputs for sqrt
        float expected = std::sqrt(a);
        float result = std::bit_cast<float>(fpu.fsqrt(i));
        float error = std::fabs(result - expected);
        float tolerance = std::max(std::fabs(expected) * TOLERANCE_SQRT, EPSILON);
        if (error > tolerance)
        {
            std::cout << "FSQRT(" << a << ") = " << result << " (expected: " << expected << ", error: " << error << ", tolerance: " << tolerance << ")" << std::endl;
        }
        if (i == 0x10000000 || i == 0x20000000 || i == 0x30000000 || i == 0x40000000 || i == 0x50000000 || i == 0x60000000 || i == 0x70000000 || i == 0x80000000 || i == 0x90000000 || i == 0xA0000000 || i == 0xB0000000 || i == 0xC0000000 || i == 0xD0000000 || i == 0xE0000000 || i == 0xF0000000)
            std::cout << "Testing FSQRT with i > " << i << "..." << std::endl;
        if (i == 0xFFFFFFFF)
            break;
    }
}

// コーナーケース用のテスト関数
void cornerCaseTestFPU(FPU &fpu)
{
    std::cout << "Testing corner cases..." << std::endl;

    struct
    {
        float input1;
        float input2;
    } cornerCases[] = {
        {0.0f, 0.0f},
        {std::numeric_limits<float>::infinity(), 1.0f},
        {1.0f, -1.0f},
        {-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()},
        {3.4028235e+38, 1.0f},  // 最大正数
        {-3.4028235e+38, 1.0f}, // 最大負数
        {1.0f, 0.0f},           // 1 と 0 の組み合わせ
        {-1.0f, 0.0f},          // -1 と 0 の組み合わせ
    };

    for (const auto &testCase : cornerCases)
    {
        uint32_t f1 = std::bit_cast<uint32_t>(testCase.input1);
        uint32_t f2 = std::bit_cast<uint32_t>(testCase.input2);

        std::cout << "fadd(" << testCase.input1 << ", " << testCase.input2 << ") = "
                  << std::bit_cast<float>(fpu.fadd(f1, f2)) << " expected: "
                  << (testCase.input1 + testCase.input2) << std::endl;

        std::cout << "fsub(" << testCase.input1 << ", " << testCase.input2 << ") = "
                  << std::bit_cast<float>(fpu.fsub(f1, f2)) << " expected: "
                  << (testCase.input1 - testCase.input2) << std::endl;

        std::cout << "fmul(" << testCase.input1 << ", " << testCase.input2 << ") = "
                  << std::bit_cast<float>(fpu.fmul(f1, f2)) << " expected: "
                  << (testCase.input1 * testCase.input2) << std::endl;

        if (testCase.input2 != 0)
        { // Division by zero check
            std::cout << "fdiv(" << testCase.input1 << ", " << testCase.input2 << ") = "
                      << std::bit_cast<float>(fpu.fdiv(f1, f2)) << " expected: "
                      << (testCase.input1 / testCase.input2) << std::endl;
        }
    }
}

// 全探索用のテスト関数
void fullTestFPU(FPU &fpu)
{
    std::cout << "Testing full range..." << std::endl;

    for (uint32_t i = 0; i <= 0xFFFFFFFF; i += 0x00000001)
    {
        float f = std::bit_cast<float>(i);
        int32_t expectedInt = std::round(f);

        int32_t resultInt = std::bit_cast<int32_t>(fpu.ftoi(i));
        if (expectedInt != resultInt && expectedInt != INT32_MIN && expectedInt != INT32_MAX)
        {
            std::cout << "ftoi(" << std::hex << "0x" << i << ") = " << std::dec << resultInt
                      << " (expected: " << expectedInt << ")" << std::endl;
        }

        // itof
        uint32_t expectedFloatBits = std::bit_cast<uint32_t>(static_cast<float>(expectedInt));
        uint32_t resultFloatBits = fpu.itof(expectedInt);
        if (expectedFloatBits != resultFloatBits && expectedInt != INT32_MIN && expectedInt != INT32_MAX)
        {
            std::cout << "itof(" << expectedInt << ") = " << std::hex << "0x" << resultFloatBits
                      << " (expected: 0x" << expectedFloatBits << ")" << std::endl;
        }
        if (i == 0xFFFFFFFF)
        {
            break;
        }
    }
}
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
        int32_t result = std::bit_cast<int32_t>(fpu.ftoi(testCase.floatBits));
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
                  << " (expected: 0x" << testCase.expectedFloatBits << ")" << std::dec << std::endl;
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
             int32_t result =
                 std::bit_cast<int32_t>(fpu.ftoi(std::bit_cast<uint32_t>(input1)));

             std::cout << "ftoi (" << input1 << ") = " << result << std::endl;
         }},
        {"itof", [&]
         {
             std::cin >> iinput;
             float result =
                 std::bit_cast<float>(fpu.itof(std::bit_cast<uint32_t>(iinput)));

             std::cout << "itof (" << iinput << ") = " << result << std::endl;
         }},
        {"ffloor", [&]
         {
             std::cin >> input1;
             float result =
                 std::bit_cast<float>(fpu.ffloor(std::bit_cast<uint32_t>(input1)));

             std::cout << "ffloor (" << input1 << ") = " << result << std::endl;
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
    cornerCaseTestFPU(fpu);
    fullTestFPU(fpu);
    // singleLoopTestFadd(fpu);
    // singleLoopTestFsub(fpu);
    singleLoopTestFmul(fpu);
    // singleLoopTestFdiv(fpu);
    // singleLoopTestFsqrt(fpu);
    std::cout << "end!" << std::endl;
    testFPU(fpu);
    return 0;
}