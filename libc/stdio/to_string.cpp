#include <stdio.h>
#include <string.h>


const char* to_string (int number)
{

    bool is_negative = number<0;

    unsigned int number_unsigned = is_negative ? -number : number;

    int i = 0;

    char *string;
    int string_length = 0;

    while(number_unsigned!=0)
    {
        string[i++] = number_unsigned % 10 + '0';
        number_unsigned = number_unsigned/10;
    }

    if(is_negative)
    {
        string[i++] = '-';
    }

    string[i] = '\0';

    // TODO: reverse()
    // for naw
    string_length = strlen(string);

    for(int t = 0; t < string_length/2; t++)
    {
        string[t] ^= string[string_length-t-1];
        string[string_length-t-1] ^= string[t];
        string[t] ^= string[string_length-t-1];
    }


    if(number == 0)
    {
        string[0] = '0';
        string[1] = '\0';
    }   

   
    return string;

}
