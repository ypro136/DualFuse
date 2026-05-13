#include <scheduler.h>

#include <task.h>
#include <paging.h>
#include <gdt.h>
#include <string.h>

bool scheduler_enabled = false;

static Task* scheduler_pick_next_ready_task() {
    Task* candidate = currentTask->next ? currentTask->next : firstTask;
    for (int scheduler_search_iteration = 0; scheduler_search_iteration < 512; scheduler_search_iteration++) {
        if (candidate->state == TASK_STATE_READY)
            return candidate;
        candidate = candidate->next ? candidate->next : firstTask;
    }
    return dummyTask;
}

void schedule(AsmPassedInterrupt* interrupt_frame) {
    if (!tasksInitiated || !scheduler_enabled || !currentTask)
        return;

#if defined(DEBUG_SCHEDULER) && defined(DEBUG_LOOPING)
    printf("[sched] enter, currentTask=%d, interrupt_frame=%p\n",
           currentTask->id, interrupt_frame);
#endif

    asm volatile("fxsave %0" :: "m"(currentTask->fpuenv) : "memory");
    memcpy(&currentTask->registers, interrupt_frame, sizeof(AsmPassedInterrupt));

#if defined(DEBUG_SCHEDULER) && defined(DEBUG_LOOPING)
    printf("[sched] saved task %d rip=0x%lx\n",
           currentTask->id, currentTask->registers.rip);
#endif

    Task* next_task = scheduler_pick_next_ready_task();

    if (next_task == currentTask) {
        asm volatile("fxrstor %0" :: "m"(currentTask->fpuenv) : "memory");
        return;
    }

    if (!next_task->infoPd || !next_task->infoPd->pagedir) {
        printf("schedule: next_task %d has invalid infoPd\n", next_task->id);
        next_task->state = TASK_STATE_DEAD;
        return;
    }

    currentTask = next_task;

#if defined(DEBUG_SCHEDULER)
    if (currentTask->whileTssRsp < 0xFFFF800000000000ULL) {
        printf("[sched] next task %d has bad whileTssRsp: 0x%lx\n",
               currentTask->id, currentTask->whileTssRsp);
        Halt();
    }
#endif

    if (!currentTask->infoPd) {
        printf("FATAL: currentTask->infoPd is NULL\n");
        Halt();
    }

    asm volatile("fxrstor %0" :: "m"(currentTask->fpuenv) : "memory");
    gdt_update_tss_rsp0(currentTask->whileTssRsp);
    change_page_directory(currentTask->infoPd->pagedir);
    #if defined(DEBUG_SCHEDULER)
        printf("[sched] about to restore task %d: rip=0x%lx, interrupt_frame=%p\n",
            currentTask->id, currentTask->registers.rip, interrupt_frame);
    #endif
    #if defined(DEBUG_SCHEDULER)
        if ((uint64_t)interrupt_frame < 0xFFFF800000000000ULL) {
            printf("[sched] FATAL: interrupt_frame is invalid: %p\n", interrupt_frame);
            printf("[sched]   currentTask id=%d, whileTssRsp=0x%lx\n",
                currentTask->id, currentTask->whileTssRsp);
            Halt();
        }
    #endif
        memcpy(interrupt_frame, &currentTask->registers, sizeof(AsmPassedInterrupt));
    #if defined(DEBUG_SCHEDULER)
        printf("[sched] restored successfully\n");
    #endif

#if defined(DEBUG_SCHEDULER)
    printf("[sched] restored task %d rip=0x%lx, interrupt_frame=%p\n",
           currentTask->id, currentTask->registers.rip, interrupt_frame);
#endif
}