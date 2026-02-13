#include <utility.h>

void bitmapGenericSet(uint8_t *bitmap, size_t index, bool set) {
  size_t div = index / 8;
  size_t mod = index % 8;
  if (set)
    bitmap[div] |= (1 << mod);
  else
    bitmap[div] &= ~(1 << mod);
}