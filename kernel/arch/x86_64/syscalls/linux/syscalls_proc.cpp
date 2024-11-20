#include <elf.h>
#include <linked_list.h>
#include <linux.h>
#include <liballoc.h>
#include <string.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <utility.h>
#include <hcf.hpp>

// process lifetime system calls (send help)

#define SYSCALL_PIPE 22
static int syscallPipe(int *fds) { return pipe_open(fds); }

#define SYSCALL_PIPE2 293
static int syscallPipe2(int *fds, int flags) {
  if (flags && flags != O_CLOEXEC) {
    printf("[syscalls::pipe2] FAILING! Unimplemented flags! %x\n", flags);
    return -ENOSYS;
  }

  int out = pipe_open(fds);
  if (out < 0)
    goto cleanup;

  if (flags) {
    // since basically the only one we support atm is the close-on-exec flag xd
    OpenFile *fd0 = file_system_user_get_node(currentTask, fds[0]);
    OpenFile *fd1 = file_system_user_get_node(currentTask, fds[1]);

    if (!fd0 || !fd1) {
      printf("[syscalls::pipe2] Bad sync!\n");
      Halt();
    }

    fd0->closeOnExec = true;
    fd1->closeOnExec = true;
  }

cleanup:
  return out;
}

#define SYSCALL_FORK 57
static uint32_t syscallFork() 
{
  return task_fork(currentTask->syscallRegs, currentTask->syscallRsp, true, true)->id;
}

#define SYSCALL_VFORK 58
static int syscallVfork() {
  Task *newTask =
      task_fork(currentTask->syscallRegs, currentTask->syscallRsp, false, false);
  int id = newTask->id;

  // no race condition today :")
  task_create_finish(newTask);

  currentTask->state = TASK_STATE_WAITING_VFORK;
  hand_control();

  return id;
}

typedef struct CopyPtrStyle {
  int      count;
  char   **ptrPlace;
  uint8_t *valPlace;
} CopyPtrStyle;

CopyPtrStyle copyPtrStyle(char **ptr) {
  int    count = 0;
  size_t totalLen = 0;
  while (ptr[count]) {
    totalLen += strlen(ptr[count]) + 1;
    count++;
  }
  char   **ptrPlace = malloc(count * sizeof(void *));
  uint8_t *valPlace = malloc(totalLen);
  size_t   curr = 0;
  for (int i = 0; i < count; i++) {
    ptrPlace[i] = (void *)((size_t)valPlace + curr);
    curr += strlen(ptr[i]) + 1;
    memcpy(ptrPlace[i], ptr[i], strlen(ptr[i]) + 1);
  }
  CopyPtrStyle ret = {
      .count = count, .ptrPlace = ptrPlace, .valPlace = valPlace};
  return ret;
}

#define SYSCALL_EXECVE 59
static int syscallExecve(char *filename, char **argv, char **envp) {
  CopyPtrStyle arguments = copyPtrStyle(argv);
  CopyPtrStyle environment = copyPtrStyle(envp);

  char *filenameSanitized = file_system_sanitize(currentTask->cwd, filename);
  Task *ret = elfExecute(filenameSanitized, arguments.count, arguments.ptrPlace,
                         environment.count, environment.ptrPlace, 0);
  free(filenameSanitized);
  free(arguments.ptrPlace);
  free(arguments.valPlace);
  free(environment.ptrPlace);
  free(environment.valPlace);
  if (!ret)
    return -ENOENT;

  int targetId = currentTask->id;
  currentTask->id = task_generate_id();

  ret->id = targetId;
  ret->parent = currentTask->parent;
  size_t cwdLen = strlen(currentTask->cwd) + 1;
  ret->cwd = malloc(cwdLen);
  memcpy(ret->cwd, currentTask->cwd, cwdLen);
  ret->umask = currentTask->umask;

  task_files_empty(ret);
  task_files_copy(currentTask, ret, true);

  task_create_finish(ret);

  currentTask->noInformParent = true;
  task_kill(currentTask->id, 0);
  return 0; // will never be reached, it **replaces**
}

#define SYSCALL_EXIT_TASK 60
static void syscallExitTask(int return_code) {
  // if (return_code)
  //   Halt();
  task_kill(currentTask->id, return_code);
}

#define SYSCALL_WAIT4 61
static int syscallWait4(int pid, int *wstatus, int options, struct rusage *ru) {
#if DEBUG_SYSCALLS_STUB
  if (options || ru)
    printf("[syscall::wait4] UNIMPLEMENTED options{%d} rusage{%lx}!\n", options,
           ru);
  printf("[syscall::wait4] UNIMPLEMENTED WNOHANG{%d} WUNTRACED{%d} "
         "WSTOPPED{%d} WEXITED{%d} WCONTINUED{%d} "
         "WNOWAIT{%d}\n",
         options & WNOHANG, options & WUNTRACED, options & WSTOPPED,
         options & WEXITED, options & WCONTINUED, options & WNOWAIT);
#endif

  asm volatile("sti");

  // if nothing is on the side, then ensure we have something to wait() for
  if (!currentTask->childrenTerminatedAmnt) {
    int amnt = 0;

    spinlock_cnt_read_acquire(&TASK_LL_MODIFY);
    Task *browse = firstTask;
    while (browse) {
      if (browse->state == TASK_STATE_READY && browse->parent == currentTask)
        amnt++;
      browse = browse->next;
    }
    spinlock_cnt_read_release(&TASK_LL_MODIFY);

    if (!amnt)
      return -ECHILD;
  }

  // target is the item we "found"
  KilledInfo *target = 0;

  // check if specific pid item is already there
  if (pid != -1) {
    spinlock_acquire(&currentTask->LOCK_CHILD_TERM);
    KilledInfo *browse = currentTask->firstChildTerminated;
    while (browse) {
      if (browse->pid == pid)
        break;
      browse = browse->next;
    }
    target = browse;
    spinlock_release(&currentTask->LOCK_CHILD_TERM);

    // not there? wait for it!
    if (!target) {
      // "poll"
      currentTask->waitingForPid = pid;
      currentTask->state = TASK_STATE_WAITING_CHILD_SPECIFIC;
      hand_control();

      // we're back
      currentTask->waitingForPid = 0; // just for good measure
      spinlock_acquire(&currentTask->LOCK_CHILD_TERM);
      KilledInfo *browse = currentTask->firstChildTerminated;
      while (browse) {
        if (browse->pid == pid)
          break;
        browse = browse->next;
      }
      target = browse;
      spinlock_release(&currentTask->LOCK_CHILD_TERM);
    }
  } else {
    // we got children, wait for any changes
    // OR just continue :")
    if (!currentTask->childrenTerminatedAmnt) {
      currentTask->state = TASK_STATE_WAITING_CHILD;
      hand_control();
    }
    target = currentTask->firstChildTerminated;
  }

  spinlock_acquire(&currentTask->LOCK_CHILD_TERM);
  if (!target) {
    printf("[syscalls::wait4] FATAL Just fatal!");
    Halt();
  }

  int output = target->pid;
  int ret = target->ret;

  // cleanup
  LinkedListRemove((void **)(&currentTask->firstChildTerminated), target);
  currentTask->childrenTerminatedAmnt--;
  spinlock_release(&currentTask->LOCK_CHILD_TERM);

  if (wstatus)
    *wstatus = (ret & 0xff) << 8;

#if DEBUG_SYSCALLS_EXTRA
  printf("[syscall::wait4] ret{%d} ret{%d}\n", output, ret);
#endif
  return output;
}

#define SYSCALL_EXIT_GROUP 231
static void syscallExitGroup(int return_code) { syscallExitTask(return_code); }

void syscallsRegProc() {
  register_syscall(SYSCALL_PIPE, syscallPipe);
  register_syscall(SYSCALL_PIPE2, syscallPipe2);
  register_syscall(SYSCALL_EXIT_TASK, syscallExitTask);
  register_syscall(SYSCALL_FORK, syscallFork);
  register_syscall(SYSCALL_VFORK, syscallVfork);
  register_syscall(SYSCALL_WAIT4, syscallWait4);
  register_syscall(SYSCALL_EXECVE, syscallExecve);
  register_syscall(SYSCALL_EXIT_GROUP, syscallExitGroup);
}
