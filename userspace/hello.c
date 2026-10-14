#include <stdio.h>

int main(int argc, char** argv) {
    printf("hello from DualFuse userspace\n");
    printf("argc: %d\n", argc);
    for (int argument_index = 0; argument_index < argc; argument_index++)
        printf("argv[%d]: %s\n", argument_index, argv[argument_index]);
    return 0;
}