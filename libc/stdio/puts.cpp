#include <stdio.h>
 
/**
 * Puts a string to stdout.
 *
 * Writes the string pointed to by string to stdout, followed by a newline character.
 *
 * @param string The string to write.
 * @return The number of characters written if successful, or EOF on error.
 */
size_t puts(const char *string)
{
	return printf("%s\n", string);
}