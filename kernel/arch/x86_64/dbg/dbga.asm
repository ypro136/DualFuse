;
; x86_64/dbga.asm
;
; Copyright (C) 2021 bzt (bztsrc@github)
;
; Permission is hereby granted, free of charge, to any person
; obtaining a copy of this software and associated documentation
; files (the "Software"), to deal in the Software without
; restriction, including without limitation the rights to use, copy,
; modify, merge, publish, distribute, sublicense, and/or sell copies
; of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be
; included in all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
; EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
; MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
; NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
; HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
; WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
; DEALINGS IN THE SOFTWARE.
;
; @brief Assembly routines for the mini debugger

global dbg_init
global dbg_saveregs
global dbg_loadregs
extern dbg_regs
extern dbg_decodeexc
extern dbg_main

section .text

;
; Set up dummy exception handlers to call the mini debugger
;
dbg_init:
    lea    rsi, [dbg_idt]
    lea    rdi, [dbg_isrs]
    ; exception gates
    mov    cx, 32
    xor    rax, rax
    mov    ax, cs
    or     eax, 0x0E000000
    shl    rax, 16
    mov    rdx, rax
.loop1:
    mov    eax, edi
    and    eax, 0xFFFF0000
    shl    rax, 48
    mov    ax, di
    or     rax, rdx
    stosq
    mov    rax, rdi
    shr    rax, 32
    stosq
    add    rdi, 32
    dec    cx
    jnz    .loop1
    ; interrupt gates
    xor    rax, rax
    mov    cx, 16
    or     eax, 0x0F000000
    shl    rax, 16
    or     rdx, rax
.loop2:
    mov    eax, edi
    and    eax, 0xFFFF0000
    shl    rax, 48
    mov    ax, di
    or     rax, rdx
    stosq
    mov    rax, rdi
    shr    rax, 32
    stosq
    add    rdi, 32
    dec    cx
    jnz    .loop2
    lidt    [dbg_idt_value]
    ret

;
; Save registers before we call any C code
;
dbg_saveregs:
    push   rbp                      ; Save base pointer
    mov    rbp, rsp                ; Set up new stack frame
    
    mov    [rel dbg_regs +  0], rax
    mov    [rel dbg_regs +  8], rbx
    mov    [rel dbg_regs + 16], rcx
    mov    [rel dbg_regs + 24], rdx
    mov    [rel dbg_regs + 32], rsi
    mov    [rel dbg_regs + 40], rdi
    mov    rax, [rbp + 48]         ; orig rsp (40 + 8 for our push rbp)
    mov    [rel dbg_regs + 48], rax
    mov    [rel dbg_regs + 29*8], rax
    mov    [rel dbg_regs + 56], rbp
    mov    [rel dbg_regs + 64], r8
    mov    [rel dbg_regs + 72], r9
    mov    [rel dbg_regs + 80], r10
    mov    [rel dbg_regs + 88], r11
    mov    [rel dbg_regs + 96], r12
    mov    [rel dbg_regs + 104], r13
    mov    [rel dbg_regs + 112], r14
    mov    [rel dbg_regs + 120], r15
    mov    rax, [rbp + 24]         ; orig rip (16 + 8 for our push rbp)
    mov    [rel dbg_regs + 128], rax
    mov    [rel dbg_regs + 31*8], rax

    mov    rax, cr0
    mov    [rel dbg_regs + 136], rax
    mov    rax, cr2                ; Note: Removed cr1 access as it doesn't exist
    mov    [rel dbg_regs + 152], rax
    mov    rax, cr3
    mov    [rel dbg_regs + 160], rax
    mov    rax, cr4
    mov    [rel dbg_regs + 168], rax
    mov    rax, [rbp + 40]         ; rflags (32 + 8 for our push rbp)
    mov    [rel dbg_regs + 176], rax
    mov    rax, [rbp + 16]         ; exception code (8 + 8 for our push rbp)
    mov    [rel dbg_regs + 184], rax
    
    pop    rbp                     ; Restore base pointer
    ret

;
; Restore registers after we call C code
;
dbg_loadregs:
    mov    rax, [dbg_regs +  0]
    mov    rbx, [dbg_regs +  8]
    mov    rcx, [dbg_regs + 16]
    mov    rdx, [dbg_regs + 24]
    mov    rsi, [dbg_regs + 32]
    mov    rdi, [dbg_regs + 40]
    mov    rbp, [dbg_regs + 56]
    mov    r8,  [dbg_regs + 64]
    mov    r9,  [dbg_regs + 72]
    mov    r10, [dbg_regs + 80]
    mov    r11, [dbg_regs + 88]
    mov    r12, [dbg_regs + 96]
    mov    r13, [dbg_regs + 104]
    mov    r14, [dbg_regs + 112]
    mov    r15, [dbg_regs + 120]
    ret

%macro isr 2
    align 32
    cli
%if %2 == 0
    push    0
%endif
    call    dbg_saveregs
    add     rsp, 8
    mov     rdi, %1
    call    dbg_main
    mov     rax, [dbg_regs + 31*8]  ; update the return address
    mov     [rsp], rax
    call    dbg_loadregs
    iretq
%endmacro

    align 32
dbg_isrs:
    isr  0, 0
    isr  1, 0
    isr  2, 0
    isr  3, 0
    isr  4, 0
    isr  5, 0
    isr  6, 0
    isr  7, 0
    isr  8, 1
    isr  9, 0
    isr 10, 1
    isr 11, 1
    isr 12, 1
    isr 13, 1
    isr 14, 1
    isr 15, 0
    isr 16, 0
    isr 17, 1
    isr 18, 0
    isr 19, 0
    isr 20, 0
    isr 21, 0
    isr 22, 0
    isr 23, 0
    isr 24, 0
    isr 25, 0
    isr 26, 0
    isr 27, 0
    isr 28, 0
    isr 29, 0
    isr 30, 1
    isr 31, 0

    isr 32, 0
    isr 33, 0
    isr 34, 0
    isr 35, 0
    isr 36, 0
    isr 37, 0
    isr 38, 0
    isr 39, 0
    isr 40, 0
    isr 41, 0
    isr 42, 0
    isr 43, 0
    isr 44, 0
    isr 45, 0
    isr 46, 0
    isr 47, 0

section .data
    align 16
dbg_idt:
    times (48*16) db 0
dbg_idt_value:
    dw (dbg_idt_value-dbg_idt)-1
    dq dbg_idt
