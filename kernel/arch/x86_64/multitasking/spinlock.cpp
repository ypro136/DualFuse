#include <spinlock.h>
//#include <system.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utility.h>

#include <hcf.hpp>

// Set to 1 to enable debug prints, 0 to disable
#define SPINLOCK_DEBUG 0

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
 

#if SPINLOCK_DEBUG
// Recursion guard: set to 1 while we are inside printf to avoid re‑entry
static int spinlock_printf_guard = 0;
#endif

void spinlock_irq_acquire(SpinlockIrq *lock) {
#if SPINLOCK_DEBUG
    if (!spinlock_printf_guard) {
        spinlock_printf_guard = 1;
        printf("[spinlock] trying to acquire %p\n", (void*)lock);
        spinlock_printf_guard = 0;
    }
#endif

    // Try to grab the lock with interrupts disabled
    while (1) {
        uint32_t old = 1;   // what we exchange: 1 (we want to set lock to 1)

        // Disable interrupts and atomically exchange lock->locked with 1
        asm volatile(
            "cli\n"
            "xchgl %0, %1\n"
            : "+a"(old), "=m"(lock->locked)
            : "m"(lock->locked)
            : "memory"
        );

        // old now contains the previous value of lock->locked
        if (old == 0) {
            // We own the lock – interrupts are still off
#if SPINLOCK_DEBUG
            if (!spinlock_printf_guard) {
                spinlock_printf_guard = 1;
                printf("[spinlock] acquired %p\n", (void*)lock);
                spinlock_printf_guard = 0;
            }
#endif
            return;
        }

        // Lock was busy – re‑enable interrupts and spin
        asm volatile("sti" ::: "memory");
        asm volatile("pause" ::: "memory");

#if SPINLOCK_DEBUG
        if (!spinlock_printf_guard) {
            spinlock_printf_guard = 1;
            printf("[spinlock] spinning on %p\n", (void*)lock);
            spinlock_printf_guard = 0;
        }
#endif
    }
}

void spinlock_irq_release(SpinlockIrq *lock) {
#if SPINLOCK_DEBUG
    if (!spinlock_printf_guard) {
        spinlock_printf_guard = 1;
        printf("[spinlock] releasing %p\n", (void*)lock);
        spinlock_printf_guard = 0;
    }
#endif

    // Release the lock and re‑enable interrupts
    asm volatile(
        "movl $0, %0\n"
        "sti\n"
        : "=m"(lock->locked)
        :
        : "memory"
    );
}
