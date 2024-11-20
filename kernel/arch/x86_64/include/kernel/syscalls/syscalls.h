#include <isr.h>

 #include <types.h>
#include <vfs.h>

#ifndef SYSCALLS_H
#define SYSCALLS_H

#define MAX_SYSCALLS 450

/* Ignore everything! */

/* Syscall Debugging: None */
#define NO_DEBUG_SYSCALLS 1

#if !NO_DEBUG_SYSCALLS
/* Syscall Debugging: Comprehensive */
#define DEBUG_SYSCALLS_STRACE 1
#define DEBUG_SYSCALLS_EXTRA 0

/* Syscall Debugging: Important */
#define DEBUG_SYSCALLS_FAILS 1
#define DEBUG_SYSCALLS_STUB 1

/* Syscall Debugging: Essential */
#define DEBUG_SYSCALLS_MISSING 1
#endif

void syscall_handler(AsmPassedInterrupt *regs);
void syscalls_initialize();

/* System call registration */
void syscallRegFs();
void syscallRegMem();
void syscallRegSig();
void syscallsRegEnv();
void syscallsRegProc();
void syscallsRegClock();

void register_syscall(uint32_t id, void *handler); // <- the master

/* Standard output handlers (io.c) */
int read_handler(OpenFile *fd, uint8_t *in, size_t limit);
int write_handler(OpenFile *fd, uint8_t *out, size_t limit);
int ioctl_handler(OpenFile *fd, uint64_t request, void *arg);

size_t memory_map_handler(size_t addr, size_t length, int prot, int flags,
                   OpenFile *fd, size_t pgoffset);

// /* Defined in io.c */
// extern VfsHandlers stdio;

// /* Unix pipe() (defined in pipe.c) */
extern VfsHandlers pipeReadEnd;
extern VfsHandlers pipeWriteEnd;

bool pipe_close_end(OpenFile *readFd);
int  pipe_open(int *fds);

#endif
