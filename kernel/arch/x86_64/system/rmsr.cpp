#include <system.h>

uint64_t rdmsr(uint32_t msrid) 
{
  uint32_t low;
  uint32_t high;
  __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msrid));
  return (uint64_t)low << 0 | (uint64_t)high << 32;
}