#include <stdio.h>
 
#if defined(__is_libk)
#include <kernel/tty.h>
#include <kernel/serial.h>
#endif
 
/**
 * Writes a character to stdout.
 *
 * Writes the character ic to stdout. In the default case, this calls the
 * unimplemented stdio write. When building for the kernel, this writes
 * directly to the terminal using the kernel terminal_write function.
 *
 * @param ic The character to write.
 * @return The character written, same as ic.
 */
int putchar(int character)
{
#if defined(__is_libk)
	terminal_write(character);
	write_serial(character);
#else
	// TODO: Implement stdio and the write system call.
#endif
	return character;
}