#include <spinlock.h>
#include <stdio.h>
#include <hcf.hpp>

void spinlock_acquire(Spinlock *lock) {
    while (__sync_lock_test_and_set((uint8_t *)&lock->locked, (uint8_t)1)) {
        while (*(volatile uint8_t *)&lock->locked)
            asm volatile("pause");
    }
}

void spinlock_release(Spinlock *lock) {
    __sync_lock_release((uint8_t *)&lock->locked);
}

void spinlock_cnt_read_acquire(SpinlockCnt *lock) {
    while (true) {
        int64_t observed = __sync_fetch_and_add(&lock->cnt, (int64_t)0);
        if (observed < 0) {
            asm volatile("pause");
            continue;
        }
        if (__sync_bool_compare_and_swap(&lock->cnt, observed, observed + 1))
            return;
    }
}

void spinlock_cnt_read_release(SpinlockCnt *lock) {
    __sync_sub_and_fetch(&lock->cnt, (int64_t)1);
}

void spinlock_cnt_write_acquire(SpinlockCnt *lock) {
    while (!__sync_bool_compare_and_swap(&lock->cnt, (int64_t)0, (int64_t)-1))
        asm volatile("pause");
}

void spinlock_cnt_write_release(SpinlockCnt *lock) {
    __sync_bool_compare_and_swap(&lock->cnt, (int64_t)-1, (int64_t)0);
}