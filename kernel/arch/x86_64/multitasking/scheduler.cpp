#include <scheduler.h>

#include <task.h>
#include <paging.h>
#include <gdt.h>
#include <string.h>
#include <apic.h>                     // for apicCurrentCore, apicGetBspLapicId

bool scheduler_enabled = false;

volatile uint8_t per_core_in_interrupt[256] = {0};

static Spinlock sched_task_list_lock = {0};

static Task* scheduler_pick_next_ready_task(Task* start_after, uint32_t core_id) {
    spinlock_acquire(&sched_task_list_lock);

    Task* candidate = start_after->next ? start_after->next : firstTask;
    for (int i = 0; i < 512; i++) {
        // skip tasks that cannot run on this core
        if (!(candidate->core_affinity & (1 << core_id))) {
            candidate = candidate->next ? candidate->next : firstTask;
            continue;
        }

        if (__sync_bool_compare_and_swap(&candidate->state,
                                         TASK_STATE_READY,
                                         TASK_STATE_RUNNING)) {
            spinlock_release(&sched_task_list_lock);
            return candidate;
        }
        candidate = candidate->next ? candidate->next : firstTask;
    }

    spinlock_release(&sched_task_list_lock);
    return nullptr;
}

void schedule(AsmPassedInterrupt* interrupt_frame) {
    if (!tasksInitiated || !scheduler_enabled)
        return;

    uint32_t core_id = apicCurrentCore();
    per_core_in_interrupt[core_id] = 1;
    #if defined(DEBUG_SCHEDULER) && defined(DEBUG_LOOPING)
        printf("[sched] core %d called by interrupt, runing scheduler\n", core_id);
    #endif

    Task* core_current_task = per_lapic_core_current_task[core_id];
    if (!core_current_task)
        return;

    asm volatile("fxsave %0" :: "m"(core_current_task->fpuenv) : "memory");
    memcpy(&core_current_task->registers, interrupt_frame, sizeof(AsmPassedInterrupt));

    if (core_current_task->state == TASK_STATE_RUNNING)
        core_current_task->state = TASK_STATE_READY;

    Task* next_task = scheduler_pick_next_ready_task(core_current_task, core_id);

    if (!next_task || next_task == core_current_task) {
        // Re-claim before restoring — another core could steal it in the window
        // between the READY reset above and here
        __sync_bool_compare_and_swap(&core_current_task->state,
                                      TASK_STATE_READY,
                                      TASK_STATE_RUNNING);
        asm volatile("fxrstor %0" :: "m"(core_current_task->fpuenv) : "memory");
        per_core_in_interrupt[core_id] = 0;
        return;
    }

    if (!next_task->infoPd || !next_task->infoPd->pagedir) {
        printf("schedule: next_task %d has invalid infoPd\n", next_task->id);
        next_task->state = TASK_STATE_DEAD;
        return;
    }

    // Update per‑core current task and, if this is the BSP, the global pointer
    per_lapic_core_current_task[core_id] = next_task;
    if (core_id == apicGetBspLapicId())
        currentTask = next_task;

#if defined(DEBUG_SCHEDULER) && defined(DEBUG_LOOPING)
    printf("[sched] core %u switching to task %d (rip=0x%lx)\n",
           core_id, next_task->id, next_task->registers.rip);
    if (next_task->whileTssRsp < 0xFFFF800000000000ULL) {
        printf("[sched] next task %d has bad whileTssRsp: 0x%lx\n",
               next_task->id, next_task->whileTssRsp);
        Halt();
    }
#endif

    asm volatile("fxrstor %0" :: "m"(next_task->fpuenv) : "memory");
    per_core_tss[core_id]->rsp0 = next_task->whileTssRsp;
    change_page_directory(next_task->infoPd->pagedir);

    memcpy(interrupt_frame, &next_task->registers, sizeof(AsmPassedInterrupt));

#if defined(DEBUG_SCHEDULER) && defined(DEBUG_LOOPING)
    printf("[sched] core %u restored task %d rip=0x%lx\n",
           core_id, next_task->id, next_task->registers.rip);
#endif
}

void scheduler_lapic_timer_start_on_current_ap() {
    apicWrite(APIC_REGISTER_TIMER_DIV, 0x3);
    apicWrite(APIC_REGISTER_LVT_TIMER, 32 | APIC_LVT_TIMER_MODE_PERIODIC);
    apicWrite(APIC_REGISTER_TIMER_INITCNT, 100000);
}