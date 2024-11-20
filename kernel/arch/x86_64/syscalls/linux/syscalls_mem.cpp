#include <linux.h>
#include <paging.h>
#include <syscalls.h>
#include <task.h>
#include <utility.h>

#define SYSCALL_MMAP 9
static uint64_t syscallMmap(size_t addr, size_t length, int prot, int flags,
                            int fd, size_t pgoffset) {
  if (length == 0)
    return -EINVAL;

  length = CEILING_DIVISION(length, 0x1000) * 0x1000;
  /* No point in DEBUG_SYSCALLS_ARGS'ing here */
  if (!addr)
    flags &= ~MAP_FIXED;

  if (flags & MAP_FIXED && flags & MAP_ANONYMOUS) {
    size_t pages = CEILING_DIVISION(length, PAGE_SIZE);

    for (int i = 0; i < pages; i++)
      virtual_map(addr + i * PAGE_SIZE, physical_allocate(1), PF_RW | PF_USER);

    memset((void *)addr, 0, pages * PAGE_SIZE);
    return addr;
  }

  if (!addr && fd == -1 &&
      (flags & ~MAP_FIXED & ~MAP_PRIVATE) ==
          MAP_ANONYMOUS) { // before: !addr &&
    size_t curr = currentTask->mmap_end;
#if DEBUG_SYSCALLS_EXTRA
    printf("[syscalls::mmap] No placement preference, no file descriptor: "
           "addr{%lx} length{%lx}\n",
           curr, length);
#endif
    task_adjust_heap(currentTask, currentTask->mmap_end + length,
                   &currentTask->mmap_start, &currentTask->mmap_end);
    memset((void *)curr, 0, length);
#if DEBUG_SYSCALLS_EXTRA
    printf("[syscalls::mmap] Found addr{%lx}\n", curr);
#endif
    return curr;
  } else if (!addr && fd == -1 &&
             (flags & ~MAP_FIXED & ~MAP_PRIVATE & ~MAP_SHARED) ==
                 MAP_ANONYMOUS &&
             (flags & ~MAP_FIXED & ~MAP_PRIVATE & ~MAP_ANONYMOUS) ==
                 MAP_SHARED) {
    printf("[syscalls::mmap] FATAL! Shared memory is unstable asf!\n");
    Halt();
    size_t base = currentTask->mmap_end;
    size_t pages = CEILING_DIVISION(length, PAGE_SIZE);
    currentTask->mmap_end += pages * PAGE_SIZE;

    for (int i = 0; i < pages; i++)
      virtual_map(base + i * PAGE_SIZE, physical_allocate(1),
                 PF_RW | PF_USER | PF_SHARED);

    return base;
  } else if (fd != -1) {
    OpenFile *file = file_system_user_get_node(currentTask, fd);
    if (!file)
      return -1;

    if (!file->handlers->mmap)
      return -1;
    return file->handlers->mmap(addr, length, prot, flags, file, pgoffset);
  }

#if DEBUG_SYSCALLS_STUB
  printf(
      "[syscalls::mmap] UNIMPLEMENTED! addr{%lx} len{%lx} prot{%d} flags{%x} "
      "fd{%d} "
      "pgoffset{%x}\n",
      addr, length, prot, flags, fd, pgoffset);
#endif

  return -1;
}

#define SYSCALL_MUNMAP 11
static int syscallMunmap(uint64_t addr, size_t len) {
  // todo
  return 0;
}

#define SYSCALL_BRK 12
static uint64_t syscallBrk(uint64_t brk) {
  if (!brk)
    return currentTask->heap_end;

  if (brk < currentTask->heap_end) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::brk] FAIL! Tried to go inside heap limits! brk{%lx} "
           "limit{%lx}\n",
           brk, currentTask->heap_end);
#endif
    return -1;
  }

  task_adjust_heap(currentTask, brk, &currentTask->heap_start,
                 &currentTask->heap_end);

  return currentTask->heap_end;
}

void syscallRegMem() {
  register_syscall(SYSCALL_MMAP, syscallMmap);
  register_syscall(SYSCALL_MUNMAP, syscallMunmap);
  register_syscall(SYSCALL_BRK, syscallBrk);
}