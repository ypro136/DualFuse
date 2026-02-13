#include <task.h>

#include <string.h>

#include <gdt.h>
#include <isr.h>
#include <data_structures/linked_list.h>
#include <linux.h>
#include <liballoc.h>
#include <paging.h>
//#include <schedule.h>
#include <task_stack.h>
#include <syscalls.h>
#include <pmm.h>
#include <vmm.h>

#include <utility.h>
#include <hcf.hpp>


bool tasksInitiated = false;

Task *firstTask;
Task *currentTask;

Task *dummyTask;

Task *netHelperTask;
void  kernelHelpEntry();

Spinlock LOCK_REAPER;
Task    *reaperTask;

// Task manager allowing for task management

SpinlockCnt TASK_LL_MODIFY = {0};

void task_attach_def_termios(Task *task) {
  memset(&task->term, 0, sizeof(termios));
  task->term.c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
  task->term.c_oflag = OPOST;
  task->term.c_cflag = CS8 | CREAD | CLOCAL;
  task->term.c_lflag = ECHO | ICANON | IEXTEN | ISIG | ECHOCTL;
  task->term.c_line = 0;
  task->term.c_cc[VINTR] = 3;     // Ctrl-C
  task->term.c_cc[VQUIT] = 28;    // Ctrl-task->term.c_cc[VERASE] = 127; // DEL
  task->term.c_cc[VKILL] = 21;    // Ctrl-U
  task->term.c_cc[VEOF] = 4;      // Ctrl-D
  task->term.c_cc[VTIME] = 0;     // No timer
  task->term.c_cc[VMIN] = 1;      // Return each byte
  task->term.c_cc[VSTART] = 17;   // Ctrl-Q
  task->term.c_cc[VSTOP] = 19;    // Ctrl-S
  task->term.c_cc[VSUSP] = 26;    // Ctrl-Z
  task->term.c_cc[VREPRINT] = 18; // Ctrl-R
  task->term.c_cc[VDISCARD] = 15; // Ctrl-O
  task->term.c_cc[VWERASE] = 23;  // Ctrl-W
  task->term.c_cc[VLNEXT] = 22;   // Ctrl-V
  // Initialize other control characters to 0
  for (int i = 16; i < NCCS; i++) {
    task->term.c_cc[i] = 0;
  }
}

// although there are locks on these two functions, they are EXTREMELY unsafe!
Task *task_list_allocate() {
  spinlock_cnt_write_acquire(&TASK_LL_MODIFY);
  Task *target = (Task *)malloc(sizeof(Task));
  memset(target, 0, sizeof(Task)); // TASK_STATE_DEAD is 0 too
  asm volatile("cli");
  Task *browse = firstTask;
  while (browse) {
    if (!browse->next)
      break; // found final
    browse = browse->next;
  }

  assert(browse);
  browse->next = target;
  asm volatile("sti");
  spinlock_cnt_write_release(&TASK_LL_MODIFY);
  return target;
}

// will NEVER be the first one
void task_list_destroy(Task *target) {
  spinlock_cnt_write_acquire(&TASK_LL_MODIFY);
  asm volatile("cli");
  Task *prev = firstTask;
  while (prev) {
    if (prev->next == target)
      break;
    prev = prev->next;
  }
  assert(prev);

  prev->next = target->next;
  asm volatile("sti");
  spinlock_cnt_write_release(&TASK_LL_MODIFY);
  free(target); // finally, destroy it
}

Task *task_create(uint32_t id, uint64_t rip, bool kernel_task, uint64_t *pagedir,
                  uint32_t argc, char **argv) {
  Task *target = task_list_allocate();

  uint64_t code_selector =
      kernel_task ? GDT_KERNEL_CODE : (GDT_USER_CODE | DPL_USER);
  uint64_t data_selector =
      kernel_task ? GDT_KERNEL_DATA : (GDT_USER_DATA | DPL_USER);

  target->registers.ds = data_selector;
  target->registers.cs = code_selector;
  target->registers.usermode_ss = data_selector;
  target->registers.usermode_rsp = USER_STACK_BOTTOM;

  target->registers.rflags = 0x200; // enable interrupts
  target->registers.rip = rip;

  target->id = id;
  target->tgid = id;
  target->sid = 1; // to dummy
  target->ctrlPty = -1;
  target->kernel_task = kernel_task;
  target->state = TASK_STATE_CREATED; // TASK_STATE_READY
  // target->pagedir = pagedir;
  target->infoPd = taskInfoPdAllocate(false);
  target->infoPd->pagedir = pagedir; // no lock cause only we use it

  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  target->whileTssRsp = (uint64_t)tssRsp + tssRspSize;

  void  *syscalltssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t syscalltssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(syscalltssRsp, 0, syscalltssRspSize);
  target->whileSyscallRsp = (uint64_t)syscalltssRsp + syscalltssRspSize;

  target->infoFs = taskInfoFsAllocate();
  target->infoFiles = taskInfoFilesAllocate();
  target->infoSignals = taskInfoSignalAllocate();

  LinkedListInit(&target->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&target->dsSysIntr, sizeof(TaskSysInterrupted));

  memset(target->fpuenv, 0, 512);
  ((uint16_t *)target->fpuenv)[0] = 0x37f;
  target->mxcsr = 0x1f80;

  task_attach_def_termios(target);

  // just in case it ends up becoming an orphan
  target->parent = firstTask;

  return target;
}

Task *task_create_kernel(uint64_t rip, uint64_t rdi) {
  Task *target =
      task_create(task_generate_id(), rip, true, page_directory_allocate(), 0, 0);
  stack_generate_kernel(target, rdi);
  task_create_finish(target);
  return target;
}

void task_name_kernel(Task *target, const char *str, int len) {
  target->cmdline = malloc(len);
  memcpy(target->cmdline, str, len);
  target->cmdlineLen = len;
}

void task_create_finish(Task *task) { task->state = TASK_STATE_READY; }

void task_adjust_heap(Task *task, size_t new_heap_end, size_t *start,
                      size_t *end) {
  if (new_heap_end <= *start) {
    printf("[task] Tried to adjust heap behind current values: id{%d}\n",
           task->id);
    task_kill(task->id, 139);
    return;
  }

  size_t old_page_top = CEILING_DIVISION(*end, PAGE_SIZE);
  size_t new_page_top = CEILING_DIVISION(new_heap_end, PAGE_SIZE);

  if (new_page_top > old_page_top) {
    size_t num = new_page_top - old_page_top;

    for (size_t i = 0; i < num; i++) {
      size_t virt = old_page_top * PAGE_SIZE + i * PAGE_SIZE;
      if (virtual_to_physical(virt))
        continue;

      size_t phys = physical_allocate(1);

      virtual_map(virt, phys, PF_RW | PF_USER);

      memset((void *)virt, 0, PAGE_SIZE);
    }
  } else if (new_page_top < old_page_top) {
    printf("[task] New page is lower than old page: id{%d}\n", task->id);
    task_kill(task->id, 139);
    return;
  }

  *end = new_heap_end;
}

void task_call_reaper(Task *target) {
  while (true) {
    spinlock_acquire(&LOCK_REAPER);
    if (!reaperTask) {
      // there is space!
      reaperTask = target;
      spinlock_release(&LOCK_REAPER);
      return;
    }
    spinlock_release(&LOCK_REAPER);
    hand_control();
  }
}

void task_kill(uint32_t id, uint16_t ret) {
  Task *task = task_get(id);
  if (!task)
    return;

  // Notify that poor parent... they must've been searching all over the
  // place!
  if (task->parent && !task->noInformParent) {
    spinlock_acquire(&task->parent->LOCK_CHILD_TERM);
    KilledInfo *info = (KilledInfo *)LinkedListAllocate(
        &task->parent->dsChildTerminated, sizeof(KilledInfo));
    info->pid = task->id;
    info->ret = ret;
    task->parent->childrenTerminatedAmnt++;
    if (task->parent->state == TASK_STATE_WAITING_CHILD ||
        (task->parent->state == TASK_STATE_WAITING_CHILD_SPECIFIC &&
         task->parent->waitingForPid == task->id))
      task->parent->state = TASK_STATE_READY;
    spinlock_release(&task->parent->LOCK_CHILD_TERM);
    atomicBitmapSet(&task->parent->sigPendingList, SIGCHLD);
  }

  // vfork() children need to notify parents no matter what
  if (task->parent && task->parent->state == TASK_STATE_WAITING_VFORK)
    task->parent->state = TASK_STATE_READY;

  if (task->tidptr) {
    // *task->tidptr = 0;
    atomicWrite32((uint32_t *)task->tidptr, 0);
    futexSyscall((uint32_t *)task->tidptr, FUTEX_WAKE, 1, 0, 0, 0);
  }

  // close any left open files
  taskInfoFilesDiscard(task->infoFiles, task);

  // if (!parentVfork)
  //   page_directory_free(task->pagedir);
  taskInfoPdDiscard(task->infoPd);
  // ^ only changes userspace locations so we don't need to change our pagedir

  // the "reaper" thread will finish everything in a safe context
  task_call_reaper(task);
  task->state = TASK_STATE_DEAD;

  if (currentTask == task) {
    // we're most likely in a syscall context, so...
    // task_killCleanup(task); // left for sched
    asm volatile("sti");
    // wait until we're outta here
    while (1) {
      //   printf("GET ME OUT ");
    }
  }
}

void task_free_children(Task *task) {
  if (task->noInformParent)
    return; // it's execve() trash most likely
  spinlock_cnt_read_acquire(&TASK_LL_MODIFY);
  Task *child = firstTask;
  while (child) {
    Task *next = child->next;
    // todo: reparent to init!
    if (child->parent == task && child->state != TASK_STATE_DEAD) {
      // if (!child->parent)
      // child->parent = firstTask;
      // else
      //   child->parent = child->parent->parent;

      // reparent to dummy (it doesn't care one bit, it'd be a bad parent)
      child->parent = dummyTask;
    }
    child = next;
  }
  spinlock_cnt_read_release(&TASK_LL_MODIFY);
}

Task *task_get(uint32_t id) {
  spinlock_cnt_read_acquire(&TASK_LL_MODIFY);
  Task *browse = firstTask;
  while (browse) {
    if (browse->id == id)
      break;
    browse = browse->next;
  }
  spinlock_cnt_read_release(&TASK_LL_MODIFY);
  return browse;
}

uint64_t taskIdCurr = 1;
uint64_t task_generate_id() { return taskIdCurr++; }


TaskInfoFs *taskInfoFsClone(TaskInfoFs *old) {
  TaskInfoFs *new_task_info_filesystem = taskInfoFsAllocate();

  spinlockAcquire(&old->LOCK_FS);
  new_task_info_filesystem->umask = old->umask;
  size_t len = strlen(old->cwd) + 1;
  free(new_task_info_filesystem->cwd); // no more default
  new_task_info_filesystem->cwd = malloc(len);
  memcpy(new_task_info_filesystem->cwd, old->cwd, len);
  spinlockRelease(&old->LOCK_FS);

  return new_task_info_filesystem;
}


// CLONE_FS
TaskInfoFs *taskInfoFsAllocate() {
  TaskInfoFs *target = calloc(sizeof(TaskInfoFs), 1);
  target->utilizedBy = 1;
  target->cwd = calloc(2, 1);
  target->cwd[0] = '/';
  target->umask = S_IWGRP | S_IWOTH;
  return target;
}

void taskInfoFsDiscard(TaskInfoFs *target) {
  spinlockAcquire(&target->LOCK_FS);
  target->utilizedBy--;
  if (!target->utilizedBy) {
    free(target->cwd);
    free(target);
  } else
    spinlockRelease(&target->LOCK_FS);
}

void taskInfoPdDiscard(TaskInfoPagedir *target) {
  spinlockAcquire(&target->LOCK_PD);
  target->utilizedBy--;
  if (!target->utilizedBy) {
    page_directory_free(target->pagedir);
    // todo: find a safe way to free target
    // cannot be done w/the current layout as it's done inside taskKill and the
    // scheduler needs it in case it's switched in between (will point to
    // invalid/unsafe memory). maybe with overrides but we'll see later when the
    // system is more stable.
  } else
    spinlockRelease(&target->LOCK_PD);
}

// CLONE_FILES
TaskInfoFiles *taskInfoFilesAllocate() {
  TaskInfoFiles *target = calloc(sizeof(TaskInfoFiles), 1);
  target->utilizedBy = 1;
  target->rlimitFdsHard = 1024;
  target->rlimitFdsSoft = 1024;
  target->fdBitmap = calloc(target->rlimitFdsHard / 8, 1);
  return target;
}

void taskInfoFilesDiscard(TaskInfoFiles *target, void *task) {
  spinlock_cnt_write_acquire(&target->WLOCK_FILES);
  target->utilizedBy--;
  if (!target->utilizedBy) {
    // we don't care about locks anymore (we are alone in the darkness)
    spinlock_cnt_write_release(&target->WLOCK_FILES);
    while (target->firstFile)
      fsUserClose(task, target->firstFile->key);
    free(target->fdBitmap);
    free(target);
  } else
    spinlock_cnt_write_release(&target->WLOCK_FILES);
}

// Signal stuff
TaskInfoSignal *taskInfoSignalAllocate() {
  // everything will be initiated to 0, standing for SIG_DFL (default handling)
  TaskInfoSignal *target = calloc(sizeof(TaskInfoSignal), 1);
  target->utilizedBy = 1;
  return target;
}
TaskInfoSignal *taskInfoSignalClone(TaskInfoSignal *old) {
  TaskInfoSignal *target = taskInfoSignalAllocate();

  spinlockAcquire(&old->LOCK_SIGNAL);
  memcpy(target->signals, old->signals, sizeof(old->signals));
  spinlockRelease(&old->LOCK_SIGNAL);

  return target;
}
void taskInfoSignalDiscard(TaskInfoSignal *target) {
  spinlockAcquire(&target->LOCK_SIGNAL);
  target->utilizedBy--;
  if (!target->utilizedBy) {
    free(target);
  } else
    spinlockRelease(&target->LOCK_SIGNAL);
}


// CLONE_VM
TaskInfoPagedir *taskInfoPdAllocate(bool pagedir) {
  TaskInfoPagedir *target = calloc(sizeof(TaskInfoPagedir), 1);
  target->utilizedBy = 1;
  if (pagedir)
    target->pagedir = page_directory_allocate();
  target->heap_start = USER_HEAP_START;
  target->heap_end = USER_HEAP_START;

  target->mmap_start = USER_MMAP_START;
  target->mmap_end = USER_MMAP_START;
  return target;
}

TaskInfoPagedir *taskInfoPdClone(TaskInfoPagedir *old) {
  TaskInfoPagedir *new_task_info_pagedir = taskInfoPdAllocate(true);

  spinlockAcquire(&old->LOCK_PD);
  page_directory_user_duplicate(old->pagedir, new_task_info_pagedir->pagedir);
  new_task_info_pagedir->heap_start = old->heap_start;
  new_task_info_pagedir->heap_end = old->heap_end;

  new_task_info_pagedir->mmap_start = old->mmap_start;
  new_task_info_pagedir->mmap_end = old->mmap_end;
  spinlockRelease(&old->LOCK_PD);

  return new_task_info_pagedir;
}


size_t task_change_cwd(char *newdir) {
  stat stat = {0};
  spinlock_acquire(&currentTask->infoFs->LOCK_FS);
  char *safeNewdir = fsSanitize(currentTask->infoFs->cwd, newdir);
  spinlock_release(&currentTask->infoFs->LOCK_FS);
  if (!fsStatByFilename(currentTask, safeNewdir, &stat)) {
    free(safeNewdir);
    return ERR(ENOENT);
  }

  if (!(stat.st_mode & S_IFDIR)) {
    free(safeNewdir);
    return ERR(ENOTDIR);
  }

  size_t len = strlen(safeNewdir) + 1;
  spinlock_acquire(&currentTask->infoFs->LOCK_FS);
  currentTask->infoFs->cwd = realloc(currentTask->infoFs->cwd, len);
  memcpy(currentTask->infoFs->cwd, safeNewdir, len);
  spinlock_release(&currentTask->infoFs->LOCK_FS);

  free(safeNewdir);
  return 0;
}

typedef struct {
  bool  respect_coe;
  Task *target;
} task_files_inorder_args;

void task_files_copy_inorder(AVLheader *root, task_files_inorder_args *args) {
  if (!root)
    return;
  task_files_copy_inorder(root->left, args);
  // process this
  OpenFile *curr = (OpenFile *)root->value;
  if (!args->respect_coe || !curr->closeOnExec)
    assert(fsUserDuplicateNode(args->target, curr, curr->id));
  task_files_copy_inorder(root->right, args);
}

void task_files_copy(Task *original, Task *target, bool respect_coe) {
  TaskInfoFiles *originalInfo = original->infoFiles;
  TaskInfoFiles *targetInfo = target->infoFiles;
  spinlock_cnt_read_acquire(&originalInfo->WLOCK_FILES);
  spinlock_cnt_write_acquire(&targetInfo->WLOCK_FILES);
  targetInfo->rlimitFdsHard = originalInfo->rlimitFdsHard;
  targetInfo->rlimitFdsSoft = originalInfo->rlimitFdsSoft;
  targetInfo->fdBitmap = malloc(targetInfo->rlimitFdsHard / 8);
  memcpy(targetInfo->fdBitmap, originalInfo->fdBitmap,
         targetInfo->rlimitFdsHard / 8);
  spinlock_cnt_write_release(&targetInfo->WLOCK_FILES); // inorder will manage this
  task_files_inorder_args args = {.respect_coe = respect_coe, .target = target};
  task_files_copy_inorder((void *)originalInfo->firstFile, &args);
  spinlock_cnt_read_release(&originalInfo->WLOCK_FILES);
}

Task *task_fork(AsmPassedInterrupt *cpu, uint64_t rsp, int clone_flags,
                bool spinup) {
  Task *target = task_list_allocate();

  if (!(clone_flags & CLONE_VM)) {
    target->infoPd = taskInfoPdClone(currentTask->infoPd);
  } else {
    TaskInfoPagedir *share = currentTask->infoPd;
    spinlock_acquire(&share->LOCK_PD);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_PD);
    target->infoPd = share; // share it yk!
  }

  target->id = task_generate_id();
  target->tgid = target->id;
  target->pgid = currentTask->pgid;
  target->sid = currentTask->sid;
  target->ctrlPty = currentTask->ctrlPty;
  target->kernel_task = currentTask->kernel_task;
  target->state = TASK_STATE_CREATED;

  target->cmdlineLen = currentTask->cmdlineLen;
  target->cmdline = malloc(target->cmdlineLen);
  memcpy(target->cmdline, currentTask->cmdline, target->cmdlineLen);
  if (currentTask->execname)
    target->execname = strdup(currentTask->execname);

  if (clone_flags & CLONE_THREAD)
    target->tgid = currentTask->tgid;

  // target->registers = currentTask->registers;
  memcpy(&target->registers, cpu, sizeof(AsmPassedInterrupt));
  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  target->whileTssRsp = (uint64_t)tssRsp + tssRspSize;

  void  *syscalltssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t syscalltssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(syscalltssRsp, 0, syscalltssRspSize);
  target->whileSyscallRsp = (uint64_t)syscalltssRsp + syscalltssRspSize;

  target->fsbase = currentTask->fsbase;
  target->gsbase = currentTask->gsbase;

  // target->heap_start = currentTask->heap_start;
  // target->heap_end = currentTask->heap_end;

  // target->mmap_start = currentTask->mmap_start;
  // target->mmap_end = currentTask->mmap_end;

  target->term = currentTask->term;

  target->tmpRecV = currentTask->tmpRecV;

  if (!(clone_flags & CLONE_FS))
    target->infoFs = taskInfoFsClone(currentTask->infoFs);
  else {
    TaskInfoFs *share = currentTask->infoFs;
    spinlock_acquire(&share->LOCK_FS);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_FS);
    target->infoFs = share;
  }

  if (!(clone_flags & CLONE_FILES)) {
    target->infoFiles = taskInfoFilesAllocate();
    task_files_copy(currentTask, target, false);
  } else {
    TaskInfoFiles *share = currentTask->infoFiles;
    spinlock_cnt_write_acquire(&share->WLOCK_FILES);
    share->utilizedBy++;
    spinlock_cnt_write_release(&share->WLOCK_FILES);
    target->infoFiles = share;
  }

  if (!(clone_flags & CLONE_SIGHAND))
    target->infoSignals = taskInfoSignalClone(currentTask->infoSignals);
  else {
    TaskInfoSignal *share = currentTask->infoSignals;
    spinlock_acquire(&share->LOCK_SIGNAL);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_SIGNAL);
    target->infoSignals = share;
  }

  LinkedListInit(&target->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&target->dsSysIntr, sizeof(TaskSysInterrupted));

  // they get inherited, but can still be changed thread-wise!
  target->sigBlockList = currentTask->sigBlockList;

  // returns zero yk
  target->registers.rax = 0;

  // etc (https://www.felixcloutier.com/x86/syscall)
  target->registers.rip = cpu->rcx;
  target->registers.cs = GDT_USER_CODE | DPL_USER;
  target->registers.ds = GDT_USER_DATA | DPL_USER;
  target->registers.rflags = cpu->r11;
  target->registers.usermode_rsp = rsp;
  target->registers.usermode_ss = GDT_USER_DATA | DPL_USER;

  // since the scheduler, our fpu state might've changed
  asm volatile(" fxsave %0 " ::"m"(currentTask->fpuenv));
  asm("stmxcsr (%%rax)" : : "a"(&currentTask->mxcsr));

  // yk
  target->parent = currentTask;
  target->pgid = currentTask->pgid;

  // fpu stuff
  memcpy(target->fpuenv, currentTask->fpuenv, 512);
  target->mxcsr = currentTask->mxcsr;

  target->extras = currentTask->extras;

  if (spinup)
    task_create_finish(target);

  return target;
}

// Will release lock when task isn't running via the kernel helper
void task_spinlock_exit(Task *task, Spinlock *lock) {
  assert(!task->spinlockQueueEntry);
  task->spinlockQueueEntry = lock;
}

void kernel_dummy_entry() {
  while (true)
    asm volatile("pause");
}

void tasks_initialize() {
  firstTask = (Task *)malloc(sizeof(Task));
  memset(firstTask, 0, sizeof(Task));

  currentTask = firstTask;
  currentTask->id = KERNEL_TASK_ID;
  currentTask->state = TASK_STATE_READY;
  currentTask->infoPd = taskInfoPdAllocate(false);
  currentTask->infoPd->pagedir = get_page_directory();
  currentTask->kernel_task = true;
  currentTask->infoFs = taskInfoFsAllocate();
  currentTask->infoFiles = taskInfoFilesAllocate();
  currentTask->infoFiles->fdBitmap[0] = (uint8_t)-1;
  currentTask->infoSignals = 0; // no, just no!
  LinkedListInit(&currentTask->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&currentTask->dsSysIntr, sizeof(TaskSysInterrupted));
  task_name_kernel(currentTask, entryCmdline, sizeof(entryCmdline));

  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  currentTask->whileTssRsp = (uint64_t)tssRsp + tssRspSize;
  task_attach_def_termios(currentTask);

  printf("[tasks] Current execution ready for multitasking\n");
  tasksInitiated = true;

  // task 0 represents the execution we're in right now

  // create a dummy task in case the scheduler has nothing to do
  dummyTask = task_create_kernel((uint64_t)kernel_dummy_entry, 0);
  dummyTask->state = TASK_STATE_DUMMY;
  task_name_kernel(dummyTask, dummyCmdline, sizeof(dummyCmdline));
}