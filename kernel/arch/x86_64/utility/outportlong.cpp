#include <utility.h>


void out_port_long(uint16_t portid, uint32_t value) {
  __asm__ __volatile__("outl %%eax, %%dx" : : "d"(portid), "a"(value));
}
