#ifndef _STDIO_H
#define _STDIO_H 1
 
#include <sys/cdefs.h>

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

 
#define EOF (-1)
 
#ifdef __cplusplus
extern "C" {
#endif

#define PRINTF_STATE_START 0
#define PRINTF_STATE_LENGTH 1
#define PRINTF_STATE_SHORT 2
#define PRINTF_STATE_LONG 3
#define PRINTF_STATE_SPEC 4

#define PRINTF_LENGTH_START 0
#define PRINTF_LENGTH_SHORT_SHORT 1
#define PRINTF_LENGTH_SHORT 2
#define PRINTF_LENGTH_LONG 3
#define PRINTF_LENGTH_LONG_LONG 4

int sprintf(char *buffer, volatile const char *format_string, ...);
size_t printf(const char* format_string, ...);
int putchar(int string_1);
size_t puts(const char*);
char *to_string (int number);
char* toupper(char* input);
char *hex_to_string(int* argp, int length, char *result, int radix);
char *itoa(int value, char *result, int base);


 
#ifdef __cplusplus
}
#endif
 
#endif