#include <string.h>
 
void strcpy(const char *destination_string, const char *source_string)
{
    char *destination_pointer = (char*)destination_string;

    size_t destination_length = strlen(destination_string);
    size_t source_length = strlen(source_string);
    size_t new_length = destination_length + source_length;

    for (int i = 0; i <= source_length; i++)
    {
        destination_pointer[destination_length + i] = source_string[i];
    }
    destination_pointer[new_length] = '\0';
}
