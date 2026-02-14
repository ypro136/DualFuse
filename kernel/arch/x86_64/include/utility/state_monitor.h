#pragma once

#include <types.h>
#include <console.h>

#ifndef STATE_MONITOR_H
#define STATE_MONITOR_H

class StateMonitor {
public:
    // percent values 0..100 for three bars
    volatile uint8_t bar[3];

    // colors (32-bit RGB)
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t fill_color[3];

    // Create a monitor window of given size at (x,y)
    StateMonitor(uint32_t width = 300, uint32_t height = 120, uint32_t x = 0, uint32_t y = 0);

    // Initialize internal console and draw initial frame
    void initialize();
    // Clear the monitor window and redraw frame (does not modify progress values)
    void clear_screen();

    // Set progress for one of the three bars (idx 0..2)
    void set_progress(int idx, uint8_t percent);

    // Render current state to the window
    void render();

    // Resize / reposition
    void set_window_size(uint32_t width, uint32_t height);
    void set_window_position(uint32_t x, uint32_t y);

private:
    Console wnd; // console window used for rendering
    uint32_t window_w;
    uint32_t window_h;
    uint32_t window_x;
    uint32_t window_y;

    void draw_bar(int idx, uint8_t pct);
};

#endif

// Global instance (defined in state_monitor.cpp)
extern StateMonitor stateMonitor;
