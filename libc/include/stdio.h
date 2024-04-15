#ifndef _STDIO_H
#define _STDIO_H 1
 
#include <sys/cdefs.h>

#include <stddef.h>


 
#define EOF (-1)
 
#ifdef __cplusplus
extern "C" {
#endif
 
size_t printf(const char* format_string, ...);
int putchar(int string_1);
size_t puts(const char*);
char *to_string (int number);
char *itoa(int value, char *result, int base);


 
#ifdef __cplusplus
}
#endif
 
#endif