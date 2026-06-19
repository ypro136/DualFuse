#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

#define IS_LOCKED true 
#define IS_NOT_LOCKED false

typedef struct Spinlock {
  bool locked;
} Spinlock;
#define __SPINLOCK(name) static Spinlock name = {.locked = false}

//typedef atomic_flag Spinlock;

void spinlock_acquire(Spinlock *lock);
void spinlock_release(Spinlock *lock);

typedef struct SpinlockCnt {
  int64_t cnt;
} SpinlockCnt;

typedef struct Semaphore {
  Spinlock LOCK;
  uint32_t cnt;
  uint8_t  invalid;
} Semaphore;

typedef struct SpinlockIrq {
    volatile uint32_t locked;
    uint32_t          eflags;
} SpinlockIrq;

void spinlock_irq_acquire(SpinlockIrq *lock);
void spinlock_irq_release(SpinlockIrq *lock);


void spinlock_cnt_read_acquire(SpinlockCnt *lock);
void spinlock_cnt_read_release(SpinlockCnt *lock);

void spinlock_cnt_write_acquire(SpinlockCnt *lock);
void spinlock_cnt_write_release(SpinlockCnt *lock);

#endif
