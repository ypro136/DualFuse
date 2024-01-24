#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>

bool is_all_true(bool *var, int size)
{
    for (int i = 0; i <= size; i++)
    {
        if (!var[i])
        {
            return var[i]; // or false
        }
    }
    return true;
}

void incrment (bool *var, int index)
{
    int temp = index - 1;
    if (var[index])
    {
        var[index] = false;
        incrment (var, temp);
        return;
    }
    else
    {
        var[index] = true;
        return;
    }
}

void print_binary(bool var)
{
	if (var){printf("1");} else {printf("0");}
}

int temp = 5405873;
 
extern "C" void kernel_main(void) {
	terminal_initialize();

	// printf("Hello, kernel World!\n");

	// char *string_1 = " yasser ", *string_2 = "hatem";
	// strcpy(string_1, string_2);

	// printf(string_1);
	// printf("\n");
	// printf("===============\n");

	// printf("Hello, {ss} World!\n", " yasser ", "hatem");

	// printf("\n");
	// printf("===============\n");


	// if (temp > 0){printf("temp is 5\n");}
	// if (temp == 0){printf("temp is 0\n");}
	// printf("temp is %d\n", temp);
	// if (temp < 0){printf("temp is -5\n");}
	// printf("Hello, I should be end!\n");

	int num = 4;

    bool var[100];
	bool stop = true;

    int states = 2;


	if (num >= 0){stop = false;}


    while (!stop)
    {
        states = 2;

		/*
		printf("\n");
        //cout << "\n";
        
        for (int i = 0; i < num; i++)
        {
			printf("|var ");
			print_binary(i);
            //cout << "|" << "var " << i;
        }

		printf("|\n");
        //cout << "|\n";
		*/


        for (int i = 0; i < num; i++)
        {
            states *= 2;

            var[i] = false;
        }

            bool togle = false;
        for (int j = 1 ; j < states; j++)
        {
            if (togle)
            {
				
				printf("|\n");
                //cout << "|\n";

                incrment (var, num);
                togle = false;
            }
            else
            {
                for (int i = 0; i < num; i++)
                {
					printf("|  ");
					print_binary(var[i]);
					printf("  ");
                    //cout << "|  " << var[i] << "  ";
                }
                incrment (var, num);
                togle = true;
            }
            
        }

		printf("|\n\n--------------------------\n\n");
		//cout << "|\n\n--------------------------\n\n";

		if (is_all_true(var, num))
		{
			stop = true;
		}
    }
}