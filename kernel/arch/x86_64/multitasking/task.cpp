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

SpinlockCnt TASK_LL_MODIFY = {0};

void task_attach_def_termios(Task *task) {
  memset(&task->term, 0, sizeof(termios));
  task->term.c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
  task->term.c_oflag = OPOST;
  task->term.c_cflag = CS8 | CREAD | CLOCAL;
  task->term.c_lflag = ECHO | ICANON | IEXTEN | ISIG;
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

Task *task_create(uint32_t id, uint64_t rip, bool kernel_task, uint64_t *pagedir,
                 uint32_t argc, char **argv) {
  spinlock_cnt_write_acquire(&TASK_LL_MODIFY);
  Task *browse = firstTask;
  while (browse) {
    if (!browse->next)
      break; // found final
    browse = browse->next;
  }
  if (!browse) {
    printf("[scheduler] Something went wrong with init!\n");
    Halt();
  }
  Task *target = (Task *)malloc(sizeof(Task));
  memset(target, 0, sizeof(Task));
  browse->next = target;
  spinlock_cnt_write_release(&TASK_LL_MODIFY);

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
  target->kernel_task = kernel_task;
  target->state = TASK_STATE_CREATED; // TASK_STATE_READY
  target->pagedir = pagedir;

  void  *tssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t tssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(tssRsp, 0, tssRspSize);
  target->whileTssRsp = (uint64_t)tssRsp + tssRspSize;

  void  *syscalltssRsp = virtual_allocate(USER_STACK_PAGES);
  size_t syscalltssRspSize = USER_STACK_PAGES * BLOCK_SIZE;
  memset(syscalltssRsp, 0, syscalltssRspSize);
  target->whileSyscallRsp = (uint64_t)syscalltssRsp + syscalltssRspSize;

  target->heap_start = USER_HEAP_START;
  target->heap_end = USER_HEAP_START;

  target->mmap_start = USER_MMAP_START;
  target->mmap_end = USER_MMAP_START;

  target->umask = S_IWGRP | S_IWOTH;

  memset(target->fpuenv, 0, 512);
  ((uint16_t *)target->fpuenv)[0] = 0x37f;
  target->mxcsr = 0x1f80;

  task_attach_def_termios(target);

  return target;
}

Task *task_create_kernel(uint64_t rip, uint64_t rdi) {
  Task *target = task_create(task_generate_id(), rip, true, page_directory_allocate(), 0, 0);

  stack_generate_kernel(target, rdi);
  task_create_finish(target);
  return target;
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

void task_kill(uint32_t id, uint16_t ret) {
  Task *task = task_get(id);
  if (!task)
    return;

  // We'll need this later
  bool parentVfork = task->parent->state == TASK_STATE_WAITING_VFORK;

  // Notify that poor parent... they must've been searching all over the
  // place!
  if (task->parent && !task->noInformParent) {
    spinlock_acquire(&task->parent->LOCK_CHILD_TERM);
    KilledInfo *info = (KilledInfo *)linked_list_allocate(
        (void **)(&task->parent->firstChildTerminated), sizeof(KilledInfo));
    info->pid = task->id;
    info->ret = ret;
    task->parent->childrenTerminatedAmnt++;
    if (task->parent->state == TASK_STATE_WAITING_CHILD ||
        (task->parent->state == TASK_STATE_WAITING_CHILD_SPECIFIC &&
         task->parent->waitingForPid == task->id))
      task->parent->state = TASK_STATE_READY;
    spinlock_release(&task->parent->LOCK_CHILD_TERM);
  }

  // vfork() children need to notify parents no matter what
  if (task->parent->state == TASK_STATE_WAITING_VFORK)
    task->parent->state = TASK_STATE_READY;

  // close any left open files
  OpenFile *file = task->firstFile;
  while (file) {
    int id = file->id;
    file = file->next;
    file_system_user_close(task, id);
  }

  spinlock_cnt_write_acquire(&TASK_LL_MODIFY);
  Task *browse = firstTask;
  while (browse) {
    if (browse->next && browse->next->id == task->id)
      break;
    browse = browse->next;
  }
  spinlock_cnt_write_release(&TASK_LL_MODIFY);

  if (!parentVfork)
    page_directory_free(task->pagedir);

  // tssRsp, syscalltssRsp left

  browse->next = task->next;

  task->state = TASK_STATE_DEAD;

  if (currentTask == task) {
    // we're most likely in a syscall context, so...
    // task_kill_cleanup(task); // left for sched
    asm volatile("sti");
    // wait until we're outta here
    while (1) {
      //   printf("GET ME OUT ");
    }
  }

  task_kill_cleanup(task);
}

void task_kill_cleanup(Task *task) {
  if (task->state != TASK_STATE_DEAD)
    return;

  return;
  virtual_free((void *)task->whileTssRsp, USER_STACK_PAGES);
  virtual_free((void *)task->whileSyscallRsp, USER_STACK_PAGES);
  free(task);

  task_free_children(task);
}

void task_free_children(Task *task) {
  Task *child = firstTask;
  while (child) {
    Task *next = child->next;
    if (child->parent == task && child->state != TASK_STATE_DEAD)
      child->parent = firstTask;
    child = next;
  }
}

void task_kill_children(Task *task) {
  Task *child = firstTask;
  while (child) {
    Task *next = child->next;
    if (child->parent == task && child->state != TASK_STATE_DEAD) {
      task_kill(child->id, 0);
      // task_kill_cleanup(child); // done automatically
      task_kill_children(task); // use recursion
    }
    child = next;
  }
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

int taskIdCurr = 1;

int16_t task_generate_id() {
  return taskIdCurr++;
  // spinlockCntReadAcquire(&TASK_LL_MODIFY);
  // Task    *browse = firstTask;
  // uint16_t max = 0;
  // while (browse) {
  //   if (browse->id > max)
  //     max = browse->id;
  //   browse = browse->next;
  // }

  // spinlockCntReadRelease(&TASK_LL_MODIFY);
  // return max + 1;
}

int task_change_cwd(char *newdir) {
  stat  stat = {0};
  char *safeNewdir = file_system_sanitize(currentTask->cwd, newdir);
  if (!file_system_stat_by_filename(currentTask, safeNewdir, &stat)) {
    free(safeNewdir);
    return -ENOENT;
  }

  if (!(stat.st_mode & S_IFDIR)) {
    free(safeNewdir);
    return -ENOTDIR;
  }

  size_t len = strlen(safeNewdir) + 1;
  currentTask->cwd = realloc(currentTask->cwd, len);
  memcpy(currentTask->cwd, safeNewdir, len);

  free(safeNewdir);
  return 0;
}

void task_files_empty(Task *task) {
  OpenFile *realFile = task->firstFile;
  while (realFile) {
    OpenFile *next = realFile->next;
    file_system_user_close(task, realFile->id);
    realFile = next;
  }
}

void task_files_copy(Task *original, Task *target, bool respectCOE) {
  OpenFile *realFile = original->firstFile;
  while (realFile) {
    if (respectCOE && realFile->closeOnExec) {
      realFile = realFile->next;
      continue;
    }
    OpenFile *targetFile = file_system_user_duplicate_node_unsafe(realFile);
    linkedlist_push_front_unsafe((void **)(&target->firstFile), targetFile);
    realFile = realFile->next;
  }
}

Task *task_fork(AsmPassedInterrupt *cpu, uint64_t rsp, bool copyPages,
               bool spinup) {
  spinlock_cnt_write_acquire(&TASK_LL_MODIFY);
  Task *browse = firstTask;
  while (browse) {
    if (!browse->next)
      break; // found final
    browse = browse->next;
  }
  if (!browse) {
    printf("[scheduler] Something went wrong with init!\n");
    Halt();
  }
  Task *target = (Task *)malloc(sizeof(Task));
  memset(target, 0, sizeof(Task));
  browse->next = target;
  spinlock_cnt_write_release(&TASK_LL_MODIFY);

  if (copyPages) {
    uint64_t *targetPagedir = page_directory_allocate();
    page_directory_user_duplicate(currentTask->pagedir, targetPagedir);
    target->pagedir = targetPagedir;
  } else
    target->pagedir = currentTask->pagedir;

  target->id = task_generate_id();
  target->pgid = currentTask->pgid;
  target->kernel_task = currentTask->kernel_task;
  target->state = TASK_STATE_CREATED;

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

  target->heap_start = currentTask->heap_start;
  target->heap_end = currentTask->heap_end;

  target->mmap_start = currentTask->mmap_start;
  target->mmap_end = currentTask->mmap_end;

  target->term = currentTask->term;

  target->tmpRecV = currentTask->tmpRecV;
  target->firstFile = 0;
  size_t cmwdLen = strlen(currentTask->cwd) + 1;
  char  *newcwd = (char *)malloc(cmwdLen);
  memcpy(newcwd, currentTask->cwd, cmwdLen);
  target->cwd = newcwd;
  target->umask = currentTask->umask;

  task_files_copy(currentTask, target, false);

  // returns zero yk
  target->registers.rax = 0;

  // etc (https://www.felixcloutier.com/x86/syscall)
  target->registers.rip = cpu->rcx;
  target->registers.cs = GDT_USER_CODE | DPL_USER;
  target->registers.ds = GDT_USER_DATA | DPL_USER;
  target->registers.rflags = cpu->r11;
  target->registers.usermode_rsp = rsp;
  target->registers.usermode_ss = GDT_USER_DATA | DPL_USER;

  // yk
  target->parent = currentTask;

  // fpu stuff
  memcpy(target->fpuenv, currentTask->fpuenv, 512);
  target->mxcsr = currentTask->mxcsr;

  if (spinup)
    task_create_finish(target);

  return target;
}

void kernel_dummy_entry() {
  while (true)
    dummyTask->state = TASK_STATE_DUMMY;
}

void tasks_initialize() {
  firstTask = (Task *)malloc(sizeof(Task));
  memset(firstTask, 0, sizeof(Task));

  currentTask = firstTask;
  currentTask->id = KERNEL_TASK_ID;
  currentTask->state = TASK_STATE_READY;
  currentTask->pagedir = get_page_directory();
  currentTask->kernel_task = true;
  currentTask->cwd = malloc(2);
  currentTask->cwd[0] = '/';
  currentTask->cwd[1] = '\0';

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
}