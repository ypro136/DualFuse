#include <state_monitor.h>

#include <framebufferutil.h>
#include <psf.h>
#include <stdio.h>

// Global instance
StateMonitor stateMonitor;

// Default constructor
StateMonitor::StateMonitor(uint32_t width, uint32_t height, uint32_t x, uint32_t y)
    : wnd(width, height, x, y), window_w(width), window_h(height), window_x(x), window_y(y)
{
    bar[0] = bar[1] = bar[2] = 0;
    bg_color = 0x1B262C;
    border_color = 0xBBBBBB;
    fill_color[0] = 0x00FF00; // green
    fill_color[1] = 0xFFFF00; // yellow
    fill_color[2] = 0x00AFFF; // blue-ish
}

void StateMonitor::initialize()
{
    wnd.initialize();
    // give the monitor a title that will be drawn over its border
    wnd.set_window_size(window_w, window_h);
    wnd.set_window_position(window_x, window_y);
    // draw frame
    wnd.draw_box(0, 0, window_w, window_h, bg_color);
    // draw border
    wnd.draw_box(0, 0, window_w, 2, border_color);
    wnd.draw_box(0, window_h - 2, window_w, 2, border_color);
    wnd.draw_box(0, 0, 2, window_h, border_color);
    wnd.draw_box(window_w - 2, 0, 2, window_h, border_color);
    wnd.set_title("State Monitor");
    render();
}

void StateMonitor::clear_screen()
{
#if defined(DEBUG_CONSOLE)
    printf("[state_monitor] clearing screen\n");
#endif
    // Clear the monitor window area
    wnd.clear_screen();

    // Redraw frame and border
    wnd.draw_box(0, 0, window_w, window_h, bg_color);
    wnd.draw_box(0, 0, window_w, 2, border_color);
    wnd.draw_box(0, window_h - 2, window_w, 2, border_color);
    wnd.draw_box(0, 0, 2, window_h, border_color);
    wnd.draw_box(window_w - 2, 0, 2, window_h, border_color);

    // Re-render bars in current state
    render();

#if defined(DEBUG_CONSOLE)
    printf("[state_monitor] cleared and redrew frame\n");
#endif
}

void StateMonitor::set_progress(int idx, uint8_t percent)
{
    if (idx < 0 || idx > 2) return;
    if (percent > 100) percent = 100;
    bar[idx] = percent;
    draw_bar(idx, percent);
}

void StateMonitor::render()
{
    // draw bars for all three
    for (int i = 0; i < 3; i++) {
        draw_bar(i, bar[i]);
    }
    wnd.draw_title();
}

void StateMonitor::set_window_size(uint32_t width, uint32_t height)
{
    window_w = width;
    window_h = height;
    wnd.set_window_size(width, height);
}

void StateMonitor::set_window_position(uint32_t x, uint32_t y)
{
    window_x = x;
    window_y = y;
    wnd.set_window_position(x, y);
}

void StateMonitor::draw_bar(int idx, uint8_t pct)
{
    // layout: vertical spacing with margins
    const int margin = 8;
    const int bars = 3;
    int available_h = (int)window_h - (margin * (bars + 1));
    int bar_h = available_h / bars;
    int x = margin;
    int y = margin + idx * (bar_h + margin);
    int w = (int)window_w - margin * 2;

    // background of bar
    wnd.draw_box(x, y, w, bar_h, 0x2B2B2B);
    // filled portion
    int filled_w = (w * pct) / 100;
    if (filled_w > 0) {
        wnd.draw_box(x, y, filled_w, bar_h, fill_color[idx]);
    }
    // border
    wnd.draw_box(x, y, w, 1, border_color);
    wnd.draw_box(x, y + bar_h - 1, w, 1, border_color);
    wnd.draw_box(x, y, 1, bar_h, border_color);
    wnd.draw_box(x + w - 1, y, 1, bar_h, border_color);
}
