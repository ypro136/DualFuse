#ifndef _KERNEL_SERIAL_IO_H
#define _KERNEL_SERIAL_IO_H

#include <stddef.h>
#include <serial.h>


void outb(unsigned short port, unsigned char val);

unsigned char inb(unsigned short port);

#endif
