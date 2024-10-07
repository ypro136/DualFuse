; Declare constants for the multiboot header.
%define ALIGN    (1 << 0)             ; align loaded modules on page boundaries
%define MEMINFO  (1 << 1)             ; provide memory map
%define VBE      0                    ; VBE video mode support
%define FLAGS    (ALIGN | MEMINFO)    ; this is the Multiboot 'flag' field
%define MAGIC    0x1BADB002           ; 'magic number' lets bootloader find the header
%define CHECKSUM -(MAGIC + FLAGS)     ; checksum of above, to prove we are multiboot

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

; Reserve a stack for the initial thread.
section .bss
align 16
stack_bottom:
    resb 16384 * 8
stack_top:

; Declare external symbols
extern _init


; The kernel entry point.
section .boot
global _start
_start:
    mov ecx, (initial_page_directory - 0xC0000000)
    mov cr3, ecx

    mov ecx, cr4
    or ecx, 0x10
    mov cr4, ecx

    mov ecx, cr0
    or ecx, 0x80000000

    mov cr0, ecx

    jmp higher_half

section .text
higher_half:

    cli
    mov esp, stack_top


    mov [multiboot_magic], eax
    mov [multiboot_pointer], ebx

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

section .data
align 4096

global multiboot_magic
multiboot_magic:
    dd 0
global multiboot_pointer
multiboot_pointer:
    dd 0

global initial_page_directory
initial_page_directory:
    dd 10000011b
    times 768-1 dd 0

    dd (0 << 22) | 10000011b
    dd (1 << 22) | 10000011b
    dd (2 << 22) | 10000011b
    dd (3 << 22) | 10000011b

    times 256-4 dd 0
