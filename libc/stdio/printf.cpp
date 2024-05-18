#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

    // sprintf(): dose the work!!!
    // snprintf(): Wraps over sprintf()
    // printf(): Wraps over snprintf()


    // sprintf(): Write formatted data to buffer
    // snprintf(): Write formatted data to buffer, limits maximum characters written
	// printf(): Write formatted data to terminal


 
/**
 * Prints the given data buffer to stdout.
 *
 * @param data Pointer to the data buffer to print.
 * @param length Length of the data buffer in bytes.
 * @return true on success, false on error.
 */
static bool print(const char *data, size_t length)
{
	const char *bytes = data;
	for (size_t i = 0; i < length; i++)
	{
		if (putchar(bytes[i]) == EOF)
		{
			return false;
		}
	}
	return true;
}

/**
 * Writes formatted output to a character string buffer.
 *
 * This function parses the provided format string containing text and
 * format specifiers like %d, %c, etc. It uses the additional arguments
 * provided in va_list to replace the format specifiers with actual values.
 *
 * The formatted output is written to the buffer parameter.
 *
 * @param buffer Pointer to character buffer to write output to.
 * @param format_string Format string containing text and format specifiers.
 * @param arguments va_list containing additional arguments to replace format specifiers.
 * @return Number of characters written to the buffer.
 */
int sprintf(char *buffer, volatile const char *format_string, va_list arguments)
{
	// arguments should have been initialized by va_start at some point before the call

	

	char current = *format_string;

	bool format_specifier = false;

	char *buffer_pointer = buffer;

	int written = 0;

	while (current != '\0')
	{

		current = *format_string;

		switch (current)
		{
		case '%':
			if (!format_specifier)
			{
				format_specifier = true;
			}
			else if (format_specifier)
			{
				*buffer = current;
				format_specifier = false;
				buffer++;
			}
			
			break;

		case 'd':

			if (format_specifier)
			{
				// add a int in decimal.
				int temp_int = va_arg(arguments, int);

				char *temp_string;
				itoa(temp_int, temp_string, 10);

				int temp_string_length = strlen(temp_string);
				*buffer = '\0';
				strcat(buffer_pointer, temp_string);
				buffer += temp_string_length;

				format_specifier = false;
			}
			else
			{
				*buffer = current;
				buffer++;
			}
			break;

		case 's':
			if (format_specifier)
			{
				// add a string.
				const char *temp_string = va_arg(arguments, const char *);
				int temp_string_length = strlen(temp_string);
				*buffer = '\0';
				strcat(buffer_pointer, temp_string);
				buffer += temp_string_length;

				format_specifier = false;
			}
			else
			{
				*buffer = current;
				buffer++;
			}
			break;

		case 'c':
			if (format_specifier)
			{
				// add a char ,keep in mind 'char' is promoted to 'int' when passed through '...' .
				*buffer = va_arg(arguments, int);

				format_specifier = false;
			}
			else
			{
				*buffer = current;
			}
			buffer++;
			break;

		case 'x':
			if (format_specifier)
			{
				// add a int in hexadecimal.
				int temp_int = va_arg(arguments, int);

				char *temp_string;
				itoa(temp_int, temp_string, 16);

				int temp_string_length = strlen(temp_string);
				*buffer = '\0';
				strcat(buffer_pointer, temp_string);
				buffer += temp_string_length;

				format_specifier = false;
			}
			else
			{
				*buffer = current;
				buffer++;
			}
			break;

		case ' ':
			if (format_specifier)
			{
				// skip spaces.

				*buffer = current;

			}
			else
			{
				*buffer = current;
			}
			buffer++;
			break;

		default:
			*buffer = current;
			buffer++;
			format_specifier = false;
			break;
		}

		format_string++;
	}
	*buffer = '\0';
	written = strlen(buffer_pointer);

	// arguments is expected to be released by va_end at some point after the call.

	return written;
}

int snprintf(char* string, const char* format, va_list arguments) 
{
    int written = 0;

    return written;
}
 
/**
 * Prints formatted output to the console.
 *
 * This acts as a wrapper around sprintf() to format a string, and print()
 * to write the formatted string to the console.
 *
 * @param format_string Format string containing text and format specifiers.
 * @param ... Variable arguments matching the format specifiers in format_string.
 * @return Number of characters printed.
 */
size_t printf(const char *format_string, ...)
{
	va_list arguments;
	va_start(arguments, format_string);

	char string[1024];

	int written = 0;

	written = sprintf(string, format_string, arguments);
	if (print(string, written))
	{
		va_end(arguments);
		return written;
	}
	else
	{
		va_end(arguments);
		return -1;
	}

}
