#pragma once

#include <stdio.h>

static void Halt(void)
{
    printf("Halt!!\n");
    for (;;) {
        asm ("hlt");
    }
}