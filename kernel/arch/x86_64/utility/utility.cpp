#include <utility.h>
#include <liballoc.h>

bool systemDiskInit = false;

void atomicBitmapSet(volatile uint64_t *bitmap, unsigned int bit) {
    __sync_fetch_and_or(bitmap, (uint64_t)1 << bit);
}

void atomicBitmapClear(volatile uint64_t *bitmap, unsigned int bit) {
    __sync_fetch_and_and(bitmap, ~((uint64_t)1 << bit));
}

uint64_t atomicBitmapGet(volatile uint64_t *bitmap) {
    return __sync_fetch_and_add(bitmap, (uint64_t)0);
}

uint8_t atomicRead8(volatile uint8_t *target) {
    return __sync_fetch_and_add(target, (uint8_t)0);
}

uint16_t atomicRead16(volatile uint16_t *target) {
    return __sync_fetch_and_add(target, (uint16_t)0);
}

uint32_t atomicRead32(volatile uint32_t *target) {
    return __sync_fetch_and_add(target, (uint32_t)0);
}

uint64_t atomicRead64(volatile uint64_t *target) {
    return __sync_fetch_and_add(target, (uint64_t)0);
}

void atomicWrite8(volatile uint8_t *target, uint8_t value) {
    __sync_lock_test_and_set(target, value);
}

void atomicWrite16(volatile uint16_t *target, uint16_t value) {
    __sync_lock_test_and_set(target, value);
}

void atomicWrite32(volatile uint32_t *target, uint32_t value) {
    __sync_lock_test_and_set(target, value);
}

void atomicWrite64(volatile uint64_t *target, uint64_t value) {
    __sync_lock_test_and_set(target, value);
}

bool bitmapGenericGet(uint8_t *bitmap, size_t index) {
    size_t div = index / 8;
    size_t mod = index % 8;
    return bitmap[div] & (1 << mod);
}

void bitmapGenericSet(uint8_t *bitmap, size_t index, bool set) {
    size_t div = index / 8;
    size_t mod = index % 8;
    if (set)
        bitmap[div] |= (1 << mod);
    else
        bitmap[div] &= (uint8_t)~(1 << mod);
}

char *strdup(char *source) {
    size_t len = strlen(source) + 1;
    char *target = (char *)malloc(len);
    memcpy(target, source, len);
    return target;
}

static unsigned long int next = 1;

int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}