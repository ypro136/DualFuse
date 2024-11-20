#include <paging.h>

#include <types.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <data_structures/bitmap.h>
#include <bootloader.h>
#include <limine.h>
#include <liballoc.h>
#include <task.h>
#include <utility.h>
#include <pmm.h>
#include <vmm.h>
#include <hcf.hpp>



// A small note: When mapping or doing other operations, there is a
// chance that the respective page layer (pml4, pdp, pd, pt, etc) is using
// full-size entries (by setting the appropriate flag)! Although I dislike using
// those, I should be keeping this in mind, in case I ever do otherwise. Limine
// HHDM regions shouldn't cause any issues, since I purposefully avoid mapping
// or modifying memory mappings close to said regions...

#define PAGING_DEBUG 0

#define HHDMoffset (bootloader.hhdmOffset)
uint64_t *globalPagedir = 0;

void paging_initialize() {
  
  // I will keep on using limine's for the time being
  uint64_t pdPhys = 0;
  asm volatile("movq %%cr3,%0" : "=r"(pdPhys));
  if (!pdPhys) {
    printf("[paging] Could not parse default pagedir!\n");
    Halt();
  }

  uint64_t pdVirt = pdPhys + bootloader.hhdmOffset;
  globalPagedir = (uint64_t *)pdVirt;

  // VirtualSeek(bootloader.hhdmOffset);
}

void virtual_map_region_by_length(uint64_t virt_addr, uint64_t phys_addr,
                              uint64_t length, uint64_t flags) {
#if ELF_DEBUG
  printf("[paging::map::region] virt{%lx} phys{%lx} len{%lx}\n", virt_addr,
         phys_addr, length);
#endif
  uint32_t pagesAmnt = CEILING_DIVISION(length, PAGE_SIZE);
  for (int i = 0; i < pagesAmnt; i++) {
    uint64_t xvirt = virt_addr + i * PAGE_SIZE;
    uint64_t xphys = phys_addr + i * PAGE_SIZE;
    virtual_map(xvirt, xphys, flags);
  }
}

// Will NOT check for the current task and update it's pagedir (on the struct)!
void change_page_directory_unsafe(uint64_t *pd) {
  uint64_t targ = paging_virtual_to_physical((size_t)pd);
  if (!targ) {
    printf("[paging] Could not change to pd{%lx}!\n", pd);
    Halt();
  }
  asm volatile("movq %0, %%cr3" ::"r"(targ));

  globalPagedir = pd;
}

// Used by the scheduler to avoid accessing globalPagedir directly
void change_page_directory_fake(uint64_t *pd) {
  if (!virtual_to_physical((size_t)pd)) {
    printf("[paging] Could not (fake) change to pd{%lx}!\n", pd);
    Halt();
  }

  globalPagedir = pd;
}

void change_page_directory(uint64_t *pd) 
{

  if (tasksInitiated){

      currentTask->pagedir = pd;
  }
  
  change_page_directory_unsafe(pd);
}

uint64_t *get_page_directory() { return (uint64_t *)globalPagedir; }

void invalidate(uint64_t vaddr) { asm volatile("invlpg %0" ::"m"(vaddr)); }

size_t paging_physical_allocate() {
  size_t phys = physical_allocate(1);

  void *virt = (void *)(phys + HHDMoffset);
  memset(virt, 0, PAGE_SIZE);

  return phys;
}

SpinlockCnt WLOCK_PAGING = {0};

void virtual_map(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
  virtual_mapL(globalPagedir, virt_addr, phys_addr, flags);
}

void virtual_mapL(uint64_t *pagedir, uint64_t virt_addr, uint64_t phys_addr,
                 uint64_t flags) {
  if (virt_addr % PAGE_SIZE) {
    printf("[paging] Tried to map non-aligned address! virt{%lx} phys{%lx}\n",
           virt_addr, phys_addr);
    Halt();
  }
  virt_addr = AMD64_MM_STRIPSX(virt_addr);

  uint32_t pml4_index = PML4E(virt_addr);
  uint32_t pdp_index = PDPTE(virt_addr);
  uint32_t pd_index = PDE(virt_addr);
  uint32_t pt_index = PTE(virt_addr);

  spinlock_cnt_write_acquire(&WLOCK_PAGING);
  if (!(pagedir[pml4_index] & PF_PRESENT)) {
    size_t target = paging_physical_allocate();
    pagedir[pml4_index] = target | PF_PRESENT | PF_RW | PF_USER;
  }
  size_t *pdp = (size_t *)(PTE_GET_ADDR(pagedir[pml4_index]) + HHDMoffset);

  if (!(pdp[pdp_index] & PF_PRESENT)) {
    size_t target = paging_physical_allocate();
    pdp[pdp_index] = target | PF_PRESENT | PF_RW | PF_USER;
  }
  size_t *pd = (size_t *)(PTE_GET_ADDR(pdp[pdp_index]) + HHDMoffset);

  if (!(pd[pd_index] & PF_PRESENT)) {
    size_t target = paging_physical_allocate();
    pd[pd_index] = target | PF_PRESENT | PF_RW | PF_USER;
  }
  size_t *pt = (size_t *)(PTE_GET_ADDR(pd[pd_index]) + HHDMoffset);

  if (pt[pt_index] & PF_PRESENT) {
    physical_free(PTE_GET_ADDR(pt[pt_index]), 1);
    // printf("[paging] Overwrite (without unmapping) WARN! virt{%lx}
    // phys{%lx}\n",
    //        virt_addr, phys_addr);
  }
  pt[pt_index] = (P_PHYS_ADDR(phys_addr)) | PF_PRESENT | flags; // | PF_RW

  invalidate(virt_addr);
  spinlock_cnt_write_release(&WLOCK_PAGING);
#if ELF_DEBUG
  printf("[paging] Mapped virt{%lx} to phys{%lx}\n", virt_addr, phys_addr);
#endif
}

size_t paging_virtual_to_physical(size_t virt_addr) {
  if (!globalPagedir)
    return 0;

  if (virt_addr >= HHDMoffset && virt_addr <= (HHDMoffset + bootloader.mmTotal))
    return virt_addr - HHDMoffset;

  size_t virt_addr_init = virt_addr;
  virt_addr &= ~0xFFF;

  virt_addr = AMD64_MM_STRIPSX(virt_addr);

  uint32_t pml4_index = PML4E(virt_addr);
  uint32_t pdp_index = PDPTE(virt_addr);
  uint32_t pd_index = PDE(virt_addr);
  uint32_t pt_index = PTE(virt_addr);

  spinlock_cnt_read_acquire(&WLOCK_PAGING);
  if (!(globalPagedir[pml4_index] & PF_PRESENT))
  {
      spinlock_cnt_read_release(&WLOCK_PAGING);
      return 0;
  }
  /*else if (globalPagedir[pml4_index] & PF_PRESENT &&
           globalPagedir[pml4_index] & PF_PS)
    return (void *)(PTE_GET_ADDR(globalPagedir[pml4_index] +
                                 (virt_addr & PAGE_MASK(12 + 9 + 9 + 9))));*/
  size_t *pdp = (size_t *)(PTE_GET_ADDR(globalPagedir[pml4_index]) + HHDMoffset);

  if (!(pdp[pdp_index] & PF_PRESENT))
  {
      spinlock_cnt_read_release(&WLOCK_PAGING);
      return 0;
  }
  /*else if (pdp[pdp_index] & PF_PRESENT && pdp[pdp_index] & PF_PS)
    return (void *)(PTE_GET_ADDR(pdp[pdp_index] +
                                 (virt_addr & PAGE_MASK(12 + 9 + 9))));*/
  size_t *pd = (size_t *)(PTE_GET_ADDR(pdp[pdp_index]) + HHDMoffset);

  if (!(pd[pd_index] & PF_PRESENT))
  {
      spinlock_cnt_read_release(&WLOCK_PAGING);
      return 0;
  }

  /*else if (pd[pd_index] & PF_PRESENT && pd[pd_index] & PF_PS)
    return (
        void *)(PTE_GET_ADDR(pd[pd_index] + (virt_addr & PAGE_MASK(12 + 9))));*/
  size_t *pt = (size_t *)(PTE_GET_ADDR(pd[pd_index]) + HHDMoffset);

  if (pt[pt_index] & PF_PRESENT) {
    spinlock_cnt_read_release(&WLOCK_PAGING);
    return (size_t)(PTE_GET_ADDR(pt[pt_index]) +
                    ((size_t)virt_addr_init & 0xFFF));
  }

//error
  spinlock_cnt_read_release(&WLOCK_PAGING);
  return 0;
}

uint32_t virtual_unmap(uint32_t virt_addr) {
  // not used anywhere yet
  return 0;
}

uint64_t *page_directory_allocate() {
  if (!tasksInitiated) {
    printf("[paging] FATAL! Tried to allocate pd without tasks initiated!\n");
    Halt();
  }
  uint64_t *out = virtual_allocate(1);

  memset(out, 0, PAGE_SIZE);

  uint64_t *model = task_get(KERNEL_TASK_ID)->pagedir;
  for (int i = 0; i < 512; i++)
    out[i] = model[i];

  return out;
}

// todo: clear orphans after a whole page level is emptied!
// destroys any userland stuff on the page directory
void page_directory_free(uint64_t *page_dir) {
  spinlock_cnt_write_acquire(&WLOCK_PAGING);

  for (int pml4_index = 0; pml4_index < 512; pml4_index++) {
    if (!(page_dir[pml4_index] & PF_PRESENT) || page_dir[pml4_index] & PF_PS)
      continue;
    size_t *pdp = (size_t *)(PTE_GET_ADDR(page_dir[pml4_index]) + HHDMoffset);

    for (int pdp_index = 0; pdp_index < 512; pdp_index++) {
      if (!(pdp[pdp_index] & PF_PRESENT) || pdp[pdp_index] & PF_PS)
        continue;
      size_t *pd = (size_t *)(PTE_GET_ADDR(pdp[pdp_index]) + HHDMoffset);

      for (int pd_index = 0; pd_index < 512; pd_index++) {
        if (!(pd[pd_index] & PF_PRESENT) || pd[pd_index] & PF_PS)
          continue;
        size_t *pt = (size_t *)(PTE_GET_ADDR(pd[pd_index]) + HHDMoffset);

        for (int pt_index = 0; pt_index < 512; pt_index++) {
          if (!(pt[pt_index] & PF_PRESENT) || pt[pt_index] & PF_PS)
            continue;

          // we only free mappings related to userland (ones from ELF)
          if (!(pt[pt_index] & PF_USER))
            continue;

          uint64_t phys = PTE_GET_ADDR(pt[pt_index]);
          physical_free(phys, 1);
        }
      }
    }
  }

  spinlock_cnt_write_release(&WLOCK_PAGING);
}

void page_directory_user_duplicate(uint64_t *source, uint64_t *target) {
  spinlock_cnt_read_acquire(&WLOCK_PAGING);
  for (int pml4_index = 0; pml4_index < 512; pml4_index++) {
    if (!(source[pml4_index] & PF_PRESENT) || source[pml4_index] & PF_PS)
      continue;
    size_t *pdp = (size_t *)(PTE_GET_ADDR(source[pml4_index]) + HHDMoffset);

    for (int pdp_index = 0; pdp_index < 512; pdp_index++) {
      if (!(pdp[pdp_index] & PF_PRESENT) || pdp[pdp_index] & PF_PS)
        continue;
      size_t *pd = (size_t *)(PTE_GET_ADDR(pdp[pdp_index]) + HHDMoffset);

      for (int pd_index = 0; pd_index < 512; pd_index++) {
        if (!(pd[pd_index] & PF_PRESENT) || pd[pd_index] & PF_PS)
          continue;
        size_t *pt = (size_t *)(PTE_GET_ADDR(pd[pd_index]) + HHDMoffset);

        for (int pt_index = 0; pt_index < 512; pt_index++) {
          if (!(pt[pt_index] & PF_PRESENT) || pt[pt_index] & PF_PS)
            continue;

          // we only duplicate mappings related to userland (ones from ELF)
          if (!(pt[pt_index] & PF_USER))
            continue;

          size_t physSource = PTE_GET_ADDR(pt[pt_index]);
          size_t physTarget =
              (pt[pt_index] & PF_SHARED) ? physSource : paging_physical_allocate();

          size_t virt =
              BITS_TO_VIRT_ADDR(pml4_index, pdp_index, pd_index, pt_index);

          void *ptrSource = (void *)(physSource + HHDMoffset);
          void *ptrTarget = (void *)(physTarget + HHDMoffset);

          memcpy(ptrTarget, ptrSource, PAGE_SIZE);

          spinlock_cnt_read_release(&WLOCK_PAGING);
          virtual_mapL(target, virt, physTarget, PF_RW | PF_USER);
          spinlock_cnt_read_acquire(&WLOCK_PAGING);
        }
      }
    }
  }

  spinlock_cnt_read_release(&WLOCK_PAGING);
}
