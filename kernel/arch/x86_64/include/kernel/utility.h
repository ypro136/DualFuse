#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

#ifndef UTILITY_H
#define UTILITY_H



#define CEILING_DIVISION(a,b) (((a + b) - 1) / b)

#define COMBINE_64(higher, lower) (((uint64_t)(higher) << 32) | (uint64_t)(lower))
#define SPLIT_64_HIGHER(value) ((value) >> 32)
#define SPLIT_64_LOWER(value) ((value) & 0xFFFFFFFF)

#define SPLIT_32_HIGHER(value) ((value) >> 16)
#define SPLIT_32_LOWER(value) ((value) & 0xFFFF)


// TODO: move me to linux.h
// include/asm-generic/ioctl.h
#define _IOC_NRBITS 8
#define _IOC_TYPEBITS 8

/*
 * Let any architecture override either of the following before
 * including this file.
 */

#ifndef _IOC_SIZEBITS
#define _IOC_SIZEBITS 14
#endif

#ifndef _IOC_DIRBITS
#define _IOC_DIRBITS 2
#endif

#define _IOC_NRMASK ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK ((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT 0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT (_IOC_SIZESHIFT + _IOC_SIZEBITS)

/*
 * Direction bits, which any architecture can choose to override
 * before including this file.
 *
 * NOTE: _IOC_WRITE means userland is writing and kernel is
 * reading. _IOC_READ means userland is reading and kernel is writing.
 */

#ifndef _IOC_NONE
#define _IOC_NONE 0U
#endif

#ifndef _IOC_WRITE
#define _IOC_WRITE 1U
#endif

#ifndef _IOC_READ
#define _IOC_READ 2U
#endif

#define _IOC(dir, type, nr, size)                                              \
  (((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) |                     \
   ((nr) << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))

#define _IOC_TYPECHECK(t) (sizeof(t))

/*
 * Used to create numbers.
 *
 * NOTE: _IOW means userland is writing and kernel is reading. _IOR
 * means userland is reading and kernel is writing.
 */
#define _IO(type, nr) _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, size)                                                   \
  _IOC(_IOC_READ, (type), (nr), (_IOC_TYPECHECK(size)))
#define _IOW(type, nr, size)                                                   \
  _IOC(_IOC_WRITE, (type), (nr), (_IOC_TYPECHECK(size)))
#define _IOWR(type, nr, size)                                                  \
  _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), (_IOC_TYPECHECK(size)))
#define _IOR_BAD(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#define _IOW_BAD(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR_BAD(type, nr, size)                                              \
  _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))

/* used to decode ioctl numbers.. */
#define _IOC_DIR(nr) (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr) (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
// TODO.


extern bool tasksInitiated;

extern bool systemDiskInit;




struct interrupt_registers
{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_number, error_code;
    uint32_t eip, csm, eflags, useresp, ss;
};

char *strdup(char *source);

void out_port_byte(uint16_t port, uint8_t data);

char in_port_byte(uint16_t port);

uint32_t inportlong(uint16_t portid);
void     out_port_long(uint16_t portid, uint32_t value);

void initiateSSE();

void hand_control();

bool bitmapGenericGet(uint8_t *bitmap, size_t index);
void bitmapGenericSet(uint8_t *bitmap, size_t index, bool set);

void     atomicBitmapSet(volatile uint64_t *bitmap, unsigned int bit);
void     atomicBitmapClear(volatile uint64_t *bitmap, unsigned int bit);
uint64_t atomicBitmapGet(volatile uint64_t *bitmap);

uint8_t  atomicRead8(volatile uint8_t *target);
uint16_t atomicRead16(volatile uint16_t *target);
uint32_t atomicRead32(volatile uint32_t *target);
uint64_t atomicRead64(volatile uint64_t *target);

void atomicWrite8(volatile uint8_t *target, uint8_t value);
void atomicWrite16(volatile uint16_t *target, uint16_t value);
void atomicWrite32(volatile uint32_t *target, uint32_t value);
void atomicWrite64(volatile uint64_t *target, uint64_t value);

// simple rand implimintation
void srand(unsigned int seed);
int rand(void);

#endif