#include <calculator.h>
#include <GUI.h>
#include <window.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <liballoc.h>
#include <string.h>
#include <stdio.h>
#include <shell.h>

// ─── Button grid definition ──────────────────────────────────────────────────
//
//  Row 0:  [ 7 ]  [ 8 ]  [ 9 ]  [ / ]
//  Row 1:  [ 4 ]  [ 5 ]  [ 6 ]  [ * ]
//  Row 2:  [ 1 ]  [ 2 ]  [ 3 ]  [ - ]
//  Row 3:  [ C ]  [ 0 ]  [ = ]  [ + ]
//  Row 4:  [        %        ]  (spans 2 cols, centred)
//
// 'key' is the character forwarded to calc_input().
// '\n' = evaluate (same as '=').

static const CalcButton k_buttons[] = {
    // label   key   col  row  span
    { "7",     '7',   0,   0,   1 },
    { "8",     '8',   1,   0,   1 },
    { "9",     '9',   2,   0,   1 },
    { "/",     '/',   3,   0,   1 },

    { "4",     '4',   0,   1,   1 },
    { "5",     '5',   1,   1,   1 },
    { "6",     '6',   2,   1,   1 },
    { "*",     '*',   3,   1,   1 },

    { "1",     '1',   0,   2,   1 },
    { "2",     '2',   1,   2,   1 },
    { "3",     '3',   2,   2,   1 },
    { "-",     '-',   3,   2,   1 },

    { "C",     'C',   0,   3,   1 },
    { "0",     '0',   1,   3,   1 },
    { "=",     '=',   2,   3,   1 },
    { "+",     '+',   3,   3,   1 },

    { "%",     '%',   1,   4,   2 },   // centred over cols 1-2
};
static const int k_num_buttons = (int)(sizeof(k_buttons) / sizeof(k_buttons[0]));

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Minimal itoa for signed ints using u64toa from GUI.cpp
static void itoa_signed(int value, char* buf, int base)
{
    if (value < 0) {
        buf[0] = '-';
        u64toa((uint64_t)(-(int64_t)value), buf + 1, base);
    } else {
        u64toa((uint64_t)value, buf, base);
    }
}

// strlen for bare-metal
static int calc_strlen(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

// Client-area origin (top-left of the area below the title bar + border)
static inline int client_x(XPCalculator* c)
{
    return c->window->x + WINDOW_BORDER_WIDTH;
}
static inline int client_y(XPCalculator* c)
{
    return c->window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
}

// Compute on-screen rect for button index @idx
static void btn_rect(XPCalculator* c, int idx, int* bx, int* by, int* bw, int* bh)
{
    const CalcButton& b = k_buttons[idx];
    int step = CALC_BTN_W + CALC_BTN_PAD;

    *bx = client_x(c) + CALC_BTN_PAD + b.col * step;
    *by = client_y(c) + CALC_DISPLAY_H + CALC_BTN_PAD * 2 + b.row * (CALC_BTN_H + CALC_BTN_PAD);
    *bw = b.col_span * step - CALC_BTN_PAD;
    *bh = CALC_BTN_H;
}

// ─── Logic ───────────────────────────────────────────────────────────────────

static void calc_reset(XPCalculator* calc)
{
    calc->display[0]  = '0';
    calc->display[1]  = '\0';
    calc->accumulator = 0;
    calc->pending_op  = '\0';
    calc->fresh_operand = true;
    calc->error       = false;
}

static int apply_op(char op, int a, int b, bool* err)
{
    *err = false;
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0) { *err = true; return 0; }
            return a / b;
        case '%':
            if (b == 0) { *err = true; return 0; }
            return a % b;
        default:  return b;
    }
}

void calc_input(XPCalculator* calc, char c)
{
    if (calc->error && c != 'C' && c != 'c') return;

    // ── Digit ──
    if (c >= '0' && c <= '9') {
        if (calc->fresh_operand) {
            calc->display[0] = c;
            calc->display[1] = '\0';
            calc->fresh_operand = false;
        } else {
            int len = calc_strlen(calc->display);
            if (len < 10) {                     // cap display width
                // Don't allow leading zeros
                if (!(len == 1 && calc->display[0] == '0')) {
                    calc->display[len]     = c;
                    calc->display[len + 1] = '\0';
                } else {
                    calc->display[0] = c;
                    calc->display[1] = '\0';
                }
            }
        }
        return;
    }

    // ── Clear ──
    if (c == 'C' || c == 'c') {
        calc_reset(calc);
        return;
    }

    // ── Evaluate (= or Enter) ──
    if (c == '=' || c == '\n' || c == '\r') {
        if (calc->pending_op != '\0') {
            int b = atoi(calc->display);
            bool err = false;
            int result = apply_op(calc->pending_op, calc->accumulator, b, &err);
            if (err) {
                strncpy(calc->display, "ERR", sizeof(calc->display));
                calc->error = true;
            } else {
                itoa_signed(result, calc->display, 10);
                calc->accumulator = result;
            }
            calc->pending_op    = '\0';
            calc->fresh_operand = true;
        }
        return;
    }

    // ── Operator ──
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
        // If there's already a pending op, evaluate it first
        if (calc->pending_op != '\0' && !calc->fresh_operand) {
            int b = atoi(calc->display);
            bool err = false;
            int result = apply_op(calc->pending_op, calc->accumulator, b, &err);
            if (err) {
                strncpy(calc->display, "ERR", sizeof(calc->display));
                calc->error = true;
                calc->pending_op = '\0';
                return;
            }
            calc->accumulator = result;
            itoa_signed(result, calc->display, 10);
        } else {
            // First operator press — store current display as accumulator
            calc->accumulator = atoi(calc->display);
        }
        calc->pending_op    = c;
        calc->fresh_operand = true;
        return;
    }
}

// ─── Rendering ───────────────────────────────────────────────────────────────

void calc_draw_frame(XPCalculator* calc)
{
    if (!calc || !calc->window) return;

    int cx = client_x(calc);
    int cy = client_y(calc);

    // ── Display strip ──
    fill_rectangle(cx, cy, CALC_CLIENT_W, CALC_DISPLAY_H, 0xFFFFFF);
    draw_beveled_border_thick(cx, cy, CALC_CLIENT_W, CALC_DISPLAY_H,
                              0x808080, 0xF0F0F0, 0xFFFFFF, false);  // inset

    // Right-align the display text
    int text_w = calc_strlen(calc->display) * 8;
    int text_x = cx + CALC_CLIENT_W - text_w - 8;
    int text_y = cy + (CALC_DISPLAY_H - 12) / 2;
    draw_text(calc->display, text_x, text_y,
              calc->error ? 0xCC0000 : 0x000000,
              0xFFFFFF);

    // Show pending operator hint in top-left of display
    if (calc->pending_op != '\0') {
        char hint[3] = { calc->pending_op, '\0', '\0' };
        draw_text(hint, cx + 4, cy + 4, 0x808080, 0xFFFFFF);
    }

    // ── Buttons ──
    for (int i = 0; i < k_num_buttons; i++) {
        int bx, by, bw, bh;
        btn_rect(calc, i, &bx, &by, &bw, &bh);

        const CalcButton& b = k_buttons[i];

        // Colour operators differently from digits
        bool is_op    = (b.key == '+' || b.key == '-' ||
                         b.key == '*' || b.key == '/' ||
                         b.key == '%' || b.key == '=');
        bool is_clear = (b.key == 'C');

        uint32_t face = is_clear ? 0xE08080
                      : is_op   ? 0xD4D0C8
                      :           XP_BUTTON_FACE;

        fill_rectangle(bx, by, bw, bh, face);
        draw_beveled_border_thick(bx, by, bw, bh,
                                  XP_BUTTON_HIGHLIGHT, face, XP_BUTTON_SHADOW, true);

        // Centre label
        draw_text_centered(b.label, bx, by + (bh - 12) / 2, bw,
                           XP_WINDOW_TEXT, face);
    }
}

// ─── Mouse handling ───────────────────────────────────────────────────────────

void calc_handle_mouse(XPCalculator* calc, int mx, int my)
{
    for (int i = 0; i < k_num_buttons; i++) {
        int bx, by, bw, bh;
        btn_rect(calc, i, &bx, &by, &bw, &bh);

        if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
            calc_input(calc, k_buttons[i].key);
            return;
        }
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

XPCalculator* create_calculator(XPWindow* window)
{
    XPCalculator* calc = (XPCalculator*)malloc(sizeof(XPCalculator));
    if (!calc) return nullptr;

    calc->window = window;
    calc_reset(calc);
    return calc;
}

void destroy_calculator(XPCalculator* calc)
{
    if (calc) free(calc);
}

// ─── Window callbacks ─────────────────────────────────────────────────────────

void calc_draw_frame_wrapper(void* ctx)
{
    calc_draw_frame(static_cast<XPCalculator*>(ctx));
}

void calc_on_move(void* /*ctx*/, int /*x*/, int /*y*/) {}

void calc_set_active(void* /*ctx*/) {}

// ─── Desktop icon handler ─────────────────────────────────────────────────────

void on_calculator_icon_click()
{
    XPWindowCallbacks* cb = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    cb->draw_frame = calc_draw_frame_wrapper;
    cb->on_move    = calc_on_move;
    cb->set_active = calc_set_active;
    cb->context    = nullptr;

    XPWindow* win = create_xp_window(
        200, 150,                   // x, y — centred-ish on screen
        CALC_WIN_W, CALC_WIN_H,
        "Calculator",
        cb
    );
    win->window_type = WINDOW_TYPE_CALC;

    XPCalculator* calc = create_calculator(win);
    win->context       = (void*)calc;

    set_active_xp_window(win);
}