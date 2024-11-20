//#include <stdatomic.h>

#include <types.h>

#ifndef SPINLOCK_H
#define SPINLOCK_H

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

void spinlock_cnt_read_acquire(SpinlockCnt *lock);
void spinlock_cnt_read_release(SpinlockCnt *lock);

void spinlock_cnt_write_acquire(SpinlockCnt *lock);
void spinlock_cnt_write_release(SpinlockCnt *lock);

#endif
