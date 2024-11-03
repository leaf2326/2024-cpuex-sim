#include "Option.hpp"

Options::Options() :m_flag(0) {}

void Options::set(uint32_t flag)
{
    m_flag |= flag;
}
bool Options::is(uint32_t flag) const
{
    return (m_flag == flag);
}
bool Options::has(uint32_t flag) const
{
    return (m_flag & flag) != 0;
}