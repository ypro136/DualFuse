#ifndef _STDIO_H
#define _STDIO_H 1
 
#include <sys/cdefs.h>

 
#define EOF (-1)
 
#ifdef __cplusplus
extern "C" {
#endif
 
int printf(const char* format_string, ...);
int putchar(int);
int puts(const char*);
const char* to_string (int number);


 
#ifdef __cplusplus
}
#endif
 
#endif