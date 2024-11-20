#include <string.h>
 
/**
 * Compares two blocks of memory.
 * 
 * Compares the first size bytes of the block of memory pointed by 
 * aptr to the first size bytes pointed by bptr, returning zero if 
 * they all match or a negative/positive value if the first byte that 
 * does not match is lower/greater in aptr than in bptr.
 */
int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}