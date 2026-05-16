#include <utility.h>

#include <paging.h>

#include <task.h>

void hand_control() {
  current_task_this_core()->schedPageFault = true;
  volatile uint8_t _drop = *(uint8_t *)(SCHED_PAGE_FAULT_MAGIC_ADDRESS);
  (void)(_drop);
}