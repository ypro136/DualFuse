#include <string.h>
 
/**
 * Copies size bytes from memory area src to memory area dst. 
 * The memory areas must not overlap. Use memmove() if the memory 
 * areas do overlap.
 *
 * @param dst Pointer to the destination array where the content is to be copied, type-casted to a pointer of type void*
 * @param src Pointer to the source of data to be copied, type-casted to a pointer of type const void*
 * @param size Number of bytes to copy.
 * @return Pointer to the destination array dst.
*/
void* memcpy(void* __restrict__ dstptr, const void* __restrict__ srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];
	return dstptr;
}