#include <string.h>
 
size_t strlen(const char* string) {
	size_t length = 0;
	while (string[length] != '\0')
		length++;
	return length;
}