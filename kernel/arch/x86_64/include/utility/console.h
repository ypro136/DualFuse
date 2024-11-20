 #include <types.h>

#include <limine.h>


#ifndef CONSOLE_H
#define CONSOLE_H


extern int bg_color;
extern int textcolor;
extern bool console_initialized;

#define TTY_CHARACTER_WIDTH 8
#define TTY_CHARACTER_HEIGHT 16


void console_initialize();
void drawCharacter(int charnum);


// Internal variables
void     changeBg(int rgb);
void     changeTextColor(int rgb);
uint32_t getConsoleX();
uint32_t getConsoleY();
void     setConsoleX(uint32_t x);
void     setConsoleY(uint32_t y);

// Control the console's pointer
void eraseBull();
void updateBull();

void clear_screen();

void printfch(char character);
void putchar_(char c);

#endif
