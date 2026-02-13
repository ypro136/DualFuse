#include <isr.h>

 #include <types.h>
#include <vfs.h>

#ifndef SYSCALLS_H
#define SYSCALLS_H

#define MAX_SYSCALLS 450

/* Ignore everything! */

/* Debugf helpers */
#if DEBUG_SYSCALLS_FAILS
int dbgSysFailf(const char *format, ...);
#else
#define dbgSysFailf(...) ((void)0)
#endif

#if DEBUG_SYSCALLS_EXTRA
int dbgSysExtraf(const char *format, ...);
#else
#define dbgSysExtraf(...) ((void)0)
#endif

#if DEBUG_SYSCALLS_STUB
int dbgSysStubf(const char *format, ...);
#else
#define dbgSysStubf(...) ((void)0)
#endif

/* Fast userspace locks (defined in futex.c) */
size_t futexSyscall(uint32_t *addr, int op, uint32_t value,
                    struct timespec *utime, uint32_t *addr2, uint32_t value3);

#define RET_IS_ERR(syscall_return) ((syscall_return) > -4096UL)


void syscall_handler(AsmPassedInterrupt *regs);
void syscalls_initialize();

/* System call registration */
void syscallRegFs();
void syscallRegMem();
void syscallRegSig();
void syscallsRegEnv();
void syscallsRegProc();
void syscallsRegClock();

/* Signal stuff */
typedef enum SignalInternal {
  SIGNAL_INTERNAL_CORE = 0,
  SIGNAL_INTERNAL_TERM,
  SIGNAL_INTERNAL_IGN,
  SIGNAL_INTERNAL_STOP,
  SIGNAL_INTERNAL_CONT
} SignalInternal;

extern SignalInternal signalInternalDecisions[65];
void   initiateSignalDefs();
void   signalsPendingHandleSys(void *taskPtr, uint64_t *rsp,
                               AsmPassedInterrupt *registers);
void   signalsPendingHandleSched(void *taskPtr);
size_t signalsSigreturnSyscall(void *taskPtr);
bool   signalsPendingQuick(void *taskPtr);
bool   signalsRevivableState(int state);

void registerSyscall(uint32_t id, void *handler); // <- the master

char *atResolvePathname(int dirfd, char *pathname);
void  atResolvePathnameCleanup(char *pathname, char *resolved);

size_t eventFdOpen(uint64_t initValue, int flags);

/* Event FDs (defined in eventfd.c) */
extern VfsHandlers eventFdHandlers;


size_t syscallGetPid();


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

#if DEBUG_SIGNALS_HITS
#define dbgSigHitf printf
#else
#define dbgSigHitf(...) ((void)0)
#endif
#if DEBUG_SIGNALS_STUB
#define dbgSigStubf printf
#else
#define dbgSigStubf(...) ((void)0)
#endif

/* Cmon now it's really useful! */
size_t syscallRtSigprocmask(int how, sigset_t *nset, sigset_t *oset,
                            size_t sigsetsize);

#endif
