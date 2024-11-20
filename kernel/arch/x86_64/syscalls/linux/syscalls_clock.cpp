#include <linux.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <timer.h>

// #define SYSCALL_NANOSLEEP 35
// static int syscallNanosleep() {
//   sleep(1);
//   return -1;
// }

#define SYSCALL_CLOCK_GETTIME 228
static int syscallClockGettime(int which, timespec *spec) {
  switch (which) {
  case CLOCK_REALTIME: {
    spec->tv_sec = timerBootUnix + timerTicks / 1000;
    size_t remainingInMs = timerTicks - (spec->tv_sec * 1000);
    spec->tv_nsec = remainingInMs * 1000000;
    // todo: accurancy
    return 0;
    break;
  }
  case CLOCK_MONOTONIC: {
    // todo
    return syscallClockGettime(CLOCK_REALTIME, spec);
    break;
  }
  default:
#if DEBUG_SYSCALLS_STUB
    printf("[syscalls::gettime] UNIMPLEMENTED! which{%d} timespec{%lx}!\n", which, spec);
#endif
    return -1;
    break;
  }
}

void syscallsRegClock() {
  // register_syscall(SYSCALL_NANOSLEEP, syscallNanosleep);
  register_syscall(SYSCALL_CLOCK_GETTIME, syscallClockGettime);
}
