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
        B = 1 << 1,         // 00000010
        I = 1 << 2,         // 00000100
        All = 0xFF,         // 11111111
    };
    void set(uint32_t flag);
    bool is(uint32_t flag) const;
    bool has(uint32_t flag) const;
};

#endif