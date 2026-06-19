#include <task.h>

#include <string.h>

#include <gdt.h>
#include <isr.h>
#include <data_structures/linked_list.h>
#include <linux.h>
#include <liballoc.h>
#include <paging.h>
#include <task_stack.h>
#include <syscalls.h>
#include <pmm.h>
#include <vmm.h>
#include <task_stack.h>

#include <utility.h>
#include <hcf.hpp>
#include <bootloader.h>

Task* per_lapic_core_current_task[256] = {0};

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

void kernel_ap_idle_entry() {
    while (true)
        asm volatile("sti; hlt");
}

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
    // Allocate a whole page for the task structure (guaranteed 16‑byte aligned)
    Task *target = (Task *)virtual_allocate(1);   // 1 page = 4 KB
    printf("[task] virtual_allocate returned %p\n", target);
    if (!target) {
        spinlock_cnt_write_release(&TASK_LL_MODIFY);
        return NULL;
    }
    printf("[task] Allocated task structure at %p\n", target);
    memset(target, 0, PAGE_SIZE);   // clear the whole page

    asm volatile("cli"); 
    Task *browse = firstTask;
    while (browse) {
        if (!browse->next) break;
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
        if (prev->next == target) break;
        prev = prev->next;
    }
    assert(prev);
    prev->next = target->next;
    asm volatile("sti");
    spinlock_cnt_write_release(&TASK_LL_MODIFY);
    virtual_free(target, 1);   // free the page
}

Task *task_create(uint32_t id, uint64_t rip, bool kernel_task, uint64_t *pagedir, uint32_t argc, char **argv) {
  printf("[task_create] start\n");
  Task *target = task_list_allocate();

  printf("[task_create] task_list_allocate done %p\n", target);
  uint64_t code_selector = kernel_task ? GDT_KERNEL_CODE : (GDT_USER_CODE | DPL_USER);
  uint64_t data_selector = kernel_task ? GDT_KERNEL_DATA : (GDT_USER_DATA | DPL_USER);

  target->core_affinity = TASK_AFFINITY_BSP;   // only BSP may run

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
  printf("[task_create] infoPd allocated %p\n", target->infoPd);
  if (!target->infoPd) {
      printf("task_create: infoPd allocation failed\n");
      free(target);
      return NULL;
  }
  target->infoPd->pagedir = pagedir; // no lock cause only we use it
  printf("[task_create] pagedir set %p\n", pagedir);

  printf("[task_create] infoSignals done\n");

  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  target->whileTssRsp = (uint64_t)tssRsp + tssRspSize;

  void  *syscalltssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t syscalltssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(syscalltssRsp, 0, syscalltssRspSize);
  target->whileSyscallRsp = (uint64_t)syscalltssRsp + syscalltssRspSize;

  printf("[task_create] stacks done\n");

  target->infoFs = taskInfoFsAllocate();
      printf("[task_create] infoFs done\n");
  target->infoFiles = taskInfoFilesAllocate();
      printf("[task_create] infoFiles done\n");
  target->infoSignals = taskInfoSignalAllocate();
      printf("[task_create] infoSignals done\n");

  LinkedListInit(&target->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&target->dsSysIntr, sizeof(TaskSysInterrupted));

  memset(target->fpuenv, 0, 512);
  ((uint16_t *)target->fpuenv)[0] = 0x37f;           // FCW at offset 0
  *(uint32_t *)(&target->fpuenv[24]) = 0x1f80;        // MXCSR at offset 24 in fxsave area
  target->mxcsr = 0x1f80;

  target->core_affinity = TASK_AFFINITY_BSP;


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

  // Always try to map pages for the range [old_page_top, new_page_top).
  // If the heap is currently empty, we must map at least the page that
  // contains *end, even when new_page_top == old_page_top.
  if (new_page_top >= old_page_top) {
      size_t num = new_page_top - old_page_top;
      if (num == 0 && *end == *start && new_heap_end > *start) 
          num = 1;                     // first allocation – map the initial page

      for (size_t i = 0; i < num; i++) {
          size_t virt = old_page_top * PAGE_SIZE + i * PAGE_SIZE;
          if (virtual_to_physical(virt))   // already mapped? skip
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
  if (task->state == TASK_STATE_DEAD)
    return;

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

  if (current_task_this_core() == task) {
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
TaskInfoFiles *taskInfoFilesAllocate() 
{
  #if defined(DEBUG_TASK)
    printf("[tasks] enter taskInfoFilesAllocate\n");
    #endif
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
  #if defined(DEBUG_TASK)
    printf("[tasks] enter taskInfoPdAllocate \n");
    #endif

    #if defined(DEBUG_TASK)
    printf("[tasks] pagedir is : %d \n", pagedir);
    #endif
  TaskInfoPagedir *target = calloc(sizeof(TaskInfoPagedir), 1);
  #if defined(DEBUG_TASK)
    printf("[tasks] target is at : %lx \n", target);
    #endif
  target->utilizedBy = 1;
  if (pagedir)
  {
    target->pagedir = page_directory_allocate();
  }
  
  target->heap_start = USER_HEAP_START;
  target->heap_end = USER_HEAP_START;

  target->mmap_start = USER_MMAP_START;
  target->mmap_end = USER_MMAP_START;
  #if defined(DEBUG_TASK)
    printf("[tasks] end taskInfoPdAllocate \n");
    #endif
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
  spinlock_acquire(&current_task_this_core()->infoFs->LOCK_FS);
  char *safeNewdir = fsSanitize(current_task_this_core()->infoFs->cwd, newdir);
  spinlock_release(&current_task_this_core()->infoFs->LOCK_FS);
  if (!fsStatByFilename(current_task_this_core(), safeNewdir, &stat)) {
    free(safeNewdir);
    return ERR(ENOENT);
  }

  if (!(stat.st_mode & S_IFDIR)) {
    free(safeNewdir);
    return ERR(ENOTDIR);
  }

  size_t len = strlen(safeNewdir) + 1;
  spinlock_acquire(&current_task_this_core()->infoFs->LOCK_FS);
  current_task_this_core()->infoFs->cwd = realloc(current_task_this_core()->infoFs->cwd, len);
  memcpy(current_task_this_core()->infoFs->cwd, safeNewdir, len);
  spinlock_release(&current_task_this_core()->infoFs->LOCK_FS);

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
    target->infoPd = taskInfoPdClone(current_task_this_core()->infoPd);
  } else {
    TaskInfoPagedir *share = current_task_this_core()->infoPd;
    spinlock_acquire(&share->LOCK_PD);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_PD);
    target->infoPd = share; // share it yk!
  }

  target->core_affinity = TASK_AFFINITY_BSP;   // only BSP may run

  target->id = task_generate_id();
  target->tgid = target->id;
  target->pgid = current_task_this_core()->pgid;
  target->sid = current_task_this_core()->sid;
  target->ctrlPty = current_task_this_core()->ctrlPty;
  target->kernel_task = current_task_this_core()->kernel_task;
  target->state = TASK_STATE_CREATED;

  target->cmdlineLen = current_task_this_core()->cmdlineLen;
  target->cmdline = malloc(target->cmdlineLen);
  memcpy(target->cmdline, current_task_this_core()->cmdline, target->cmdlineLen);
  if (current_task_this_core()->execname)
    target->execname = strdup(current_task_this_core()->execname);

  if (clone_flags & CLONE_THREAD)
    target->tgid = current_task_this_core()->tgid;

  // target->registers = current_task_this_core()->registers;
  memcpy(&target->registers, cpu, sizeof(AsmPassedInterrupt));
  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  target->whileTssRsp = (uint64_t)tssRsp + tssRspSize;

  void  *syscalltssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t syscalltssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(syscalltssRsp, 0, syscalltssRspSize);
  target->whileSyscallRsp = (uint64_t)syscalltssRsp + syscalltssRspSize;

  target->fsbase = current_task_this_core()->fsbase;
  target->gsbase = current_task_this_core()->gsbase;

  // target->heap_start = current_task_this_core()->heap_start;
  // target->heap_end = current_task_this_core()->heap_end;

  // target->mmap_start = current_task_this_core()->mmap_start;
  // target->mmap_end = current_task_this_core()->mmap_end;

  target->term = current_task_this_core()->term;

  target->tmpRecV = current_task_this_core()->tmpRecV;

  if (!(clone_flags & CLONE_FS))
    target->infoFs = taskInfoFsClone(current_task_this_core()->infoFs);
  else {
    TaskInfoFs *share = current_task_this_core()->infoFs;
    spinlock_acquire(&share->LOCK_FS);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_FS);
    target->infoFs = share;
  }

  if (!(clone_flags & CLONE_FILES)) {
    target->infoFiles = taskInfoFilesAllocate();
    task_files_copy(current_task_this_core(), target, false);
  } else {
    TaskInfoFiles *share = current_task_this_core()->infoFiles;
    spinlock_cnt_write_acquire(&share->WLOCK_FILES);
    share->utilizedBy++;
    spinlock_cnt_write_release(&share->WLOCK_FILES);
    target->infoFiles = share;
  }

  if (!(clone_flags & CLONE_SIGHAND))
    target->infoSignals = taskInfoSignalClone(current_task_this_core()->infoSignals);
  else {
    TaskInfoSignal *share = current_task_this_core()->infoSignals;
    spinlock_acquire(&share->LOCK_SIGNAL);
    share->utilizedBy++;
    spinlock_release(&share->LOCK_SIGNAL);
    target->infoSignals = share;
  }

  LinkedListInit(&target->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&target->dsSysIntr, sizeof(TaskSysInterrupted));

  // they get inherited, but can still be changed thread-wise!
  target->sigBlockList = current_task_this_core()->sigBlockList;

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
  asm volatile(" fxsave %0 " ::"m"(current_task_this_core()->fpuenv));
  asm("stmxcsr (%%rax)" : : "a"(&current_task_this_core()->mxcsr));

  // yk
  target->parent = current_task_this_core();
  target->pgid = current_task_this_core()->pgid;

  // fpu stuff
  memcpy(target->fpuenv, current_task_this_core()->fpuenv, 512);
  target->mxcsr = current_task_this_core()->mxcsr;

  target->extras = current_task_this_core()->extras;

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

void tasks_initialize() 
{
  #if defined(DEBUG_TASK)
    printf("[tasks] enter tasks_initialize\n");
    #endif
  firstTask = (Task *)malloc(sizeof(Task));
  if (!firstTask) {
      printf("[tasks] FATAL: malloc failed for firstTask\n");
      Halt();
  }  
  memset(firstTask, 0, sizeof(Task));

  #if defined(DEBUG_TASK)
    printf("[tasks] allocated firstTask in memory \n");
    #endif

    
    currentTask = firstTask;
    current_task_this_core()->core_affinity = TASK_AFFINITY_BSP;
  per_lapic_core_current_task[apicGetBspLapicId()] = current_task_this_core();
  current_task_this_core()->id = KERNEL_TASK_ID;
  current_task_this_core()->state = TASK_STATE_READY;
  current_task_this_core()->infoPd = taskInfoPdAllocate(false);
  current_task_this_core()->infoPd->pagedir = get_page_directory();
  current_task_this_core()->kernel_task = true;
  current_task_this_core()->infoFs = taskInfoFsAllocate();
  current_task_this_core()->infoFiles = taskInfoFilesAllocate();
  current_task_this_core()->infoFiles->fdBitmap[0] = (uint8_t)-1;
  current_task_this_core()->infoSignals = 0; // no, just no!
  LinkedListInit(&current_task_this_core()->dsChildTerminated, sizeof(KilledInfo));
  LinkedListInit(&current_task_this_core()->dsSysIntr, sizeof(TaskSysInterrupted));
  task_name_kernel(currentTask, entryCmdline, sizeof(entryCmdline));
  #if defined(DEBUG_TASK)
    printf("[tasks] initialized firstTask \n");
    #endif

  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  current_task_this_core()->whileTssRsp = (uint64_t)tssRsp + tssRspSize;
  task_attach_def_termios(currentTask);

  #if defined(DEBUG_TASK)
    printf("[tasks] initialized tssRsp and termios \n");
    #endif

  printf("[tasks] Current execution ready for multitasking\n");
  tasksInitiated = true;

  // task 0 represents the execution we're in right now

  // create a dummy task in case the scheduler has nothing to do
  dummyTask = task_create_kernel((uint64_t)kernel_dummy_entry, 0);
  dummyTask->core_affinity = TASK_AFFINITY_BSP; 
  dummyTask->state = TASK_STATE_DUMMY;
  task_name_kernel(dummyTask, dummyCmdline, sizeof(dummyCmdline));
}

// Safely free everything a dead task still holds.
// Must be called repeatedly from a kernel context (BSP idle loop or a dedicated reaper task).
void task_reaper_loop(void) {
    spinlock_acquire(&LOCK_REAPER);

    while (reaperTask != NULL) {
        Task *victim = reaperTask;
        reaperTask = NULL;                 // let new victims be queued
        spinlock_release(&LOCK_REAPER);    // release early, work on our copy

        // Free the user and syscall stacks (compute base from top)
        if (victim->whileTssRsp) {
            void *base = (void *)(victim->whileTssRsp -
                                  USER_STACK_PAGES * BLOCK_SIZE);
            virtual_free(base, USER_STACK_PAGES);
        }
        if (victim->whileSyscallRsp) {
            void *base = (void *)(victim->whileSyscallRsp -
                                  USER_STACK_PAGES * BLOCK_SIZE);
            virtual_free(base, USER_STACK_PAGES);
        }

        // Free the command line and executable name (both are malloc'd)
        if (victim->cmdline)  free(victim->cmdline);
        if (victim->execname) free(victim->execname);

        // Remove from the global task list and free the 4K page
        task_list_destroy(victim);

        spinlock_acquire(&LOCK_REAPER);    // reacquire for next victim
    }

    spinlock_release(&LOCK_REAPER);
}

// Optional dedicated reaper task entry – use only if you want a
// separate task instead of running the reaper inside the BSP idle loop.
void reaper_kernel_task_entry(void) {
    while (1) {
        task_reaper_loop();
        asm volatile("pause");
    }
}

// One‑shot kernel task entry.
// The real entry point is passed in RDI (the second argument to task_create_kernel).
void kernel_one_shot_entry(void) {
    // Retrieve the function pointer that was stored in the task's RDI
    void (*real_func)(void) = (void (*)(void)) current_task_this_core()->registers.rdi;

    real_func();                               // execute the actual work

    // Self‑terminate (this call never returns)
    task_kill(current_task_this_core()->id, 0);

    // Paranoia – should never get here
    while (1) { asm volatile("pause"); }
}

// Create a kernel task and give it a name in one call.
// Returns the Task* if you need it, otherwise you can ignore the return value.
Task* task_create_named_kernel(uint64_t rip, uint64_t rdi,
                                      const char *name) {
    Task *t = task_create_kernel(rip, rdi);
    if (t && name)
        task_name_kernel(t, name, strlen(name));
    return t;
}

static void check_stack_overlap(void *vaddr, size_t pages) {
    uint64_t alloc_virt_start = (uint64_t)vaddr;
    uint64_t alloc_virt_end   = alloc_virt_start + pages * BLOCK_SIZE;

    // Physical pages we just received (via HHDM, so phys = vaddr - hhdmOffset)
    uint64_t alloc_phys_start = alloc_virt_start - bootloader.hhdmOffset;
    uint64_t alloc_phys_end   = alloc_phys_start + pages * BLOCK_SIZE;

    spinlock_cnt_read_acquire(&TASK_LL_MODIFY);

    for (Task *t = firstTask; t != NULL; t = t->next) {
        if (t->state == TASK_STATE_DEAD)
            continue;

        // Check virtual overlap (as before)
        uint64_t tss_base = t->whileTssRsp - USER_STACK_PAGES * BLOCK_SIZE;
        uint64_t tss_end  = t->whileTssRsp;
        if (alloc_virt_start < tss_end && alloc_virt_end > tss_base) {
            printf("[FATAL] Virtual overlap with task %d kernel stack\n", t->id);
            Halt();
        }
        uint64_t sys_base = t->whileSyscallRsp - USER_STACK_PAGES * BLOCK_SIZE;
        uint64_t sys_end  = t->whileSyscallRsp;
        if (alloc_virt_start < sys_end && alloc_virt_end > sys_base) {
            printf("[FATAL] Virtual overlap with task %d syscall stack\n", t->id);
            Halt();
        }

        // NEW: Check physical overlap – does our new physical range
        //       coincide with the physical pages backing another task’s stacks?
        // We translate each task’s stack virtual base to physical.
        uint64_t tss_virt_base = tss_base;
        uint64_t tss_phys_base = virtual_to_physical(tss_virt_base);
        if (tss_phys_base) {
            uint64_t tss_phys_end = tss_phys_base + USER_STACK_PAGES * BLOCK_SIZE;
            if (alloc_phys_start < tss_phys_end && alloc_phys_end > tss_phys_base) {
                printf("[FATAL] Physical overlap: new phys 0x%lx-0x%lx "
                       "clashes with task %d kernel stack (virt 0x%lx-0x%lx -> phys 0x%lx-0x%lx)\n",
                       alloc_phys_start, alloc_phys_end, t->id,
                       tss_virt_base, tss_end, tss_phys_base, tss_phys_end);
                Halt();
            }
        }

        uint64_t sys_virt_base = sys_base;
        uint64_t sys_phys_base = virtual_to_physical(sys_virt_base);
        if (sys_phys_base) {
            uint64_t sys_phys_end = sys_phys_base + USER_STACK_PAGES * BLOCK_SIZE;
            if (alloc_phys_start < sys_phys_end && alloc_phys_end > sys_phys_base) {
                printf("[FATAL] Physical overlap: new phys 0x%lx-0x%lx "
                       "clashes with task %d syscall stack (virt 0x%lx-0x%lx -> phys 0x%lx-0x%lx)\n",
                       alloc_phys_start, alloc_phys_end, t->id,
                       sys_virt_base, sys_end, sys_phys_base, sys_phys_end);
                Halt();
            }
        }
    }

    spinlock_cnt_read_release(&TASK_LL_MODIFY);
}