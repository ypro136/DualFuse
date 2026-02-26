#include <syscalls.h>

#include <console.h>
#include <framebufferutil.h>
#include <gdt.h>
#include <isr.h>
#include <keyboard.h>
//#include <schedule.h>
#include <serial.h>
#include <string.h>
#include <system.h>
#include <task.h>
#include <utility.h>
#include <hcf.hpp>


#if DEBUG_SYSCALLS_STRACE
#include <linux_syscalls.h>
#endif

SignalInternal signalInternalDecisions[65] = {0};

size_t   syscalls[MAX_SYSCALLS] = {0};
uint32_t syscallCnt = 0;


#if DEBUG_SYSCALLS_EXTRA
int dbgSysExtraf(const char *format, ...) {
  printf(" %s//%s ", ANSI_BLUE, ANSI_RESET);
  spinlock_acquire(&LOCK_DEBUGF);
  va_list va;
  va_start(va, format);
  int ret = vfctprintf(debug, 0, format, va);
  va_end(va);
  spinlock_release(&LOCK_DEBUGF);
  return ret;
}
#endif

#if DEBUG_SYSCALLS_STUB
int dbgSysStubf(const char *format, ...) {
  printf(" %s//%s ", ANSI_BLACK, ANSI_RESET);
  spinlock_acquire(&LOCK_DEBUGF);
  va_list va;
  va_start(va, format);
  int ret = vfctprintf(debug, 0, format, va);
  va_end(va);
  spinlock_release(&LOCK_DEBUGF);
  return ret;
}
#endif


// fast function to tell if a signal is pending (for constant-pull syscalls) TODO:move to signal.cpp
bool signalsPendingQuick(void *taskPtr) {
  Task *task = (Task *)taskPtr;
  if (task->kernel_task)
    return false;
  sigset_t pendingList = atomicBitmapGet(&task->sigPendingList);
  sigset_t unblockedList = pendingList & ~task->sigBlockList;
  for (int i = 0; i < _NSIG; i++) {
    // todo: do this EVERYWHERE (1<<i) won't work above 32
    if (!bitmapGenericGet((uint8_t *)&unblockedList, i))
      continue;
    struct sigaction *action = &task->infoSignals->signals[i];
    __sighandler_t    userHandler =
        (__sighandler_t)atomicRead64((size_t *)(size_t)&action->sa_handler);
    if (userHandler == SIG_IGN)
      continue;
    if (userHandler == SIG_DFL &&
        signalInternalDecisions[i] == SIGNAL_INTERNAL_IGN)
      continue;
    // otherwise, we passed!
    return true;
  }
  return false;
}

// stack is laid out like this afterwards
// * uint64_t retaddr;
// * struct sigcontext ucontext;
// * struct siginfo info; (optional)
// * struct fpstate fp;

void initiateSignalDefs() {
  signalInternalDecisions[SIGABRT] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGALRM] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGBUS] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGCHLD] = SIGNAL_INTERNAL_IGN;
  // signalInternalDecisions[SIGCLD] = SIGNAL_INTERNAL_IGN;
  signalInternalDecisions[SIGCONT] = SIGNAL_INTERNAL_CONT;
  // signalInternalDecisions[SIGEMT] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGFPE] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGHUP] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGILL] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGINT] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGIO] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGIOT] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGKILL] = SIGNAL_INTERNAL_TERM;
  // signalInternalDecisions[SIGLOST] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGPIPE] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGPOLL] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGPROF] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGPWR] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGQUIT] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGSEGV] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGSTKFLT] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGSTOP] = SIGNAL_INTERNAL_STOP;
  signalInternalDecisions[SIGTSTP] = SIGNAL_INTERNAL_STOP;
  signalInternalDecisions[SIGSYS] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGTERM] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGTRAP] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGTTIN] = SIGNAL_INTERNAL_STOP;
  signalInternalDecisions[SIGTTOU] = SIGNAL_INTERNAL_STOP;
  signalInternalDecisions[SIGUNUSED] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGURG] = SIGNAL_INTERNAL_IGN;
  signalInternalDecisions[SIGUSR1] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGUSR2] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGVTALRM] = SIGNAL_INTERNAL_TERM;
  signalInternalDecisions[SIGXCPU] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGXFSZ] = SIGNAL_INTERNAL_CORE;
  signalInternalDecisions[SIGWINCH] = SIGNAL_INTERNAL_IGN;
}


void register_syscall(uint32_t id, void *handler) {
  if (id > MAX_SYSCALLS) {
    printf("[syscalls] FATAL! Exceded limit! limit{%d} id{%d}\n", MAX_SYSCALLS,
           id);
    Halt();
  }

  if (syscalls[id]) {
    printf("[syscalls] FATAL! id{%d} found duplicate!\n", id);
    Halt();
  }

  syscalls[id] = (size_t)handler;
  syscallCnt++;
}

void registerSyscall(uint32_t id, void *handler) {
  if (id > MAX_SYSCALLS) {
    printf("[syscalls] FATAL! Exceded limit! limit{%d} id{%d}\n", MAX_SYSCALLS,
           id);
    Halt();
  }

  if (syscalls[id]) {
    printf("[syscalls] FATAL! id{%d} found duplicate!\n", id);
    Halt();
  }

  syscalls[id] = (size_t)handler;
  syscallCnt++;
}

typedef uint64_t (*SyscallHandler)(uint64_t a1, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6);
extern "C" void syscall_handler(AsmPassedInterrupt *regs) {
  uint64_t *rspPtr = (uint64_t *)((size_t)regs + sizeof(AsmPassedInterrupt));
  uint64_t  rsp = *rspPtr;

  currentTask->systemCallInProgress = true;
  currentTask->syscallRegs = regs;
  currentTask->syscallRsp = rsp;

  asm volatile("sti"); // do other task stuff while we're here!

  uint64_t id = regs->rax;
  if (id > MAX_SYSCALLS) {
    regs->rax = -1;
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls] FAIL! Tried to access syscall{%d} (out of bounds)!\n",
           id);
#endif
    currentTask->syscallRsp = 0;
    currentTask->syscallRegs = 0;
    currentTask->systemCallInProgress = false;
  }
  size_t handler = syscalls[id];

#if DEBUG_SYSCALLS_STRACE
  // printf("[syscalls] id{%d} handler{%lx}\n", id, handler);

  bool usable = id < (sizeof(linux_syscalls) / sizeof(linux_syscalls[0]));
  const LINUX_SYSCALL *info = &linux_syscalls[id];

  if (!handler)
    printf("\033[0;31m");
  printf("%d [syscalls] %s( ", currentTask->id, usable ? info->name : "???");
  if (usable) {
    if (info->rdi[0])
      printf("\b%s:%lx ", info->rdi, regs->rdi);
    if (info->rsi[0])
      printf("%s:%lx ", info->rsi, regs->rsi);
    if (info->rdx[0])
      printf("%s:%lx ", info->rdx, regs->rdx);
    if (info->r10[0])
      printf("%s:%lx ", info->r10, regs->r10);
    if (info->r8[0])
      printf("%s:%lx ", info->r8, regs->r8);
    if (info->r9[0])
      printf("%s:%lx ", info->r9, regs->r9);
  }
  printf("\b)");
  if (!handler)
    printf("\033[0m\n");
#endif

  if (!handler) {
    regs->rax = -ENOSYS;
#if DEBUG_SYSCALLS_MISSING
    printf("[syscalls] Tried to access syscall{%d} (doesn't exist)!\n", id);
#endif
    currentTask->syscallRsp = 0;
    currentTask->syscallRegs = 0;
    currentTask->systemCallInProgress = false;
  }

  long int ret = ((SyscallHandler)(handler))(regs->rdi, regs->rsi, regs->rdx,
                                             regs->r10, regs->r8, regs->r9);
#if DEBUG_SYSCALLS_STRACE
  printf(" = %d\n", ret);
#endif

  regs->rax = ret;

//cleanup
  currentTask->syscallRsp = 0;
  currentTask->syscallRegs = 0;
  currentTask->systemCallInProgress = false;
}

// System calls themselves

void syscalls_initialize() {
  /*
   * Linux system call ranges
   * Handlers are defined in every file of linux/
   */

  // Filesystem operations
  syscallRegFs();

  // Memory management
  syscallRegMem();

  // POSIX signals
  syscallRegSig();

  // Task/Process environment
  syscallsRegEnv();

  // Task/Process management
  syscallsRegProc();

  // Time/Date/Clocks!
  syscallsRegClock();

  printf("[syscalls] System calls are ready to fire: %d/%d\n", syscallCnt,
         MAX_SYSCALLS);
}
