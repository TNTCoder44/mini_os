#include <stdint.h>
#include "stdio.h"
#include "memory.h"

#include "donut.h"

extern uint8_t __bss_start;
extern uint8_t __end;

#define NULL (void*)0

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
    memset(&__bss_start, 0, (&__end) - (&__bss_start));
    clrscr();

    // donut();
    printf("Hello from the kernel!!");

end:
    for (;;)
        ;
}
