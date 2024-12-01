#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"

#include "donut.h"
#include "stdio.h"
#include <stdint.h>
#include "string.h"

int8_t b[1760], z[1760];

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void delay_us(unsigned int microseconds) {
    uint64_t start_time = rdtsc();
    uint64_t frequency = 1000000; // Passen Sie die CPU-Taktfrequenz hier an
    uint64_t wait_ticks = (microseconds * frequency) / 1000000;
    while ((rdtsc() - start_time) < wait_ticks);
}

void donut()
{
    int sA=1024,cA=0,sB=1024,cB=0,_;
    for (;;) 
    {
        memset(b, 32, 1760);  // text buffer
        memset(z, 127, 1760);   // z buffer
        int sj=0, cj=1024;
        for (int j = 0; j < 90; j++) 
        {
            int si = 0, ci = 1024;  // sine and cosine of angle i
            for (int i = 0; i < 324; i++) 
            {
                int R1 = 1, R2 = 2048, K2 = 5120*1024;

                int x0 = R1*cj + R2,
                    x1 = ci*x0 >> 10,
                    x2 = cA*sj >> 10,
                    x3 = si*x0 >> 10,
                    x4 = R1*x2 - (sA*x3 >> 10),
                    x5 = sA*sj >> 10,
                    x6 = K2 + R1*1024*x5 + cA*x3,
                    x7 = cj*si >> 10,
                    x = 40 + 30*(cB*x1 - sB*x4)/x6,
                    y = 12 + 15*(cB*x4 + sB*x1)/x6,
                    N = (-cA*x7 - cB*((-sA*x7>>10) + x2) - ci*(cj*sB >> 10) >> 10) - x5 >> 7;

                int o = x + 80 * y;
                int8_t zz = (x6-K2)>>15;
                if (22 > y && y > 0 && x > 0 && 80 > x && zz < z[o]) 
                {
                    z[o] = zz;
                    b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
                }
                R(5, 8, ci, si)  // rotate i
            }
            R(9, 7, cj, sj)  // rotate j
        }
        for (int k = 0; 1761 > k; k++)
            putc(k % 80 ? b[k] : 10);
        R(5, 7, cA, sA);
        R(5, 8, cB, sB);
        delay_us(15000 * 1000);
        printf("\x1b[23A");

        for (;;)
            ;
    }
}

#pragma GCC diagnostic warning "-Wall"
