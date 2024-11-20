#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

char* toupper(char* input) {
    char* result = input;
    
    while (*input) {
        if (*input >= 'a' && *input <= 'z') {
            *input = *input - 'a' + 'A';
        }
        input++;
    }
    
    return result;
}