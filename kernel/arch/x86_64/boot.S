; Declare constants for the multiboot header.
%define ALIGN    (1 << 0)             ; align loaded modules on page boundaries
%define MEMINFO  (1 << 1)             ; provide memory map
%define VBE      0                    ; VBE video mode support
%define FLAGS    (ALIGN | MEMINFO)    ; this is the Multiboot 'flag' field
%define MAGIC    0x1BADB002           ; 'magic number' lets bootloader find the header
%define CHECKSUM -(MAGIC + FLAGS)     ; checksum of above, to prove we are multiboot

BITS 32

; Declare a header as in the Multiboot Standard.
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0

    dd 0
    dd 800
    dd 600
    dd 32


; Declare external symbols
extern _init


; The kernel entry point.
section .boot
global _start
_start:

    mov ecx, cr0
    or ecx, 0x80000000 

    mov cr0, ecx

    mov eax, higher_half
    jmp [eax]
;    jmp higher_half


; Reserve a stack for the initial thread.
section .bss
align 16
stack_bottom:
    resb 16384 * 8
stack_top:


section .text
higher_half:

    cli
    mov esp, stack_top

    ; Call the global constructors.
    call _init

    push ebx
    push eax

    xor ebp, ebp

    extern kernel_main
    
    ; Transfer control to the main kernel.
    call kernel_main

; Hang if kernel_main unexpectedly returns.
halt:
    hlt
    jmp halt

    times 256-4 dd 0
