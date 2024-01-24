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


 
static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

int sprintf(char *buffer, const char* format_string, va_list arguments)
{
    // arguments should have been initialized by va_start at some point before the call

	char current = *format_string;

	bool format_specifier = false;

	char *buffer_pointer = buffer;

    int written = 0;

	while (current != '\0') 
	{

		current = *format_string;

		switch (current) {
		case '%':
			if (!format_specifier) 
			{
				format_specifier = true;
			} 
			else if (format_specifier) 
			{
				*buffer = current;
				format_specifier = false;
			}
			buffer++;
			break;

		case 'd':
			
			// TODO:add a digit.

			*buffer = current;
			
			buffer++;
			break;

		case 's':
			if (format_specifier)
			{
				// add a string.
				char *temp_string = va_arg(arguments, char*);
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
				// add a char.
				*buffer = va_arg(arguments, char);

				*buffer = current;

				format_specifier = false;
			}
			else
			{
				*buffer = current;
			}
			buffer++;
			break;

		case 'h':
			if (format_specifier)
			{
				// TODO:add a int in hex.

				*buffer = current;

				format_specifier = false;
				
			}
			else
			{
				*buffer = current;
			}
			buffer++;
			break;

		case ' ':
			if (format_specifier)
			{
				// skip spaces.

				*buffer = '%';
				buffer++;
				*buffer = current;

				format_specifier = false;
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
 
int printf(const char* format_string, ...)
 {
	va_list arguments;
	va_start(arguments, format_string);

	char *string;

	int written = 0;
 
	written = sprintf(string, format_string, arguments);
	print(string, written);
 
	va_end(arguments);
	return written;
}
