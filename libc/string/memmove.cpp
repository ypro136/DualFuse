#include <string.h>
 
/**
 * Copies a region of memory from one location to another, 
 * handling overlap between source and destination.
 * 
 * This is a safer alternative to memcpy when the source 
 * and destination memory regions overlap.
 *
 * @param dstptr Pointer to the destination memory region
 * @param srcptr Pointer to the source memory region
 * @param size Number of bytes to copy
 * @return Pointer to the destination memory region (i.e. dstptr)
*/
void* memmove(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	if (dst < src) {
		for (size_t i = 0; i < size; i++)
			dst[i] = src[i];
	} else {
		for (size_t i = size; i != 0; i--)
			dst[i-1] = src[i-1];
	}
	return dstptr;
}