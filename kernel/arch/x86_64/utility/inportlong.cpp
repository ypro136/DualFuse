#include <utility.h>


uint32_t inportlong(uint16_t portid) {
  uint32_t ret;
  __asm__ __volatile__("inl %%dx, %%eax" : "=a"(ret) : "d"(portid));
  return ret;
}

