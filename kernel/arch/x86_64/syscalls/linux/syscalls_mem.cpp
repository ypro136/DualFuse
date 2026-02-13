#include <linux.h>
#include <paging.h>
#include <vmm.h>
#include <syscalls.h>
#include <task.h>
#include <utility.h>

#define SYSCALL_MMAP 9
static uint64_t syscallMmap(size_t addr, size_t length, int prot, int flags,
                            int fd, size_t pgoffset) {
  if (length == 0 || (addr % PAGE_SIZE) != 0)
    return ERR(EINVAL);

  length = CEILING_DIVISION(length, 0x1000) * 0x1000;
  /* No point in DEBUG_SYSCALLS_ARGS'ing here */
  if (!addr)
    flags &= ~MAP_FIXED;

  if (flags & MAP_FIXED && flags & MAP_ANONYMOUS) {
    size_t pages = CEILING_DIVISION(length, PAGE_SIZE);

    spinlock_acquire(&currentTask->infoPd->LOCK_PD);
    size_t end = addr + pages * PAGE_SIZE;
    if (end > currentTask->infoPd->mmap_end)
      currentTask->infoPd->mmap_end = end;
    spinlock_release(&currentTask->infoPd->LOCK_PD);

    for (int i = 0; i < pages; i++)
      virtual_map(addr + i * PAGE_SIZE, physical_allocate(1), PF_RW | PF_USER);

    memset((void *)addr, 0, pages * PAGE_SIZE);
    return addr;
  }

  if (!addr && fd == -1 &&
      (flags & ~MAP_FIXED & ~MAP_PRIVATE) ==
          MAP_ANONYMOUS) { // before: !addr &&
    spinlock_acquire(&currentTask->infoPd->LOCK_PD);
    size_t curr = currentTask->infoPd->mmap_end;
    task_adjust_heap(currentTask, currentTask->infoPd->mmap_end + length,
                   &currentTask->infoPd->mmap_start,
                   &currentTask->infoPd->mmap_end);
    spinlock_release(&currentTask->infoPd->LOCK_PD);
    memset((void *)curr, 0, length);
    return curr;
  } else if (!addr && fd == -1 &&
             (flags & ~MAP_FIXED & ~MAP_PRIVATE & ~MAP_SHARED) ==
                 MAP_ANONYMOUS &&
             (flags & ~MAP_FIXED & ~MAP_PRIVATE & ~MAP_ANONYMOUS) ==
                 MAP_SHARED) {
    printf("[syscalls::mmap] FATAL! Shared memory is unstable asf!\n");
    Halt();
    /*size_t base = currentTask->mmap_end;
    size_t pages = CEILING_DIVISION(length, PAGE_SIZE);
    currentTask->mmap_end += pages * PAGE_SIZE;

    for (int i = 0; i < pages; i++)
      virtual_map(base + i * PAGE_SIZE, physical_allocate(1),
                 PF_RW | PF_USER | PF_SHARED);

    return base;*/
  } else if (fd != -1) {
    OpenFile *file = fsUserGetNode(currentTask, fd);
    if (!file)
      return -1;

    if (!file->handlers->mmap)
      return -1;
    spinlock_acquire(&file->LOCK_OPERATIONS);
    size_t ret =
        file->handlers->mmap(addr, length, prot, flags, file, pgoffset);
    spinlock_release(&file->LOCK_OPERATIONS);
    return ret;
  }

  dbgSysStubf("dead end");
  return -1;
}

#define SYSCALL_MPROTECT 10
static size_t syscallMprotect(uint64_t start, uint64_t len, uint64_t prot) {
  // todo
  return 0;
}

#define SYSCALL_MUNMAP 11
static size_t syscallMunmap(uint64_t addr, size_t len) {
  if ((addr % PAGE_SIZE) != 0 || !len)
    return ERR(EINVAL);

  spinlock_acquire(&currentTask->infoPd->LOCK_PD);
  bool insideBounds = addr >= currentTask->infoPd->mmap_start &&
                      (addr + len) <= currentTask->infoPd->mmap_end;
  spinlock_release(&currentTask->infoPd->LOCK_PD);
  if (!insideBounds)
    return ERR(EINVAL);

  size_t pages = CEILING_DIVISION(len, PAGE_SIZE);
  for (size_t i = 0; i < pages; i++) {
    size_t phys = virtual_to_physical(addr + i * PAGE_SIZE);
    if (!phys)
      continue;
    // printf("%lx %lx\n", addr + i * PAGE_SIZE, phys);
    virtual_map(addr + i * PAGE_SIZE, 0, PF_USER);
    // PhysicalFree(phys, 1); will be done by ^
  }
  return 0;
}

#define SYSCALL_BRK 12
static uint64_t syscallBrk(uint64_t brk) {
  size_t ret = 0;
  spinlock_acquire(&currentTask->infoPd->LOCK_PD);

  if (!brk) {
    ret = currentTask->infoPd->heap_end;
    goto cleanup;
  }

  if (brk < currentTask->infoPd->heap_end) {
    dbgSysFailf("inside heap limits");
    ret = -1;
    goto cleanup;
  }

  task_adjust_heap(currentTask, brk, &currentTask->infoPd->heap_start,
                 &currentTask->infoPd->heap_end);

  ret = currentTask->infoPd->heap_end;
cleanup:
  spinlock_release(&currentTask->infoPd->LOCK_PD);
  return ret;
}

void syscallRegMem() {
  registerSyscall(SYSCALL_MMAP, syscallMmap);
  registerSyscall(SYSCALL_MUNMAP, syscallMunmap);
  registerSyscall(SYSCALL_MPROTECT, syscallMprotect);
  registerSyscall(SYSCALL_BRK, syscallBrk);
}