#include <idt.h>
#include <isr.h>
#include <timer.h>
#include <gdt.h>

#include <syscalls.h>
#include <console.h>
#include <scheduler.h>
#include <task.h>
#include <dbg.h>
#include <apic.h>

#include <utility.h>
#include <hcf.hpp>
#include <paging.h>   // for SCHED_PAGE_FAULT_MAGIC_ADDRESS

bool isr_initialized = false;

char *format = "[isr] Kernel Halt: %s!\n";

char *exceptions[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode",
    "No Coprocessor", "Double Fault", "Coprocessor Segment Overrun",
    "Bad TSS", "Segment Not Present", "Stack Fault",
    "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

void remap_pic() {
    out_port_byte(0x20, 0x11);
    out_port_byte(0xA0, 0x11);
    out_port_byte(0x21, 0x20);
    out_port_byte(0xA1, 0x28);
    out_port_byte(0x21, 0x04);
    out_port_byte(0xA1, 0x02);
    out_port_byte(0x21, 0x01);
    out_port_byte(0xA1, 0x01);
    out_port_byte(0x21, 0x00);
    out_port_byte(0xA1, 0x00);
}

irqHandler *firstIrqHandler = 0;

void isr_initialize() {
    remap_pic();

    for (int i = 0; i < 48; i++)
        set_idt_gate(i, (uint64_t)asm_isr_redirect_table[i], 0x8E);

    set_idt_gate(0x80, (uint64_t)isr128, 0xEE);

    set_idt();
    asm volatile("sti");

    isr_initialized = true;
    printf("idt and isr initialized.\n");
}

// ----------------------------------------------------------
// User‑task fault handling
// ----------------------------------------------------------
void handle_task_fault(AsmPassedInterrupt *regs) {
    bool last_console_state;
    if (regs->interrupt == 14) {
        uint64_t err_pos;
        asm volatile("movq %%cr2, %0" : "=r"(err_pos));
        last_console_state = console_is_output_enabled();
        console_set_output_enabled(false);
        printf("[isr] Core %d: Page fault at cr2{%lx} rip{%lx} err{%lx}\n",
               apicCurrentCore(), err_pos, regs->rip, regs->error);
        console_set_output_enabled(last_console_state);
    }
    last_console_state = console_is_output_enabled();
    console_set_output_enabled(false);
    printf("[isr::task] [%c] Killing task{%d} because of %s!\n",
           current_task_this_core()->kernel_task ? '-' : 'u',
           current_task_this_core()->id,
           exceptions[regs->interrupt]);
    console_set_output_enabled(last_console_state);
    task_kill(current_task_this_core()->id, 139);
}

// ----------------------------------------------------------
// TSS RSP helpers for syscalls / interrupts
// ----------------------------------------------------------
extern "C" uint64_t handle_syscall_tssrsp(uint64_t rsp) {
    if (!tasksInitiated) return rsp;
    Task *task = current_task_this_core();
    void *cpu = (void *)rsp;
    void *iretqRsp = (void *)(task->whileSyscallRsp - sizeof(AsmPassedInterrupt) - 8);
    memcpy(iretqRsp, cpu, sizeof(AsmPassedInterrupt) + 8);
    return (size_t)iretqRsp;
}

extern "C" uint64_t handle_tssrsp(uint64_t rsp) {
    if (!tasksInitiated) return rsp;
    Task *task = current_task_this_core();
    AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;
    AsmPassedInterrupt *iretqRsp = (AsmPassedInterrupt *)(task->whileTssRsp - sizeof(AsmPassedInterrupt));
    memcpy(iretqRsp, cpu, sizeof(AsmPassedInterrupt));
    return (size_t)iretqRsp;
}

// ----------------------------------------------------------
// IRQ handler table & installer
// ----------------------------------------------------------
void *irq_routines[16] = {0};

void *irq_install_handler(int irq, void (*handler)(struct interrupt_registers *registers)) {
    irq_routines[irq] = handler;
    return handler;
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

void irq_handler(int irq, AsmPassedInterrupt *cpu) {
    void (*handler)(struct interrupt_registers *registers) = irq_routines[irq];
    if (handler) {
        handler((uint64_t)cpu);
        return;
    }
    bool last_console_state = console_is_output_enabled();
    console_set_output_enabled(false);
    printf("irq %d was called before it was initialized\n", irq);
    console_set_output_enabled(last_console_state);
}

// ----------------------------------------------------------
// Main interrupt dispatcher
// ----------------------------------------------------------
extern "C" void handle_interrupt(uint64_t rsp) {
    AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;

    // ---- IRQ range ----
    if (cpu->interrupt >= 32 && cpu->interrupt <= 47) {
        // EOI (APIC or PIC)
        if (apic_initialized) {
            apicWrite(0xB0, 0);
        } else {
            if (cpu->interrupt >= 40)
                out_port_byte(0xA0, 0x20);
            out_port_byte(0x20, 0x20);
        }

        switch (cpu->interrupt) {
        case 32 + 0:    // timer
            irq_handler(0, cpu);
            schedule(cpu);
            break;

        case 32 + 1:    // keyboard
            irq_handler(1, cpu);
            break;

        case 32 + 11:   // mouse?
            irq_handler(11, cpu);
            break;

        case 32 + 12:   // another
            irq_handler(12, cpu);
            break;

        default: {
            irqHandler *browse = firstIrqHandler;
            while (browse) {
                if (browse->id == (cpu->interrupt - 32)) {
                    FunctionPtr handler = browse->handler;
                    handler(cpu);
                    break;
                }
                browse = browse->next;
            }
            break;
        }
        }
        return;
    }

    // ---- Exception range ----
    if (cpu->interrupt >= 0 && cpu->interrupt <= 31) {
        // If a user task triggers an exception, kill it cleanly
        if (current_task_this_core() &&
            !current_task_this_core()->systemCallInProgress &&
            tasksInitiated &&
            current_task_this_core()->id != KERNEL_TASK_ID &&
            !current_task_this_core()->kernel_task) {
            handle_task_fault(cpu);
            return;
        }

        // --- Special handling for Page Fault (interrupt 14) ---
        if (cpu->interrupt == 14) {
            uint64_t cr2;
            asm volatile("movq %%cr2, %0" : "=r"(cr2));

            // Scheduler deliberate page‑fault: let it pass through
            if (cr2 == SCHED_PAGE_FAULT_MAGIC_ADDRESS) {
                // Only kernel tasks should use this; if it's a kernel
                // task we simply return – the scheduler will be
                // triggered on the next timer tick if needed.
                if (current_task_this_core() &&
                    current_task_this_core()->kernel_task) {
                    return;
                }
                // Fall through – treat as a real fault for user tasks
            }

            // Print page‑fault details before halting (if it gets that far)
            bool last = console_is_output_enabled();
            console_set_output_enabled(false);
            printf("[isr] Core %d: Page fault at cr2{%lx}\n", apicCurrentCore(), cr2);
            console_set_output_enabled(last);
        } else {
            bool last = console_is_output_enabled();
            console_set_output_enabled(false);
            printf("[isr] Core %d: Exception %d (%s) at rip{%lx} err{%lx}\n",
                   apicCurrentCore(), cpu->interrupt,
                   exceptions[cpu->interrupt], cpu->rip, cpu->error);
            console_set_output_enabled(last);
        }

        printf(format, exceptions[cpu->interrupt]);

        // Debug break for breakpoint exception (int 3)
        if (cpu->interrupt == 3) {
            printf("[isr] dbg : %d\n", cpu->interrupt);
            dbg_saveregs();
            dbg_main(cpu->interrupt);
            dbg_loadregs();
        }

        uint64_t cr2;
        asm volatile("movq %%cr2, %0" : "=r"(cr2));
        printf("Fault at rip=0x%lx, cr2=0x%lx, error=0x%lx\n",
               cpu->rip, cr2, cpu->error);
        printf("current_task_this_core()=%p, current_task_this_core()->infoPd=%p, "
               "current_task_this_core()->whileTssRsp=0x%lx\n",
               current_task_this_core(),
               current_task_this_core() ? current_task_this_core()->infoPd : 0,
               current_task_this_core() ? current_task_this_core()->whileTssRsp : 0);
        Halt();
    } 

    // ---- Syscall (0x80) ----
    if (cpu->interrupt == 0x80) {
        syscall_handler(cpu);
    }
}