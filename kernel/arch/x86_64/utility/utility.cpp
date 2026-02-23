#include <utility.h>
#include <liballoc.h>
#include <cstdlib>
#include <cstring>
#include <atomic>

using std::atomic;
using std::memory_order_relaxed;

bool systemDiskInit = false;

void atomicBitmapSet(volatile uint64_t *bitmap, unsigned int bit) {
  atomic<uint64_t> *atomic_bitmap = (atomic<uint64_t> *)bitmap;
  atomic_bitmap->fetch_or((1UL << bit), memory_order_relaxed);
}

void atomicBitmapClear(volatile uint64_t *bitmap, unsigned int bit) {
  atomic<uint64_t> *atomic_bitmap = (atomic<uint64_t> *)bitmap;
  atomic_bitmap->fetch_and(~(1UL << bit), memory_order_relaxed);
}

uint64_t atomicBitmapGet(volatile uint64_t *bitmap) {
  atomic<uint64_t> *atomic_bitmap = (atomic<uint64_t> *)bitmap;
  return atomic_bitmap->load(memory_order_relaxed);
}

uint8_t atomicRead8(volatile uint8_t *target) {
  atomic<uint8_t> *atomic_target = (atomic<uint8_t> *)target;
  return atomic_target->load(memory_order_relaxed);
}

uint16_t atomicRead16(volatile uint16_t *target) {
  atomic<uint16_t> *atomic_target = (atomic<uint16_t> *)target;
  return atomic_target->load(memory_order_relaxed);
}

uint32_t atomicRead32(volatile uint32_t *target) {
  atomic<uint32_t> *atomic_target = (atomic<uint32_t> *)target;
  return atomic_target->load(memory_order_relaxed);
}

uint64_t atomicRead64(volatile uint64_t *target) {
  atomic<uint64_t> *atomic_target = (atomic<uint64_t> *)target;
  return atomic_target->load(memory_order_relaxed);
}

void atomicWrite8(volatile uint8_t *target, uint8_t value) {
  atomic<uint8_t> *atomic_target = (atomic<uint8_t> *)target;
  atomic_target->store(value, memory_order_relaxed);
}

void atomicWrite16(volatile uint16_t *target, uint16_t value) {
  atomic<uint16_t> *atomic_target = (atomic<uint16_t> *)target;
  atomic_target->store(value, memory_order_relaxed);
}

void atomicWrite32(volatile uint32_t *target, uint32_t value) {
  atomic<uint32_t> *atomic_target = (atomic<uint32_t> *)target;
  atomic_target->store(value, memory_order_relaxed);
}

void atomicWrite64(volatile uint64_t *target, uint64_t value) {
  atomic<uint64_t> *atomic_target = (atomic<uint64_t> *)target;
  atomic_target->store(value, memory_order_relaxed);
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