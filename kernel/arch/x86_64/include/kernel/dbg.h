/*
 * x86_64/dbg.h
 *
 * Copyright (C) 2021 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Mini debugger
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize exceptions
 */
void dbg_init(void);

/**
 * Save registers
 */
void dbg_saveregs(void);

/**
 * Invoke the debugger
 */
void dbg_main(unsigned long excnum);

/**
 * Restore registers
 */
void dbg_loadregs(void);

#ifdef __cplusplus
}
#endif

/**
 * Trigger an exception in code
 */
#define breakpoint __asm__ __volatile__("int $3")
