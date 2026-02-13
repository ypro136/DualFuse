
#include <system.h>
#include <hcf.hpp>

void _assert(bool expression, char *file, int line) {
  if (!expression) {
    printf("[assert] Assertation failed! file{%s:%d}\n", file, line);
    Halt();
  }
}