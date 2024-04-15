#include <stdio.h>
#include <string.h>


/**
 * Converts an integer to a string.
 *
 * Handles negative numbers by prepending a '-' character.
 * Uses an unsigned integer copy for digit conversion to avoid issues with INT_MIN.
 * Digits are computed right-to-left and stored in the string.
 * The string is then reversed in-place to get the final result.
 * Special cases 0 to return '0' without computing digits.
 */
char *to_string(int number)
{
    char digits[32];
    printf("digits at start of to_string: %s\n", digits);
    printf("\n");
    itoa(number, digits, 10);
    printf("digits at end of to_string: %s\n", digits);
    printf("\n");
    return digits; // TODO: fix empty return string
    
}

        /**
 * Converts an integer value to a string in the given base.
 *
 * Allocates memory for the result string. Handles negative numbers
 * by prepending a '-' character. Uses an unsigned copy of the value
 * to compute digits to avoid issues with INT_MIN. Computes digits
 * right-to-left and stores them in the result string, then reverses
 * the string in-place to get the final result. Returns '0' for 0.
 *
 * @param value The integer value to convert
 * @param base The base to use for conversion, between 2 and 36
 * @return The string representation of the integer in the given base
 */
/**
* C++ version 0.4 char* style "itoa":
* Written by Lukás Chmela
* Released under GPLv3.

*/
char *itoa(int value, char *result, int base)
{
    

    // check that the base if valid
    if (base < 2 || base > 36)
    {
        *result = '\0';
        return result;
    }

    char *ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while (value);

    // Apply negative sign
    if (tmp_value < 0)
        *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr)
    {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}
