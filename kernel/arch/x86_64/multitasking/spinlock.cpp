#include <spinlock.h>
#include <stdio.h>
#include <hcf.hpp>

void spinlock_acquire(Spinlock *lock) 
{
  uint64_t spin_iteration_count = 0;
  bool warning_printed = false;

  while (__sync_lock_test_and_set(&lock->locked, 1))
  {
    asm volatile("pause");
#if defined(DEBUG_SPINLOCK)
    if (++spin_iteration_count >= 10000000 && !warning_printed)
    {
      printf("[DEBUG_SPINLOCK] spinlock_acquire spinning >10000000 on %p\n", (void*)lock);
      warning_printed = true;
    }
#endif
  }
}

void spinlock_release(Spinlock *lock) 
{
  __sync_lock_release(&lock->locked);
}

void spinlock_cnt_read_acquire(SpinlockCnt *lock)
{
  uint64_t spin_iteration_count = 0;
  bool warning_printed = false;

  while (true)
  {
    int64_t current = __sync_fetch_and_add(&lock->cnt, 0);      // atomic read
    if (current >= 0 && __sync_bool_compare_and_swap(&lock->cnt, current, current + 1))
      break;

    asm volatile("pause");
#if defined(DEBUG_SPINLOCK)
    if (++spin_iteration_count >= 10000000 && !warning_printed)
    {
      printf("[DEBUG_SPINLOCK] spinlock_cnt_read_acquire spinning >10000000 on %p\n", (void*)lock);
      warning_printed = true;
    }
#endif
  }
}

void spinlock_cnt_read_release(SpinlockCnt *lock)
{
  if (lock->cnt < 0)
  {
    printf("[spinlock] Something very bad is going on...\n");
    Halt();
  }
  __sync_fetch_and_sub(&lock->cnt, 1);
}

void spinlock_cnt_write_acquire(SpinlockCnt *lock)
{
  uint64_t spin_iteration_count = 0;
  bool warning_printed = false;

  while (true)
  {
    while (__sync_fetch_and_add(&lock->cnt, 0) != 0)
    {
      asm volatile("pause");
#if defined(DEBUG_SPINLOCK)
      if (++spin_iteration_count >= 10000000 && !warning_printed)
      {
        printf("[DEBUG_SPINLOCK] spinlock_cnt_write_acquire spinning >10000000 on %p\n", (void*)lock);
        warning_printed = true;
      }
#endif
    }

    if (__sync_bool_compare_and_swap(&lock->cnt, 0, -1))
      break;

    asm volatile("pause");
#if defined(DEBUG_SPINLOCK)
    if (++spin_iteration_count >= 10000000 && !warning_printed)
    {
      printf("[DEBUG_SPINLOCK] spinlock_cnt_write_acquire spinning >10000000 on %p\n", (void*)lock);
      warning_printed = true;
    }
#endif
  }
}

void spinlock_cnt_write_release(SpinlockCnt *lock)
{
  if (lock->cnt != -1)
  {
    printf("[spinlock] Something very bad is going on...\n");
    Halt();
  }
  __sync_bool_compare_and_swap(&lock->cnt, -1, 0);
}