#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive)
{
    puts("Hello world from C!");
    printf("Formatted %% %c %s", 'a', "string");

    for (;;)
        ;
}