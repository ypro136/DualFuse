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

    asm volatile("fxsave %0" :: "m"(currentTask->fpuenv) : "memory");
    memcpy(&currentTask->registers, interrupt_frame, sizeof(AsmPassedInterrupt));

    Task* next_task = scheduler_pick_next_ready_task();

    if (next_task == currentTask) {
        asm volatile("fxrstor %0" :: "m"(currentTask->fpuenv) : "memory");
        return;
    }

    currentTask = next_task;

    asm volatile("fxrstor %0" :: "m"(currentTask->fpuenv) : "memory");
    gdt_update_tss_rsp0(currentTask->whileTssRsp);
    change_page_directory(currentTask->infoPd->pagedir);
    memcpy(interrupt_frame, &currentTask->registers, sizeof(AsmPassedInterrupt));
}
