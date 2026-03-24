#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


extern "C" bool serial_initialize(uint16_t _port);
bool received();
char read();
bool transmit_empty();
void serial_write(char a);

void serial_print(const char* str);
void serial_send_line(const char* str);  // adds \r\n — matches Arduino println
int  serial_available();
char serial_read_byte();
void serial_poll();                      // MSI fallback
void serial_on_message(const char* msg); // weak — override to handle full lines



 
#endif