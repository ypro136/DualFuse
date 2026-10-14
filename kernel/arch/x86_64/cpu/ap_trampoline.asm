BITS 16
org 0x8000

start_16:
    cli
    cld
    xor ax, ax
    mov ds, ax

    mov byte [0x500], 0xA0
.wait_stage0:
    cmp byte [0x501], 1
    jne .wait_stage0

    mov al, 0xFF
    out 0xA1, al
    out 0x21, al

    mov byte [0x500], 0xA1
.wait_stage1:
    cmp byte [0x502], 1
    jne .wait_stage1

    lgdt [tmp_gdt_ptr]

    mov byte [0x500], 0xA2
.wait_stage2:
    cmp byte [0x503], 1
    jne .wait_stage2

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:start_32

BITS 32
start_32:
    mov byte [0x500], 0xA3
.wait_stage3:
    cmp byte [0x504], 1
    jne .wait_stage3

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lidt [zero_idt]

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov byte [0x500], 0xA4
.wait_stage4:
    cmp byte [0x505], 1
    jne .wait_stage4

    ; Bug 2 fix: data_header label already includes the org 0x8000 base,
    ; so [data_header + N] is the correct physical address — no extra 0x8000.
    mov eax, [data_header + 0]
    mov cr3, eax

    mov byte [0x500], 0xA5
.wait_stage5:
    cmp byte [0x506], 1
    jne .wait_stage5

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11) | (1 << 0)
    wrmsr

    mov byte [0x500], 0xA6
.wait_stage6:
    cmp byte [0x507], 1
    jne .wait_stage6

    mov byte [0x500], 0xAA
.wait_stage6a:
    cmp byte [0x508], 1
    jne .wait_stage6a

    mov eax, [0xFEE00320]
    or eax, 1 << 16
    mov [0xFEE00320], eax

    mov eax, [0xFEE00350]
    or eax, 1 << 16
    mov [0xFEE00350], eax

    mov eax, [0xFEE00360]
    or eax, 1 << 16
    mov [0xFEE00360], eax

    mov eax, [0xFEE00340]
    or eax, 1 << 16
    mov [0xFEE00340], eax

    mov byte [0x500], 0xAB
    ; Bug 1 fix: stage index 8 → go flag is ap_go_base[8] = 0x501+8 = 0x509
.wait_stage8_lapic:
    cmp byte [0x509], 1
    jne .wait_stage8_lapic

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    jmp near .paging_flushed
.paging_flushed:

    mov byte [0x500], 0xAC
    ; Bug 1 fix: stage index 9 → go flag is 0x501+9 = 0x50A
.wait_stage9_paging:
    cmp byte [0x50A], 1
    jne .wait_stage9_paging

    mov byte [0x500], 0xA7
    ; Bug 1 fix: stage index 10 → go flag is 0x501+10 = 0x50B
.wait_stage10_prejump:
    cmp byte [0x50B], 1
    jne .wait_stage10_prejump

    jmp 0x18:start_64

BITS 64
start_64:
    mov byte [0x500], 0xA8
    ; Bug 1 fix: stage index 11 → go flag is 0x501+11 = 0x50C
.wait_stage11_64bit:
    cmp byte [0x50C], 1
    jne .wait_stage11_64bit

    ; Bug 2 fix: all data_header reads use [data_header + N], no extra 0x8000
    mov rsp, qword [data_header + 8]
    mov byte [data_header + 44], 1

    mov ax, word [data_header + 24]
    mov word [bsp_gdt_ptr], ax
    mov rax, qword [data_header + 26]
    mov qword [bsp_gdt_ptr + 2], rax
    lgdt [bsp_gdt_ptr]

    ; Bug 3 fix: retfq needs two 8-byte slots — [RSP]=RIP, [RSP+8]=CS.
    ; push ax uses a 66h prefix and only decrements RSP by 2, corrupting
    ; the retfq frame.  Zero-extend CS into rax and push qword CS then RIP.
    xor rax, rax
    mov ax, word [data_header + 45]
    push rax
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:

    mov ax, word [data_header + 34]
    mov word [bsp_gdt_ptr], ax
    mov rax, qword [data_header + 36]
    mov qword [bsp_gdt_ptr + 2], rax
    lidt [bsp_gdt_ptr]

    mov ax, word [data_header + 47]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, qword [data_header + 8]

    mov rax, qword [data_header + 16]
    mov rdi, qword [data_header + 8]    ; first argument = per‑AP stack top
    mov rax, qword [data_header + 16]   ; C entry function pointer
    call rax

    cli
    hlt

tmp_gdt:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xAF, 0x00
tmp_gdt_end:

tmp_gdt_ptr:
    dw tmp_gdt_end - tmp_gdt - 1
    dd tmp_gdt

zero_idt:
    dw 0
    dd 0

bsp_gdt_ptr:
    dw 0
    dq 0

data_header:
    .bsp_cr3_phys:        dq 0
    .per_ap_stack_top:    dq 0
    .c_entry_virt:        dq 0
    .bsp_gdt_limit:       dw 0
    .bsp_gdt_base:        dq 0
    .bsp_idt_limit:       dw 0
    .bsp_idt_base:        dq 0
    .commword:            db 0
    .bsp_cs_selector:     dw 0
    .bsp_ds_selector:     dw 0