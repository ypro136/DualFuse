#include <utility.h>

void atomicBitmapSet(volatile uint64_t *bitmap, unsigned int bit) {
  atomic_fetch_or((volatile _Atomic uint64_t *)bitmap, (1UL << bit));
}

void atomicBitmapClear(volatile uint64_t *bitmap, unsigned int bit) {
  atomic_fetch_and((volatile _Atomic uint64_t *)bitmap, ~(1UL << bit));
}

uint64_t atomicBitmapGet(volatile uint64_t *bitmap) {
  return atomic_load((volatile _Atomic uint64_t *)bitmap);
}

uint8_t atomicRead8(volatile uint8_t *target) {
  return atomic_load((volatile _Atomic uint8_t *)target);
}

uint16_t atomicRead16(volatile uint16_t *target) {
  return atomic_load((volatile _Atomic uint16_t *)target);
}

uint32_t atomicRead32(volatile uint32_t *target) {
  return atomic_load((volatile _Atomic uint32_t *)target);
}

uint64_t atomicRead64(volatile uint64_t *target) {
  return atomic_load((volatile _Atomic uint64_t *)target);
}

void atomicWrite8(volatile uint8_t *target, uint8_t value) {
  atomic_store((volatile _Atomic uint8_t *)target, value);
}

void atomicWrite16(volatile uint16_t *target, uint16_t value) {
  atomic_store((volatile _Atomic uint16_t *)target, value);
}

void atomicWrite32(volatile uint32_t *target, uint32_t value) {
  atomic_store((volatile _Atomic uint32_t *)target, value);
}

void atomicWrite64(volatile uint64_t *target, uint64_t value) {
  atomic_store((volatile _Atomic uint64_t *)target, value);
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
    bitmap[div] &= ~(1 << mod);
}

char *strdup(char *source) { // TODO: move me to string somhow
  int   len = strlen(source) + 1;
  char *target = (char *)malloc(len);
  memcpy(target, source, len);
  return target;
}


static unsigned long int next = 1;

int rand(void) {
  next = next * 1103515245 + 12345;
  return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) { next = seed; }