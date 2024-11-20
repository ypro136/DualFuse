#include <utility.h>

#include <paging.h>


#include <task.h>
void hand_control() {
  currentTask->schedPageFault = true;
  volatile uint8_t _drop = *(uint8_t *)(SCHED_PAGE_FAULT_MAGIC_ADDRESS);
  (void)(_drop);
}