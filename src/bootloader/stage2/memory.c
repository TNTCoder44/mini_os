#include "memory.h"

int memcpy(void far *dst, const void far *src, uint16_t num)
{
    uint8_t far *u8Dst = (uint8_t far *)dst;
    const uint8_t far *u8Src = (const uint8_t far *)src;

    for (uint16_t i = 0; i < num; i++)
    {
        u8Dst[i] = u8Src[i];
    }
}
