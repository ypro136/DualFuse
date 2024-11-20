#include <system.h>

uint64_t wrmsr(uint32_t msrid, uint64_t value) 
{
  uint32_t low = value >> 0 & 0xFFFFFFFF;
  uint32_t high = value >> 32 & 0xFFFFFFFF;
  __asm__ __volatile__("wrmsr" : : "a"(low), "d"(high), "c"(msrid) : "memory");
  return value;
}