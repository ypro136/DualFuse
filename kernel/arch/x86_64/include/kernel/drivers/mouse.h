#include <dev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_PORT 0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT 0x02
#define MOUSE_BBIT 0x01
#define MOUSE_WRITE 0xD4
#define MOUSE_F_BIT 0x20
#define MOUSE_V_BIT 0x08

#define MOUSE_TIMEOUT 100000

// Mouse acceleration settings
#define MOUSE_ACCEL_THRESHOLD 5   // speed (pixels/packet) above which accel kicks in
#define MOUSE_ACCEL_FACTOR    2   // multiplier when above threshold
#define MOUSE_BASE_DIVISOR    5   // base speed divisor (was 5, lower = faster)

extern int mouse_position_x;
extern int mouse_position_y;
extern bool clickedLeft;
extern bool clickedRight;



void initiateMouse();
static inline int apply_mouse_accel(int delta);
void mouseIrq();

extern DevInputEvent *mouseEvent;


#endif