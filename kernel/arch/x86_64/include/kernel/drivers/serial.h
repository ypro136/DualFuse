#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" bool serial_initialize(uint16_t _port);
bool received();
char read();
bool transmit_empty();
void serial_write(char a);



 
#endif