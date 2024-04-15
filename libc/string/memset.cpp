#include <string.h>
 
/**
 * Sets the first num bytes of the block of memory
 * pointed by ptr to the specified value (interpreted as an unsigned char).
 *
 * @param bufptr Pointer to the block of memory to fill.
 * @param value Value to be set, passed as an int but casted to unsigned char.
 * @param size Number of bytes to be set to the value.
 * @return Pointer to the memory area bufptr.
 */
void *memset(void *bufptr, int value, size_t size)
{
    unsigned char *buf = (unsigned char *)bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char)value;
    return bufptr;
}