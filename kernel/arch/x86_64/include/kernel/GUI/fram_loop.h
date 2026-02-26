#ifndef FRAME_LOOP_H
#define FRAME_LOOP_H

#include <cstdint>
#include <timer.h>


extern uint64_t get_frame_time();

// only renders when timer changes
void frame_loop(void (*render_callback)(void));

// Frame loop with delta time
void frame_loop_with_delta(void (*render_callback)(uint64_t delta_time));

struct FrameStats {
    uint64_t total_frames;
    uint64_t frames_this_second;
    uint64_t last_second_frame_time;
    uint32_t current_fps;
};

void frame_loop_with_stats(void (*render_callback)(void), FrameStats* stats);


#endif 