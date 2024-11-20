#include <stdio.h>
 
#if defined(__is_libk)
#if defined(__is_i686)
#include <tty.h>
#else
#include <console.h>
#endif
#include <serial.h>
#endif


/**
 * Writes a character to stdout.
 *
 * Writes the character ic to stdout. In the default case, this calls the
 * unimplemented stdio write. When building for the kernel, this writes
 * directly to the terminal using the kernel terminal_write function.
 *
 * @param character The character to write.
 * @return The character written, same as ic.
 */
int putchar(int character)
{
#if defined(__is_libk)
	#if defined(__is_i686)
	terminal_write(character);
	#else
	if (console_initialized)
	{
		putchar_(character);
	}
	#endif
	serial_write(character);
#else
	// TODO: Implement stdio and the write system call.
#endif
	return character;
}