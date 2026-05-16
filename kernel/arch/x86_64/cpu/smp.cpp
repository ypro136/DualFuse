#include <ap_trampoline_blob.h>
#include <apic.h>
#include <bootloader.h>
#include <gdt.h>
#include <idt.h>
#include <liballoc.h>
#include <paging.h>
#include <system.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <gdt.h>
#include <scheduler.h>
#include <task.h>
#include <spinlock.h>


#define AP_STACK_SIZE 0x4000

static uint8_t *trampoline_header = nullptr;
static volatile uint32_t ap_running = 0;
static volatile uint8_t bsp_done = 0;

static uint8_t ap_stacks[256][AP_STACK_SIZE] __attribute__((aligned(16)));


static Spinlock print_lock = {0};
static void ap_debug(const char* msg, uint64_t val) {
    spinlock_acquire(&print_lock);
    printf("[smp] %s 0x%lx\n", msg, val);
    spinlock_release(&print_lock);
}

static Spinlock ap_init_lock = {0};

extern "C" void ap_entry_c(uint64_t kernel_stack_top) {
    uint32_t my_lapic_id;
    asm volatile("mov $1, %%eax; cpuid; shrl $24, %%ebx" : "=b"(my_lapic_id));
    __atomic_fetch_add(&ap_running, 1, __ATOMIC_SEQ_CST);
 
    smp_ap_initialize_gdt_and_tss(kernel_stack_top);

    while (!bsp_done)
    asm volatile("pause");

    smpInitiateAPIC();

    Task* my_idle_task = per_lapic_core_current_task[my_lapic_id];
    if (!my_idle_task) {
        ap_debug("AP has no pre-created idle task, LAPIC", my_lapic_id);
        Halt();
    }

    // Update TSS so interrupts use the idle task’s kernel stack
    per_core_tss[my_lapic_id]->rsp0 = my_idle_task->whileTssRsp;

    scheduler_lapic_timer_start_on_current_ap();

    ap_debug("AP armed and jumping, LAPIC", my_lapic_id);

    uint64_t idle_rsp = my_idle_task->whileTssRsp;
    asm volatile(
        "mov %0, %%rsp\n\t"
        "jmp *%1"
        :
        : "m"(idle_rsp), "r"((uint64_t)kernel_ap_idle_entry)
        : "memory");
}

static void clear_nx_bit(uintptr_t physical_addr) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)((cr3 & 0x7FFFFFFFFFFFF000) + bootloader.hhdmOffset);
    // Walk the same four levels and clear bit 63 on the final entry.
    // (Simplified: we assume 4‑level paging and 4 KiB page)
    int pml4_idx = (physical_addr >> 39) & 0x1FF;
    if (!(pml4[pml4_idx] & 1)) return;
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_idx] & 0x7FFFFFFFFFFFF000) + bootloader.hhdmOffset);
    int pdpt_idx = (physical_addr >> 30) & 0x1FF;
    if (!(pdpt[pdpt_idx] & 1)) return;
    if (pdpt[pdpt_idx] & (1 << 7)) {
        pdpt[pdpt_idx] &= ~(1ULL << 63);   // clear NX
        asm volatile("invlpg (%0)" :: "r"(physical_addr));
        return;
    }
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_idx] & 0x7FFFFFFFFFFFF000) + bootloader.hhdmOffset);
    int pd_idx = (physical_addr >> 21) & 0x1FF;
    if (!(pd[pd_idx] & 1)) return;
    if (pd[pd_idx] & (1 << 7)) {
        pd[pd_idx] &= ~(1ULL << 63);
        asm volatile("invlpg (%0)" :: "r"(physical_addr));
        return;
    }
    uint64_t *pt = (uint64_t *)((pd[pd_idx] & 0x7FFFFFFFFFFFF000) + bootloader.hhdmOffset);
    int pt_idx = (physical_addr >> 12) & 0x1FF;
    pt[pt_idx] &= ~(1ULL << 63);
    asm volatile("invlpg (%0)" :: "r"(physical_addr));
}

void smp_install_trampoline() {
    clear_nx_bit(0x8000);
#if defined(DEBUG_SMP)
    printf("[smp] Installing trampoline...\n");
#endif

    // Unconditionally identity‑map the first 1 MiB.
    for (uintptr_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        virtual_map(addr, addr, PF_PRESENT | PF_RW);
        tlb_shootdown_all();
    }

    // Also map the local APIC MMIO region (needed later by the AP).
    virtual_map(0xFEE00000, 0xFEE00000, PF_PRESENT | PF_RW);
    tlb_shootdown_all();

    uint8_t *dest = (uint8_t *)(bootloader.hhdmOffset + 0x8000);
    memcpy(dest, obj_ap_trampoline_bin, obj_ap_trampoline_bin_len);
    trampoline_header = dest + obj_ap_trampoline_bin_len - 49;

    // Verify identity mapping by reading through the virtual address directly
    volatile uint8_t *test = (volatile uint8_t *)0x8000;
    *test = 0xA5;
    if (*test != 0xA5) {
        printf("[smp] ERROR: identity mapping at 0x8000 failed!\n");
    } else {
#if defined(DEBUG_SMP)
        printf("[smp] identity mapping at 0x8000 works\n");
#endif
    }

    printf("[smp] trampoline installed at 0x8000\n");
}

static void apic_send_init(uint32_t apic_id) {
#if defined(DEBUG_SMP)
    printf("[smp] send INIT to APIC ID %u, x2apic=%d\n", apic_id, x2apic_mode);
#endif
    if (x2apic_mode) {
        uint64_t icr = ((uint64_t)apic_id << 32) | 0x0000C500;
        wrmsr(0x830, icr);
        while (rdmsr(0x830) & (1ULL << 12)) asm volatile("pause");
        icr = ((uint64_t)apic_id << 32) | 0x00004500;
        wrmsr(0x830, icr);
        while (rdmsr(0x830) & (1ULL << 12)) asm volatile("pause");
    } else {
        uint32_t icr_high = apic_id << 24;
        apicWrite(0x310, icr_high);
        apicWrite(0x300, 0x0000C500);
        while (apicRead(0x300) & (1 << 12)) asm volatile("pause");
        apicWrite(0x310, icr_high);
        apicWrite(0x300, 0x00004500);
        while (apicRead(0x300) & (1 << 12)) asm volatile("pause");
    }
}

static void apic_send_sipi(uint32_t apic_id, uint8_t vector) {
#if defined(DEBUG_SMP)
    printf("[smp] send SIPI to APIC ID %u, vector 0x%x\n", apic_id, vector);
#endif
    uint32_t cmd = 0x00004600 | vector;
    if (x2apic_mode) {
        uint64_t icr = ((uint64_t)apic_id << 32) | cmd;
        wrmsr(0x830, icr);
        while (rdmsr(0x830) & (1ULL << 12)) asm volatile("pause");
    } else {
        apicWrite(0x310, apic_id << 24);
        apicWrite(0x300, cmd);
        while (apicRead(0x300) & (1 << 12)) asm volatile("pause");
    }
}

static void simple_mdelay(unsigned ms) {
    for (unsigned i = 0; i < ms * 10000; ++i)
        asm volatile("pause");
}

static bool ap_wait_for_stage(volatile uint8_t *progress, uint8_t expected,
                              volatile uint8_t *go, uint8_t go_value,
                              unsigned timeout_iterations) {
    for (unsigned i = 0; i < timeout_iterations; i++) {
        if (*progress == expected) {
            *go = go_value;
            return true;
        }
        asm volatile("pause");
    }
    return false;
}

// ---------- Helper for staged handshake ----------
#define HANDSHAKE_STAGES 12
static const char *stage_names[HANDSHAKE_STAGES] = {
    "16-bit entry",
    "PICs disabled",
    "temp GDT loaded",
    "protected mode entered",
    "PAE enabled",
    "CR3 loaded",
    "EFER.LME set",
    "before paging enable",
    "LAPIC masked",
    "paging just enabled",      // new
    "paging enabled (pre-jump)",
    "64-bit mode entered"
};

static const uint8_t stage_markers[HANDSHAKE_STAGES] = {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
    0xA5, 0xA6, 0xAA, 0xAB, 0xAC, 0xA7, 0xA8
};

// In smp_boot_all_aps(), replace the loop with:
void smp_boot_all_aps() {
    debug_page_table_entry(0x8000);

    printf("[smp] Starting APs sequentially...\n");
    
    uint64_t cr3_phys;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_phys));
    *(uint64_t *)(trampoline_header + 0)  = cr3_phys;
    printf("[smp] Header CR3 = 0x%lx%08lx\n",
       (unsigned long)(cr3_phys >> 32), (unsigned long)(cr3_phys & 0xFFFFFFFF));
    *(uint64_t *)(trampoline_header + 16) = (uint64_t)&ap_entry_c;

    GDTPtr gdt_ptr, idt_ptr;
    asm volatile("sgdt %0" : "=m"(gdt_ptr));
    *(uint16_t *)(trampoline_header + 24) = gdt_ptr.limit;
    *(uint64_t *)(trampoline_header + 26) = gdt_ptr.base;

    asm volatile("sidt %0" : "=m"(idt_ptr));
    *(uint16_t *)(trampoline_header + 34) = idt_ptr.limit;
    *(uint64_t *)(trampoline_header + 36) = idt_ptr.base;

    uint16_t bsp_cs, bsp_ds;
    asm volatile("mov %%cs, %0" : "=r"(bsp_cs));
    asm volatile("mov %%ds, %0" : "=r"(bsp_ds));
    *(uint16_t *)(trampoline_header + 45) = bsp_cs;
    *(uint16_t *)(trampoline_header + 47) = bsp_ds;

#if defined(DEBUG_SMP)
    printf("[smp] BSP GDT: limit=0x%x, base=0x%lx\n", gdt_ptr.limit, gdt_ptr.base);
    printf("[smp] BSP IDT: limit=0x%x, base=0x%lx\n", idt_ptr.limit, idt_ptr.base);
    printf("[smp] BSP CS=0x%x, DS=0x%x\n", bsp_cs, bsp_ds);
    printf("[smp] BSP LAPIC ID = %u\n", apicGetBspLapicId());
#endif

    volatile uint8_t *ap_progress = (volatile uint8_t *)(bootloader.hhdmOffset + 0x500);
    volatile uint8_t *ap_go_base  = (volatile uint8_t *)(bootloader.hhdmOffset + 0x501);

    for (int i = 0; i < smp_ap_count; i++) {
        uint32_t apic_id = smp_ap_lapic_id_list[i];
        if (apic_id == apicGetBspLapicId()) continue;

        printf("[smp] --- preparing AP %u (index %d) ---\n", apic_id, i);

        *(uint64_t *)(trampoline_header + 8) = (uintptr_t)&ap_stacks[i] + AP_STACK_SIZE;
        *(volatile uint8_t *)(trampoline_header + 44) = 0;
        for (int j = 0; j < HANDSHAKE_STAGES; j++)
            ap_go_base[j] = 0;   // clear go flags 0x501 … 0x50A

        apic_send_init(apic_id);
        simple_mdelay(10);

        apic_send_sipi(apic_id, 0x08);

        bool failed = false;
        for (int stage = 0; stage < HANDSHAKE_STAGES; stage++) {
            bool ok = ap_wait_for_stage(ap_progress, stage_markers[stage],
                                        &ap_go_base[stage], 1,
                                        100000);
            if (ok) {
                printf("[smp] AP %u stage %d: %s\n", apic_id, stage, stage_names[stage]);
            } else {
                printf("[smp] AP %u FAILED at stage %d (%s) – last progress was 0x%x\n",
                       apic_id, stage, stage_names[stage], *ap_progress);
                failed = true;
                break;
            }
        }

        if (!failed) {
            printf("[smp] AP %u reached 64-bit C entry; waiting for commword...\n", apic_id);
            for (int t = 0; t < 100000; t++) {
                if (*(volatile uint8_t *)(trampoline_header + 44) == 1)
                    break;
                asm volatile("pause");
            }
            if (*(volatile uint8_t *)(trampoline_header + 44) == 1)
                printf("[smp] AP %u commword set – running.\n", apic_id);
            else
                printf("[smp] AP %u commword NOT set after all stages!\n", apic_id);
        } else {
            printf("[smp] Skipping remaining APs? Continuing anyway.\n");
        }
    }

    volatile uint32_t *ap_alive_marker = (volatile uint32_t *)(bootloader.hhdmOffset + 0x510);
    printf("[smp] AP alive marker = 0x%x\n", *ap_alive_marker);

    printf("[smp] free physical blocks: %ld / %ld\n",
       (unsigned long)(physical_total_blocks_count - physical_used_blocks_count),
       (unsigned long)physical_total_blocks_count);
    // BSP creates all AP idle tasks sequentially — no concurrent alloc
    asm volatile("cli");
    for (int i = 0; i < smp_ap_count; i++) {
        uint32_t apic_id = smp_ap_lapic_id_list[i];
        if (apic_id == apicGetBspLapicId()) continue;
        printf("[smp] creating idle task for AP %d (index %d)\n", apic_id, i);
        Task* idle = task_create_kernel((uint64_t)kernel_ap_idle_entry, 0);
        printf("[task] created kernel task %d at %p\n", idle->id, idle);
        if (!idle) { printf("[smp] failed to create idle for AP %u\n", apic_id); Halt(); }
        idle->state = TASK_STATE_DUMMY;
        idle->infoPd->pagedir = (uint64_t*)get_page_directory();
        idle->core_affinity = TASK_AFFINITY_ANY;   // this idle task can run on any core
        per_lapic_core_current_task[apic_id] = idle;
        printf("[smp] pre-created idle task for AP %u\n", apic_id);
    }
    asm volatile("sti");

    bsp_done = 1;
    printf("[smp] All AP boot sequences finished. %u AP(s) running.\n", ap_running);
}