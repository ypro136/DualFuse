 #include <types.h>

#ifndef FAST_SYSCALL_H
#define FAST_SYSCALL_H

void syscall_inst_initialize();

extern "C" void syscall_entry();

#endif