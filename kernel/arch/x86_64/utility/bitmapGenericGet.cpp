
#include <utility.h>

bool bitmapGenericGet(uint8_t *bitmap, size_t index) {
  size_t div = index / 8;
  size_t mod = index % 8;
  return bitmap[div] & (1 << mod);
}
