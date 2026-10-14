#include <system.h>

void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) 
{
  uint32_t _eax = *eax, _ecx = *ecx;
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
               : "a"(_eax), "c"(_ecx)
               : "memory");
}