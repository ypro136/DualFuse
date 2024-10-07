#include <kernel/idt.h>
#include <kernel/isr.h>
#include <kernel/timer.h>


#include <kernel/utility.h>
#include <utility/hcf.hpp>



char *format = "[isr] Kernel panic: %s!\n";

char *exceptions[] = {"Division By Zero",
                      "Debug",
                      "Non Maskable Interrupt",
                      "Breakpoint",
                      "Into Detected Overflow",
                      "Out of Bounds",
                      "Invalid Opcode",
                      "No Coprocessor",

                      "Double Fault",
                      "Coprocessor Segment Overrun",
                      "Bad TSS",
                      "Segment Not Present",
                      "Stack Fault",
                      "General Protection Fault",
                      "Page Fault",
                      "Unknown Interrupt",

                      "Coprocessor Fault",
                      "Alignment Check",
                      "Machine Check",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",

                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved",
                      "Reserved"};

void remap_pic() {
  outPortByte(0x20, 0x11);
  outPortByte(0xA0, 0x11);

  outPortByte(0x21, 0x20);
  outPortByte(0xA1, 0x28);

  outPortByte(0x21, 0x04);
  outPortByte(0xA1, 0x02);

  outPortByte(0x21, 0x01);
  outPortByte(0xA1, 0x01);

  outPortByte(0x21, 0x00);
  outPortByte(0xA1, 0x00);
}

irqHandler *firstIrqHandler = 0;

void isr_initialize() {

  remap_pic();

  // ISR exceptions 0 - 31
  for (int i = 0; i < 48; i++) {
    set_idt_gate(i, (uint64_t)asm_isr_redirect_table[i], 0x8E);
  }

  // Syscalls having DPL 3
  set_idt_gate(0x80, (uint64_t)isr128, 0xEE);

  // Finalize
  set_idt();
  asm volatile("sti");
}

// irqHandler *registerIRQhandler(uint8_t id, void *handler) {
//   // printf("IRQ %d reserved!\n", id);
//   irqHandler *target = (irqHandler *)LinkedListAllocate(
//       (void **)(&firstIrqHandler), sizeof(irqHandler));

//   target->id = id;
//   target->handler = handler;
//   // target->next = 0; // null ptr

//   return target;
// }

// void handleTaskFault(AsmPassedInterrupt *regs) {
//   if (regs->interrupt == 14) {
//     uint64_t err_pos;
//     asm volatile("movq %%cr2, %0" : "=r"(err_pos));
//     printf("[isr] Page fault occured at cr2{%lx} rip{%lx}\n", err_pos,
//            regs->rip);
//   }
//   printf("[isr::task] [%c] Killing task{%d} because of %s!\n",
//          currentTask->kernel_task ? '-' : 'u', currentTask->id,
//          exceptions[regs->interrupt]);
//   // printf("at %lx\n", regs->rip);
//   // panic();
//   taskKill(currentTask->id, 139);
//   schedule((uint64_t)regs);
// }

// uint64_t handle_syscall_tssrsp(uint64_t rsp) {
//   if (!tasksInitiated)
//     return rsp;

//   void *cpu = (void *)rsp;

//   void *iretqRsp =
//       (void *)(currentTask->whileSyscallRsp - sizeof(AsmPassedInterrupt) - 8);
//   memcpy(iretqRsp, cpu, sizeof(AsmPassedInterrupt) + 8);

//   return (size_t)iretqRsp;
// }

// uint64_t handle_tssrsp(uint64_t rsp) {
//   if (!tasksInitiated)
//     return rsp;

//   AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;

//   AsmPassedInterrupt *iretqRsp =
//       (AsmPassedInterrupt *)(currentTask->whileTssRsp -
//                              sizeof(AsmPassedInterrupt));
//   memcpy(iretqRsp, cpu, sizeof(AsmPassedInterrupt));

//   return (size_t)iretqRsp;
// }

void *irq_routines[16] = {0};

void irq_install_handler(int irq, void (*handler)(struct interrupt_registers *registers))
{
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq)
{
    irq_routines[irq] = 0;
}

void irq_handler(int irq, AsmPassedInterrupt *cpu)
{
  void (*handler)(struct interrupt_registers *registers);

      handler = irq_routines[irq];

      if(handler)
      {
        handler((uint64_t)cpu);
        return;
      }
      printf("irq %d was called befor it was initalized\n", irq);
}

// pass stack ptr
extern "C" void handle_interrupt(uint64_t rsp) {
  AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;
  if (cpu->interrupt >= 32 && cpu->interrupt <= 47) { // IRQ
    if (cpu->interrupt >= 40) {
      outPortByte(0xA0, 0x20);
    }
    outPortByte(0x20, 0x20);
    switch (cpu->interrupt) {
    case 32 + 0: // irq0
      irq_handler(0, cpu);
      break;

    case 32 + 1: // irq1
      irq_handler(1, cpu);
      break;

    default: { // execute other handlers
      irqHandler *browse = firstIrqHandler;
      while (browse) {
        if (browse->id == (cpu->interrupt - 32)) {
          FunctionPtr handler = browse->handler;
          handler(cpu);
        }
        browse = browse->next;
      }
      break;
    }
    }
  } else if (cpu->interrupt >= 0 && cpu->interrupt <= 31) { // ISR
    // if (currentTask->systemCallInProgress)
    //   printf("[isr] Happened from system call!\n");

    // if (!currentTask->systemCallInProgress && tasksInitiated &&
    //     currentTask->id != KERNEL_TASK_ID) { // && !currentTask->kernel_task
    //   handleTaskFault(cpu);
    //   return;
    // }

    return; // TODO: remove me and implement the above.

    if (cpu->interrupt == 14) {
      uint64_t err_pos;
      asm volatile("movq %%cr2, %0" : "=r"(err_pos));
      printf("[isr] Page fault occured at cr2{%lx} rip{%lx}\n", err_pos,
             cpu->rip);
    }
    printf(format, exceptions[cpu->interrupt]);
    // if (framebuffer == KERNEL_GFX)
    //   printf(format, exceptions[cpu->interrupt]);
    Halt();
  } else if (cpu->interrupt == 0x80) {
    // syscallHandler(cpu);
  }
}