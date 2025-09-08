#include <spinlock.h>
//#include <system.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utility.h>

#include <hcf.hpp>



void spinlock_acquire(Spinlock *lock) 
{
  while (lock->locked)
  {
    hand_control();
  }
}

void spinlock_release(Spinlock *lock) 
{
  lock->locked = IS_NOT_LOCKED;
}

// Cnt spinlock is basically just a counter that increases for every read
// operation. When something has to modify, it waits for it to become 0 and
// makes it -1, not permitting any reads. Useful for linked lists..

void spinlock_cnt_read_acquire(SpinlockCnt *lock) {
  while (lock->cnt < 0)
    hand_control();
  lock->cnt++;
}

void spinlock_cnt_read_release(SpinlockCnt *lock) {
  if (lock->cnt < 0) {
    printf("[spinlock] Something very bad is going on...\n");
    Halt();
  }

  lock->cnt--;
}

void spinlock_cnt_write_acquire(SpinlockCnt *lock) {
  while (lock->cnt != 0)
    hand_control();
  lock->cnt = -1;
}

void spinlock_cnt_write_release(SpinlockCnt *lock) {
  if (lock->cnt != -1) {
    printf("[spinlock] Something very bad is going on...\n");
    Halt();
  }
  lock->cnt = 0;
}
