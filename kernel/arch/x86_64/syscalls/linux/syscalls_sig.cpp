#include <linux.h>
#include <syscalls.h>

#define SYSCALL_RT_SIGACTION 13
static int syscallRtSigaction(int sig, const struct sigaction *act,
                              struct sigaction *oact, size_t sigsetsize) {
#if DEBUG_SYSCALLS_STUB
  printf("[syscalls::sigaction] UNIMPLEMENTED!\n");
#endif
  return -ENOSYS;
}

#define SYSCALL_RT_SIGPROCMASK 14
static int syscallRtSigprocmask(int how, __sigset_t *nset, __sigset_t *oset,
                                size_t sigsetsize) {
#if DEBUG_SYSCALLS_STUB
  printf("[syscalls::sigprocmask] UNIMPLEMENTED!\n");
#endif
  return -ENOSYS;
}

void syscallRegSig() {
  register_syscall(SYSCALL_RT_SIGACTION, syscallRtSigaction);
  register_syscall(SYSCALL_RT_SIGPROCMASK, syscallRtSigprocmask);
}