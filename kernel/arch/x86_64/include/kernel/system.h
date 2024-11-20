#include <stdint.h>

#ifndef SYSTEM_H
#define SYSTEM_H


#define SYSCALL_TEST 0
#define SYSCALL_EXIT_TASK 1
#define SYSCALL_FORK 2
#define SYSCALL_READ 3
#define SYSCALL_WRITE 4

#define SYSCALL_GETPID 20

#define SYSCALL_GETARGC 400
#define SYSCALL_GETARGV 401
#define SYSCALL_GET_HEAP_START 402
#define SYSCALL_GET_HEAP_END 403
#define SYSCALL_ADJUST_HEAP_END 404

extern bool systemDiskInit;

void     syscallTest(char *msg);
void     syscallExitTask(int return_code);
uint32_t syscallFork();
void     syscallRead(int file, char *str, uint32_t count);
void     syscallWrite(int file, char *str, uint32_t count);

uint32_t syscallGetPid();

int      syscallGetArgc();
char    *syscallGetArgv(int curr);
uint32_t syscallGetHeapStart();
uint32_t syscallGetHeapEnd();
void     syscallAdjustHeapEnd(uint32_t heap_end);

void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
uint64_t rdmsr(uint32_t msrid);
uint64_t wrmsr(uint32_t msrid, uint64_t value);


#endif