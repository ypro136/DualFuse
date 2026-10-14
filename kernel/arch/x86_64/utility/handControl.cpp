#include <utility.h>
#include <paging.h>
#include <task.h>
#include <scheduler.h>
#include <apic.h>
#include <hcf.hpp>

void hand_control() {
#if defined(DEBUG_SCHEDULER)
    if (apic_initialized) {
        uint32_t core_id = apicCurrentCore();
        if (per_core_in_interrupt[core_id]) {
            printf("[hand_control] FATAL: called from interrupt context on core %u\n", core_id);
            Halt();
        }
    }
#endif
    current_task_this_core()->schedPageFault = true;
    volatile uint8_t _drop = *(uint8_t *)(SCHED_PAGE_FAULT_MAGIC_ADDRESS);
    (void)(_drop);
}