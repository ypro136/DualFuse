/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)


KERNEL_PMA = 0x00100000;
KERNEL_VMA = 0xC0000000;

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	/*. = KERNEL_PMA;*/

	/* First put the multiboot header, as it is required to be put very
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.multiboot ALIGN(4K) :
	{
		*(.multiboot)
	}

	.note.gnu.build-id ALIGN(4K):
	{
		*(.note.gnu.build-id)
	}

	.boot ALIGN(4K):
	{
		*(.boot)
	}

	/*. += KERNEL_VMA;*/

	.text ALIGN(4K): AT(ADDR(.text))
	{
		*(.text)
	}


	/* Read-only data. */
	.rodata ALIGN(4K): AT(ADDR(.rodata))
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4K): AT(ADDR(.data))
	{
		*(.data)

	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K): AT(ADDR(.bss))
	{
		*(COMMON)
		*(.bss)
	}

	_kernel_end = .;

	/* The compiler may produce other sections, put them in the proper place in
	   in this file, if you'd like to include them in the final kernel. */
}