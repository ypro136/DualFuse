 #include <types.h>


#ifndef PAGING_H
#define PAGING_H

// Page [*] flags
#define PF_PRESENT (1 << 0) // Page is present in the table
#define PF_RW (1 << 1)      // Read-write
#define PF_USER (1 << 2)    // User-mode (CPL==3) access allowed
#define PF_PWT (1 << 3)     // Page write-thru
#define PF_PCD (1 << 4)     // Cache disable
#define PF_ACCESS (1 << 5)  // Indicates whether page was accessed
#define PF_DIRTY (1 << 6)   // Indicates whether 4K page was written
#define PF_PS (1 << 7)      // Page size (valid for PD and PDPT only)
#define PF_PAT (1 << 7)     // Page Attribute Table (valid for PT only)
#define PF_GLOBAL (1 << 8)  // Indicates the page is globally cached
#define PF_SHARED (1 << 9)  // Userland page is shared
// #define PF_SYSTEM (1 << 9)  // Page used by the kernel

// Region caching (following the Limine protocol)
#define PF_CACHE_WC (PF_PAT | PF_PWT)

// Virtual address' bitmasks and shifts
#define PGSHIFT_PML4E 39
#define PGSHIFT_PDPTE 30
#define PGSHIFT_PDE 21
#define PGSHIFT_PTE 12
#define PGMASK_ENTRY 0x1ff
#define PGMASK_OFFSET 0x3ff

#define BITS_TO_VIRT_ADDR(pml4_index, pdpte_index, pd_index, pt_index)         \
  (((uint64_t)pml4_index << PGSHIFT_PML4E) |                                   \
   ((uint64_t)pdpte_index << PGSHIFT_PDPTE) |                                  \
   ((uint64_t)pd_index << PGSHIFT_PDE) | ((uint64_t)pt_index << PGSHIFT_PTE))

// Workaround repeated characters
#define AMD64_MM_STRIPSX(a) ((uintptr_t)(a) & 0xFFFFFFFFFFFF)
#define AMD64_MM_ADDRSX(a)                                                     \
  (((uintptr_t)(a) & (1ULL << 47)) ? (0xFFFFFF0000000000 | ((uintptr_t)(a)))   \
                                   : ((uintptr_t)(a)))

// Virtual address' macros
#define PML4E(a) (((a) >> PGSHIFT_PML4E) & PGMASK_ENTRY)
#define PDPTE(a) (((a) >> PGSHIFT_PDPTE) & PGMASK_ENTRY)
#define PDE(a) (((a) >> PGSHIFT_PDE) & PGMASK_ENTRY)
#define PTE(a) (((a) >> PGSHIFT_PTE) & PGMASK_ENTRY)

#define PTE_ADDR_MASK 0x000ffffffffff000
#define PTE_GET_ADDR(VALUE) ((VALUE) & PTE_ADDR_MASK)
#define PTE_GET_FLAGS(VALUE) ((VALUE) & ~PTE_ADDR_MASK)

#define PAGE_MASK(x) ((1 << (x)) - 1)

// Sizes & lengths
#define USER_STACK_PAGES (0x30)
#define PAGE_SIZE 0x1000
#define PAGE_SIZE_LARGE 0x200000
#define PAGE_SIZE_HUGE 0x40000000

// Dummy pagefault system (see system.h)
#define SCHED_PAGE_FAULT_MAGIC_ADDRESS 0x5FFFFFFFF000

// Processes' heap & stack locations
#define USER_MMAP_START 0x700000000000
#define USER_HEAP_START 0x600000000000
#define USER_STACK_BOTTOM 0x800000000000

#define P_PHYS_ADDR(x) ((x) & ~0xFFF)

void paging_initialize();

void virtual_mapL(uint64_t *pagedir, uint64_t virt_addr, uint64_t phys_addr,
                 uint64_t flags);
void virtual_map(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
// uint32_t virtual_unmap(uint32_t virt_addr);
size_t paging_virtual_to_physical(size_t virt_addr);

uint64_t *get_page_directory();
void      change_page_directory(uint64_t *pd);
void      change_page_directory_unsafe(uint64_t *pd);
void      change_page_directory_fake(uint64_t *pd);

uint64_t *page_directory_allocate();
void      page_directory_free(uint64_t *page_dir);

void page_directory_user_duplicate(uint64_t *source, uint64_t *target);

void invalidate(uint64_t vaddr);

#endif