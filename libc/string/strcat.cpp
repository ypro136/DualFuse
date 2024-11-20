#include <string.h>


/**
 * Appends a copy of the source string to the destination string.
 *
 * This function appends the source string to the end of destination string.
 * The terminating null character of destination is overwritten.
 *
 * @param destination_string Pointer to the destination character array. This should contain a C string,
 *                           and have enough space to hold the concatenated result.
 * @param source_string Pointer to the source C string to append.
 *
 * @return Pointer to destination_string.
 */
char *strcat(const char *_destination_string, const char *_source_string)
{
     char *destination_string = (char*)_destination_string;
     char *source_string = (char*)_source_string;

     while (*destination_string)
          destination_string++;
     while (*destination_string++ = *source_string++);
     
     return --destination_string;
}