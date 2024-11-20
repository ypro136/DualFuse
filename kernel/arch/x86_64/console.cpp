#include <console.h>

#include <framebufferutil.h>
#include <bootloader.h>

#include <utility.h>
#include <psf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int bg_color = 0x1B262C;
int textcolor = 0xBBE1FA;

bool console_initialized = false;
bool is_char[200][200] = {0};


uint32_t width = 0;
uint32_t height = 0;
uint32_t screen_width_char = 0;


#define CHAR_HEIGHT (psf->height)
#define CHAR_WIDTH (8)


void draw_rect(int x, int y, int w, int h, int rgb) 
{ // Draw a filled rectangle
  unsigned int offset = (x + y * screen_width); // Finding the location of the pixel in the video array
  for (int i = 0; i < h; i++) 
  {
    for (int j = 0; j < w; j++) 
    { // color each line
    int *address_pointer = (int*)tempframebuffer->address;
      address_pointer[offset + j] = rgb;
    }
    offset += screen_width; // switch to the beginnering of next line
  }
}


void console_initialize() 
{
  width = 0;
  height = 0;

  screen_width_char = screen_width / CHAR_WIDTH;

  psfLoadDefaults();
  console_initialized = true;
}

bool scroll_console(bool check) {
  if (check && !(height >= (screen_height - CHAR_HEIGHT)))
    return false;

  for (int y = CHAR_HEIGHT; y < screen_height; y++) {
    {
      void       *dest = (void *)(((size_t)tempframebuffer) + (y - CHAR_HEIGHT) * pitch);
      const void *src = (void *)(((size_t)tempframebuffer) + y * pitch);
      memcpy(dest, src, screen_width);
    }
  }
  draw_rect(0, screen_height - CHAR_HEIGHT, screen_width, CHAR_HEIGHT, bg_color);
  height -= CHAR_HEIGHT;

  return true;
}

bool cursorHidden = false;

void eraseBull() {
  if (cursorHidden)
    return;
  draw_rect(width, height, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
}

void updateBull() {
  if (cursorHidden)
    return;
  if (width >= screen_width - CHAR_WIDTH) {
    bool neededScrolling = scroll_console(true);
    if (!neededScrolling)
      height += CHAR_HEIGHT;
    width = 0;
  }
  draw_rect(width, height, CHAR_WIDTH, CHAR_HEIGHT, textcolor);
}

void clear_screen() {
  width = 0;
  height = 0;
  draw_rect(0, 0, screen_width, screen_height, bg_color);
  updateBull();
}

void changeTextColor(int rgb) {
  textcolor = rgb;

}

void changeBg(int rgb) {
  bg_color = rgb;

}

void changeColor(int color) {
  textcolor = color;

}

uint32_t getConsoleX() { return width; }
uint32_t getConsoleY() { return height; }

void setConsoleX(uint32_t x) {
  eraseBull();
  width = x;
  updateBull();
}
void setConsoleY(uint32_t y) {
  eraseBull();
  height = y;
  updateBull();
}

void drawCharacter(int charnum) {
  if (!charnum)
    return;

//   if (ansiHandle(charnum))
//     return;

  if (width > (screen_width - CHAR_WIDTH * 2)) {
    width = 0;
    height += CHAR_HEIGHT;
  }

  scroll_console(true);

  switch (charnum) {
  case -1:
    draw_rect(width, height, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
    width += CHAR_WIDTH;
    break;
  case '\n':
    // drawCharacter(-1);
    eraseBull();
    is_char[width/CHAR_WIDTH][height/CHAR_HEIGHT] = 0;
    width = 0;
    height += CHAR_HEIGHT;
    break;
  case 0xd:
    eraseBull();
    width = 0;
    break;
  case 0xf: // todo: alternative character sets
    break;
  case '\b':
    eraseBull();
    do{
      if (width == 0)
      {
        height -= CHAR_HEIGHT;
        width = (screen_width_char - 2) * CHAR_WIDTH;
      }
      else
      {
        width -= CHAR_WIDTH;
      }
    }while (is_char[width/CHAR_WIDTH][height/CHAR_HEIGHT] == 0);
    draw_rect(width, height, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
    break;
  case '\t':
    for (int i = 0; i < 4; i++)
      drawCharacter(' ');
    break;
  case ' ':
    eraseBull();
    psfPutC(charnum, width, height, textcolor);
    is_char[width/CHAR_WIDTH][height/CHAR_HEIGHT] = 0;    
    width += CHAR_WIDTH;
    break;
  default:
    eraseBull();
    psfPutC(charnum, width, height, textcolor);
    is_char[width/CHAR_WIDTH][height/CHAR_HEIGHT] = 1;
    width += CHAR_WIDTH;
    break;
  }
  updateBull();
}

void printfch(char character) {
  drawCharacter(character);
}

// printf.c uses this
void putchar_(char c) { printfch(c); }

