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
        DEBUG = 1 << 0,
        GDB = 1 << 1,
        I = 1 << 2,
        OUTPUTREG = 1 << 3,
        LIMIT = 1 << 4,
        CACHE = 1 << 5,
        MEMORY = 1 << 6,
        ICOUNT = 1 << 7,
        NOTIFY = 1 << 8,
        All = 0xFFFF,             
    };
    void on(uint32_t flag) noexcept;
    void off(uint32_t flag) noexcept;
    bool is(uint32_t flag) const noexcept;
    bool has(uint32_t flag) const noexcept;
};

#endif