#include <console.h>

#include <framebufferutil.h>
#include <bootloader.h>
#include <serial.h>

#include <utility.h>
#include <spinlock.h>
#include <psf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>


int bg_color = 0x1B262C;
int textcolor = 0xBBE1FA;

Spinlock LOCK_CONSOLE = ATOMIC_FLAG_INIT;
bool console_initialized = false;
bool is_char[200][200] = {0};


uint32_t cursor_position_x = 0;
uint32_t cursor_position_y = 0;
uint32_t screen_width_char = 0;


#define CHAR_HEIGHT (psf->height)
#define CHAR_WIDTH (8)


void draw_rect(int x, int y, int w, int h, int rgb) 
{
  #if defined(DEBUG_CONSOLE)
  if (!tempframebuffer || !tempframebuffer->address || screen_width == 0) return;
  #endif
  // Draw a filled rectangle
  unsigned int offset = (x + y * screen_width); // Finding the location of the pixel in the video array
  int *address_pointer = (int*)tempframebuffer->address;
  for (int i = 0; i < h; i++) 
  {
    for (int j = 0; j < w; j++) 
    { // color each line
      address_pointer[offset + j] = rgb;
    }
    offset += screen_width; // switch to the beginnering of next line
  }
}


void console_initialize() 
{
  cursor_position_x = 0;
  cursor_position_y = 0;

  screen_width_char = screen_width / CHAR_WIDTH;

  bool PSF_state = psfLoadDefaults();
  #if defined(DEBUG_CONSOLE)
  if(PSF_state)
  {
    printf("[console] PSF font loaded successfully dumping first 32 bytes\n");
    for (int i = 0; i < 32; i++) {
      printf("%02X ", ((uint8_t *)psf)[i]);
    }
    printf("\n");
  }
  else
  {
    printf("[console:error] Failed to load PSF font, using default\n");
  }
  
  printf("[console] console_initialized=%d tempframebuffer=%p psf=%p screen_width=%u\n",
    console_initialized, tempframebuffer, psf, screen_width);
  #endif

  console_initialized = true;
}

bool scroll_console() {
  if (!(cursor_position_y >= (screen_height - CHAR_HEIGHT)))
  {
    return false;
  }
  for (int y = CHAR_HEIGHT; y < screen_height; y++) 
  {
    {
      void       *dest = (void *)(((size_t)tempframebuffer) + (y - CHAR_HEIGHT) * pitch);
      const void *src = (void *)(((size_t)tempframebuffer) + y * pitch);
      memcpy(dest, src, screen_width);
    }
  }
  draw_rect(0, screen_height - CHAR_HEIGHT, screen_width, CHAR_HEIGHT, bg_color);
  cursor_position_y -= CHAR_HEIGHT;

  return true;
}

bool cursorHidden = false;

void eraseBull() {
  if (cursorHidden)
    return;
  draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
}

void updateBull() {
  if (cursorHidden)
    return;
  if (cursor_position_x >= screen_width - CHAR_WIDTH) {
    bool neededScrolling = scroll_console();
    if (!neededScrolling)
      cursor_position_y += CHAR_HEIGHT;
    cursor_position_x = 0;
  }
  draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, textcolor);
}

void clear_screen() 
{
  #if defined(DEBUG_CONSOLE)
  printf("[console] clearing screen\n");
  #endif
  cursor_position_x = 0;
  cursor_position_y = 0;
  draw_rect(0, 0, screen_width, screen_height, bg_color);
  #if defined(DEBUG_CONSOLE)
  printf("[console]:[clear_screen] draw_rect done\n");
  #endif
  updateBull();
  #if defined(DEBUG_CONSOLE)
  printf("[console]:[clear_screen] updateBull done\n");
  #endif
  
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

uint32_t getConsoleX() { return cursor_position_x; }
uint32_t getConsoleY() { return cursor_position_y; }

void setConsoleX(uint32_t x) {
  eraseBull();
  cursor_position_x = x;
  updateBull();
}
void setConsoleY(uint32_t y) {
  eraseBull();
  cursor_position_y = y;
  updateBull();
}

void drawCharacter(int charnum) {
  if (!charnum){return;}
    
//   if (ansiHandle(charnum))
//     return;

  if (cursor_position_x > (screen_width - CHAR_WIDTH * 2)) {
    cursor_position_x = 0;
    cursor_position_y += CHAR_HEIGHT;
  }

  scroll_console();

  switch (charnum) {
  case -1:
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
    cursor_position_x += CHAR_WIDTH;
    break;
  case '\n':
    // drawCharacter(-1);
    eraseBull();
    is_char[cursor_position_x/CHAR_WIDTH][cursor_position_y/CHAR_HEIGHT] = 0;
    cursor_position_x = 0;
    cursor_position_y += CHAR_HEIGHT;
    break;
  case 0xd:
    eraseBull();
    cursor_position_x = 0;
    break;
  case 0xf: // todo: alternative character sets
    break;
  case '\b':
    eraseBull();
    do{
      if (cursor_position_x == 0)
      {
        if (cursor_position_y != 0)
        {
          cursor_position_y -= CHAR_HEIGHT;
          cursor_position_x = (screen_width_char - 2) * CHAR_WIDTH;
        }
      }
      else
      {
        cursor_position_x -= CHAR_WIDTH;
      }
    }while (is_char[cursor_position_x/CHAR_WIDTH][cursor_position_y/CHAR_HEIGHT] == 0);
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);
    break;
  case '\t':
    for (int i = 0; i < 4; i++)
      drawCharacter(' ');
    break;
  case ' ':
    eraseBull();
    psfPutC(charnum, cursor_position_x, cursor_position_y, textcolor);
    is_char[cursor_position_x/CHAR_WIDTH][cursor_position_y/CHAR_HEIGHT] = 0;    
    cursor_position_x += CHAR_WIDTH;
    break;
  default:
    eraseBull();
    psfPutC(charnum, cursor_position_x, cursor_position_y, textcolor);
    is_char[cursor_position_x/CHAR_WIDTH][cursor_position_y/CHAR_HEIGHT] = 1;
    cursor_position_x += CHAR_WIDTH;
    break;
  }
  updateBull();
}

void printfch(char character) 
{
  spinlock_acquire(&LOCK_CONSOLE);
  drawCharacter(character);
  spinlock_release(&LOCK_CONSOLE);
}

// printf.c uses this
void putchar_(char c) 
{
  if (console_initialized && tempframebuffer && psf && tempframebuffer->address && screen_width != 0)
  {
    printfch(c); 
  }
  serial_write(c);
}

