#include <cstdint>
#include <timer.h>


#include <fram_loop.h>


extern uint64_t get_frame_time();

void frame_loop(void (*render_callback)(void)) {
    static uint64_t last_frame_time = 0;
    uint64_t current_frame_time = get_frame_time();
    
    // Only render if frame time has changed
    if (current_frame_time != last_frame_time) {
        last_frame_time = current_frame_time;
        render_callback();
    }
}

void frame_loop_with_delta(void (*render_callback)(uint64_t delta_time)) {
    static uint64_t last_frame_time = 0;
    uint64_t current_frame_time = get_frame_time();
    
    if (current_frame_time != last_frame_time) {
        uint64_t delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        render_callback(delta_time);
    }
}


void frame_loop_with_stats(void (*render_callback)(void), FrameStats* stats) {
    static uint64_t last_frame_time = 0;
    uint64_t current_frame_time = get_frame_time();
    
    if (current_frame_time != last_frame_time) {
        last_frame_time = current_frame_time;
        
        stats->total_frames++;
        stats->frames_this_second++;
        
        // Update FPS counter every second
        if (current_frame_time - stats->last_second_frame_time >= 1000) {  // Assuming ms
            stats->current_fps = stats->frames_this_second;
            stats->frames_this_second = 0;
            stats->last_second_frame_time = current_frame_time;
        }
        
        render_callback();
    }
}


class FrameTimer {
private:
    uint64_t last_frame_time;
    uint64_t (*timer_function)();
    
public:
    FrameTimer(uint64_t (*get_time)()) : last_frame_time(0), timer_function(get_time) {}
    
    // Returns true if a new frame should be drawn
    bool should_render() {
        uint64_t current_time = timer_function();
        if (current_time != last_frame_time) {
            last_frame_time = current_time;
            return true;
        }
        return false;
    }
    
    // Get the current frame count
    uint64_t get_current_time() {
        return timer_function();
    }
};
