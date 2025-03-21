#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
 
#include <tty.h>
 
#include "vga.h"
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xC00B8000;


static volatile size_t terminal_row;
static volatile size_t terminal_column;
static uint8_t terminal_color;
static volatile uint16_t* terminal_buffer;

 
/**
 * Initializes the terminal by clearing the screen, setting the cursor position
 * to 0,0, setting the default color, and initializing the terminal buffer to
 * point to the VGA text mode memory. It also fills the entire screen with
 * spaces in the default color.
 */
void terminal_initialize(void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}
 
/**
 * Sets the text color for the terminal.
 *
 * @param color The color value to set.
 */
void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}
 
/**
 * Puts a character entry at the given x,y position on the terminal.
 *
 * @param entry_character The character to write.
 * @param color The color attribute.
 * @param x The column position.
 * @param y The row position.
 */
void terminal_putentryat(unsigned char entry_character, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(entry_character, color);
}

/**
 * Scrolls the terminal screen up by one line.
 *
 * @param line The line number to start scrolling from.
 */
void terminal_scroll(int line)
{
	char *loop, *preloop;
	char c;

	for (loop = (char *)(0xB8000 + line * (VGA_WIDTH * 2)); loop < (char *)(0xB8000 + (line + 1) * (VGA_WIDTH * 2)); loop++)
	{
		c = *loop;
		preloop = loop - (VGA_WIDTH * 2);
		*preloop = vga_entry(c, terminal_color);
		;
	}
}
 
/**
 * Scrolls the entire terminal screen up by one line.
 * Loops through each row and scrolls it up.
 */
void terminal_scroll_all()
{
	for (int i = 0; i <= VGA_HEIGHT - 1; i++)
	{
		terminal_scroll(i);
	}
}

/**
 * Clears the last line of the terminal by setting all character cells to blank.
 */
void terminal_delete_last_line()
{
	int i, *ptr;

	for (i = 0; i < VGA_WIDTH * 2; i++)
	{
		ptr = (int *)(0xB8000 + (VGA_WIDTH * 2) * (VGA_HEIGHT - 1) + i);
		*ptr = 0;
	}
}


/**
 * Advances the terminal to a new line.
 * Increments the terminal row counter and resets the column counter.
 * If the new row exceeds the height of the terminal, scrolls the entire screen up by one line.
 */
void new_line()
{
	terminal_row++;
	terminal_column = 0;
	if (terminal_row >= VGA_HEIGHT)
	{
		terminal_scroll_all();
		terminal_row--;
		terminal_delete_last_line();
	}
}
 
/**
 * Prints a character to the terminal at the current cursor position.
 * Handles newline characters and scrolling the screen if needed.
 */
void terminal_putchar(char character)
{
	unsigned char character_unsigned = character;
	switch (character)
	{
	case '\n':
		new_line();
		break;
	case '\b':
		if (terminal_column == 0 && terminal_row != 0)
		{
			terminal_row--;
			terminal_column = VGA_WIDTH;
		}
		terminal_putentryat(' ', terminal_color, --terminal_column, terminal_row);
		break;
	case '\r':
		terminal_column = 0;
		break;
	case '\t':
	{
		if (terminal_column == VGA_WIDTH)
		{
			new_line();
		}
		uint16_t tab_length = 4;
		while (tab_length != 0)
		{
			terminal_putentryat(' ', terminal_color, terminal_column++, terminal_row);
			tab_length--;
		}
		break;
	}
	default:
		terminal_putentryat(character_unsigned, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH)
		{
			new_line();
		}
		break;
	}
	
}
 
/**
 * Writes the given data buffer to the terminal.
 * Iterates through each character in the data and prints it using terminal_putchar().
 */
void terminal_write(char data)
{
	terminal_putchar(data);
}
 
/**
 * Writes the given string to the terminal by passing it to terminal_write().
 *
 * @param data The string to write to the terminal.
 */
void terminal_writestring(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		terminal_write(*data);
	}
}
