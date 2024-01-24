#include <string.h>


char* strcat( char* destination_string, char* source_string )
{
     while (*destination_string) destination_string++;
     while (*destination_string++ = *source_string++);
     return --destination_string;
}