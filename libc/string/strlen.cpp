#include <string.h>
 
/**
 * Returns the length of the given null-terminated string.
 *
 * @param string The null-terminated string whose length to return.
 * @return The number of characters in the string before the terminating null byte.
 */
size_t strlen(const char *string)
{
	size_t length = 0;
	while (string[length] != '\0')
		length++;
	return length;
}