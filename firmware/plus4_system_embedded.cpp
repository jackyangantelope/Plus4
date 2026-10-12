
#include <cstdint>

#include "system.hpp"

namespace Plus4Emu {

uint32_t Timer::getRandomSeedFromTime()
{
    return 0x504C5553u;
}

int getRandomNumber(int& seedValue)
{
    const int64_t tmp = int32_t(seedValue) * int64_t(742938285);
    uint32_t tmp2 = uint32_t(tmp & int64_t(0x7FFFFFFF)) + uint32_t(tmp >> 31);
    if (tmp2 >= 0x80000000u) tmp2 -= 0x7FFFFFFFu;
    seedValue = int(tmp2);
    return seedValue;
}

void setRandomSeed(int& seedValue, uint32_t value)
{
    while (value >= 0x7FFFFFFFu) value -= 0x7FFFFFFEu;
    if (value == 0u) value = 0x7FFFFFFEu;
    seedValue = int(value);
    (void)getRandomNumber(seedValue);
}

}
