#include <syscalls.h>

#include <linux.h>
#include <string.h>
#include <system.h>
#include <task.h>

#include <utility.h>
#include <hcf.hpp>

#define SYSCALL_GETPID 39
static uint32_t syscallGetPid() { return currentTask->id; }

#define SYSCALL_GETCWD 79
static int syscallGetcwd(char *buff, size_t size) {
  size_t realLength = strlen(currentTask->cwd) + 1;
  if (size < realLength) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::getcwd] FAIL! Not enough space on buffer! given{%ld} "
           "required{%ld}\n",
           size, realLength);
#endif
    return -ERANGE;
  }
  memcpy(buff, currentTask->cwd, realLength);

  return realLength;
}

// Beware the 65 character limit!
char sysname[] = "Cave-Like Operating System";
char nodename[] = "cavOS";
char release[] = "0.69.2";
char version[] = "0.69.2";
char machine[] = "x86_64";

#define SYSCALL_UNAME 63
static int syscallUname(struct old_utsname *utsname) {
  memcpy(utsname->sysname, sysname, sizeof(sysname));
  memcpy(utsname->nodename, nodename, sizeof(nodename));
  memcpy(utsname->release, release, sizeof(release));
  memcpy(utsname->version, version, sizeof(version));
  memcpy(utsname->machine, machine, sizeof(machine));

  return 0;
}

#define SYSCALL_CHDIR 80
static int syscallChdir(char *newdir) {
  int ret = task_change_cwd(newdir);
#if DEBUG_SYSCALLS_FAILS
  if (ret < 0)
    printf("[syscalls::chdir] FAIL! Tried to change to %s!\n", newdir);
#endif

  return ret;
}

#define SYSCALL_FCHDIR 81
static int syscallFchdir(int fd) {
  OpenFile *file = file_system_user_get_node(currentTask, fd);
  if (!file)
    return -EBADF;
  if (!file->dirname)
    return -ENOTDIR;
  return syscallChdir(file->dirname);
}

#define SYSCALL_GETUID 102
static int syscallGetuid() {
  return 0; // root ;)
}

#define SYSCALL_GETGID 104
static int syscallGetgid() {
  return 0; // root ;)
}

#define SYSCALL_GETEUID 107
static int syscallGeteuid() {
  return 0; // root ;)
}

#define SYSCALL_GETEGID 108
static int syscallGetegid() {
  return 0; // root ;)
}

#define SYSCALL_SETPGID 109
static int syscallSetpgid(int pid, int pgid) {
  if (!pid)
    pid = currentTask->id;

  Task *task = task_get(pid);
  if (!task) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::setpgid] FAIL! Couldn't find task. pid{%d}\n", pid);
#endif
    return -EPERM;
  }

  task->pgid = pgid;
  return 0;
}

#define SYSCALL_GETPPID 110
static int syscallGetppid() {
  if (currentTask->parent)
    return currentTask->parent->id;
  else
    return KERNEL_TASK_ID;
}

#define SYSCALL_GETGROUPS 115
static int syscallGetgroups(int gidsetsize, uint32_t *gids) {
  if (!gidsetsize)
    return 1;

  gids[0] = 0;
  return 1;
}

#define SYSCALL_GETPGID 121
static int syscallGetpgid() { return currentTask->pgid; }

#define SYSCALL_PRCTL 158
static int syscallPrctl(int code, size_t addr) {
  switch (code) {
  case 0x1002:
    currentTask->fsbase = addr;
    wrmsr(MSRID_FSBASE, currentTask->fsbase);

    return 0;
    break;
  }

  printf("[syscalls::prctl] Unsupported code! code_10{%d} code_16{%x}!\n", code,
         code);
  return -ENOSYS;
}

#define SYSCALL_GET_TID 186
static int syscallGetTid() { return currentTask->id; }

#define SYSCALL_SET_TID_ADDR 218
static int syscallSetTidAddr(int *tidptr) {
  *tidptr = currentTask->id;
  return currentTask->id;
}

void syscallsRegEnv() {
  register_syscall(SYSCALL_GETPID, syscallGetPid);
  register_syscall(SYSCALL_GETCWD, syscallGetcwd);
  register_syscall(SYSCALL_CHDIR, syscallChdir);
  register_syscall(SYSCALL_GETUID, syscallGetuid);
  register_syscall(SYSCALL_GETEUID, syscallGeteuid);
  register_syscall(SYSCALL_GETGID, syscallGetgid);
  register_syscall(SYSCALL_GETEGID, syscallGetegid);
  register_syscall(SYSCALL_GETPPID, syscallGetppid);
  register_syscall(SYSCALL_GETPGID, syscallGetpgid);
  register_syscall(SYSCALL_SETPGID, syscallSetpgid);
  register_syscall(SYSCALL_PRCTL, syscallPrctl);
  register_syscall(SYSCALL_SET_TID_ADDR, syscallSetTidAddr);
  register_syscall(SYSCALL_GET_TID, syscallGetTid);
  register_syscall(SYSCALL_UNAME, syscallUname);
  register_syscall(SYSCALL_FCHDIR, syscallFchdir);
  register_syscall(SYSCALL_GETGROUPS, syscallGetgroups);
}
