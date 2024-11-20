#include <string.h>
 
/**
 * Copies the null-terminated string pointed to by source_string to the
 * buffer pointed to by destination_string.
 *
 * @param destination_string Pointer to the destination array where the content is to be copied.
 * @param source_string Pointer to the null-terminated string to be copied.
 *
 * @return destination_string
 */
void strcpy(const char *destination_string, const char *source_string)
{
    char *destination_pointer = (char *)destination_string;

    size_t destination_length = strlen(destination_string);
    size_t source_length = strlen(source_string);
    size_t new_length = destination_length + source_length;

    for (int i = 0; i <= source_length; i++)
    {
        destination_pointer[destination_length + i] = source_string[i];
    }
    destination_pointer[new_length] = '\0';

    // free(destination_pointer);
}
