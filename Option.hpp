#ifndef OPTION_HPP
#define OPTION_HPP
#include <cstdint>

class Options
{
private:
    uint32_t m_flag;

public:
    Options();
    enum MODE
    {
        ONLYSTDIO = 1 << 0, // 00000001
        GDB = 1 << 1,       // 00000010
        I = 1 << 2,         // 00000100
        REG = 1 << 3,         // 00001000
        LIMIT = 1 << 4,         // 00001000
        All = 0xFF,         // 11111111
    };
    void on(uint32_t flag) noexcept;
    void off(uint32_t flag) noexcept;
    bool is(uint32_t flag) const noexcept;
    bool has(uint32_t flag) const noexcept;
};

#endif