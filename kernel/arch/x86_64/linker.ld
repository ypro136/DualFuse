/* Tell the linker that we want an x86_64 ELF64 output file */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

/* We want the symbol kernel_main to be our entry point */
ENTRY(kernel_main)

/* Define the program headers we want so the bootloader gives us the right */
/* MMU permissions */
PHDRS
{
    limine_requests PT_LOAD       FLAGS((1 << 0) | (1 << 1) | (1 << 2));  /* Execute + Write + Read */
    text            PT_LOAD       FLAGS((1 << 0) | (1 << 2)) ;            /* Execute + Read */
    rodata          PT_LOAD       FLAGS((1 << 2)) ;                       /* Read only */
    data            PT_LOAD       FLAGS((1 << 1) | (1 << 2)) ;            /* Write + Read */
    dynamic         PT_DYNAMIC    FLAGS((1 << 0) | (1 << 1) | (1 << 2));  /* Execute + Write + Read */
}

SECTIONS
{
    /* We wanna be placed in the topmost 2GiB of the address space, for optimisations */
    /* and because that is what the Limine spec mandates. */
    /* Any address in this region will do, but often 0xffffffff80000000 is chosen as */
    /* that is the beginning of the region. */
    . = 0xffffffff80000000;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    /* Move to the next memory page for .kernel_start */
    . += CONSTANT(MAXPAGESIZE);
    . = ALIGN(4K);
    kernel_start = .;
    kernel_text_start = .;
    .text : {
        *(.text .text.*)
    } :text
    kernel_text_end = .;

    /* Move to the next memory page for .rodata */
    . += CONSTANT(MAXPAGESIZE);
    . = ALIGN(4K);
    kernel_rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    kernel_rodata_end = .;

    /* Move to the next memory page for .data */
    . += CONSTANT(MAXPAGESIZE);

    . = ALIGN(4K);
    kernel_data_start = .;
    .data : {
        *(.data .data.*)
    } :data

    . += CONSTANT(MAXPAGESIZE);
    . = ALIGN(4K);
    /* NOTE: .bss needs to be the last thing mapped to :data, otherwise lots of */
    /* unnecessary zeros will be written to the binary. */
    /* If you need, for example, .init_array and .fini_array, those should be placed */
    /* above this. */
    .bss : {
        . = ALIGN(4K);
        *(COMMON)
        *(.bss .bss.*)
    } :data

    . = ALIGN(4K);
    kernel_data_end = .;
    kernel_end = .;
    
    /* Discard .note.* and .eh_frame since they may cause issues on some hosts. */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}