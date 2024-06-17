#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>

class Serial {
public:
    static Serial& getInstance(uint16_t port);

    bool initialize();
    bool received();
    char read();
    bool transmit_empty();
    void write(char a);

private:
    Serial(uint16_t port);
    ~Serial();

    uint16_t port;
    static bool instanceCreated; // TODO: implement nullptr and us it rather than a bool
    static Serial* instance;
};
 
#endif