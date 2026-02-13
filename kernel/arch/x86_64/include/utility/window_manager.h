#pragma once

#include <types.h>
#include <stdint.h>
#include <string.h>
#include <framebufferutil.h>
#include <graphic_composer.h>




typedef struct {
    const char* title;
    FrameBuffer RGBA_buffer;
    int layer = 0;
    bool is_valid= false;

}window;

inline int compare_windows(const window& a, const window& b) noexcept {
    if (a.layer != b.layer) {
        return a.layer - b.layer;
    }
    return *a.title - *b.title;
}

class window_manager {
private:
    static constexpr int max_number_of_windows = 64;
    int count = 0;
    window* arr[max_number_of_windows];

    int insert_window(const window& item);
    int binary_search_insert(const window* arr, int count, const window& item);
    void quicksort(int low, int high);
    void partition(window* arr, int low, int high, int& lt, int& gt);

public:
    /**
     * Constructor
     * @param width main frame buffer
     */
    window_manager(volatile struct limine_framebuffer *framebuffer);
    
    /**
     * Sort windows by layer and title
     */
    void sort_windows();
    
    /**
     * Request a new window
     * @param title Window title
     * @param layer Window layer (higher = on top)
     * @return Window ID or -1 on failure
     */
    int request_window(const char* title, int layer);

    
    /**
     * Destructor - cleans up allocated memory
     */
    ~window_manager();
    
    
};