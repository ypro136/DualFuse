#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// class Serial
// {
// private:
//     uint32_t storage;
// public:
//     Serial(uint32_t number);
//     ~Serial();

//     uint32_t get();
// };

// Serial::Serial(uint32_t number)
// {
//     storage = number;
// }

// Serial::~Serial()
// {
// }

// uint32_t Serial::get()
// {
//     return storage;
// }



extern "C" bool serial_initialize(uint16_t _port);
bool received();
char read();
bool transmit_empty();
void serial_write(char a);



 
#endif