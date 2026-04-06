#pragma once

#include <cstdint>
#include <window.h>

// ─── Layout constants ────────────────────────────────────────────────────────
#define CALC_DISPLAY_H   40      // height of the number display strip
#define CALC_BTN_W       48      // button width
#define CALC_BTN_H       40      // button height
#define CALC_BTN_PAD      6      // gap between buttons
#define CALC_COLS         4
#define CALC_ROWS         5

// Total client area needed (excluding title bar / border):
//   width  = COLS*(BTN_W+PAD) + PAD            = 4*54+6  = 222
//   height = DISPLAY_H + PAD + ROWS*(BTN_H+PAD)+ PAD
//          = 40 + 6 + 5*46 + 6                 = 282
#define CALC_CLIENT_W    (20 * (SCREEN_WIDTH / 100))
#define CALC_CLIENT_H    (35 * (SCREEN_HEIGHT / 100))

// Window size (client + chrome)
#define CALC_WIN_W  (CALC_CLIENT_W + 2 * WINDOW_BORDER_WIDTH)
#define CALC_WIN_H  (CALC_CLIENT_H + TITLE_BAR_HEIGHT + 2 * WINDOW_BORDER_WIDTH)

// ─── Button descriptors ──────────────────────────────────────────────────────
struct CalcButton {
    const char* label;   // shown on face
    char        key;     // character passed to calc_input()
    int         col;     // 0-based grid column
    int         row;     // 0-based grid row
    int         col_span;// normally 1
};

// ─── Calculator state ────────────────────────────────────────────────────────
struct XPCalculator {
    XPWindow* window;

    char  display[32];      // string shown in the display box
    int   accumulator;      // stored left-hand value
    char  pending_op;       // operator waiting to be applied ('\0' = none)
    bool  fresh_operand;    // true → next digit starts a new number
    bool  error;            // division / modulo by zero
};

// ─── Public API ──────────────────────────────────────────────────────────────

/** Allocate and initialise a calculator bound to @win. */
XPCalculator* create_calculator(XPWindow* window);

/** Free resources. */
void destroy_calculator(XPCalculator* calc);

/**
 * Feed one character of input.
 * Accepted characters:
 *   '0'–'9'          digit
 *   '+'  '-'  '*'  '/'  '%'   operator
 *   '='  '\n' '\r'   evaluate
 *   'c'  'C'          clear
 */
void calc_input(XPCalculator* calc, char c);

/** Draw the full calculator UI inside the window's client area. */
void calc_draw_frame(XPCalculator* calc);

/** Handle a mouse click at (mx, my). */
void calc_handle_mouse(XPCalculator* calc, int mx, int my);

// ─── Window callbacks (for XPWindowCallbacks) ────────────────────────────────
void calc_draw_frame_wrapper(void* ctx);
void calc_on_move(void* ctx, int x, int y);
void calc_set_active(void* ctx);

// ─── Desktop icon handler ────────────────────────────────────────────────────
void on_calculator_icon_click();