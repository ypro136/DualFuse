#ifndef _STRING_H
#define _STRING_H 1
 
#include <sys/cdefs.h>
 
#include <stddef.h>
 
#ifdef __cplusplus
extern "C" {
#endif
 
int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void *bufptr, int value, size_t size);
size_t strlen(const char*);
void strcpy(const char *destination_string, const char *source_string);
char* strcat(const char* destination_string,const char* source_string );
 
#ifdef __cplusplus
}
#endif
 
#endif