#include <window_manager.h>

#include <graphic_composer.h>
#include <framebufferutil.h>

#include <liballoc.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/*
window_manager uses one graphic_composer to manage all system windows (position, size and leyer)
*/

GraphicComposer* graphic_composer;
// bool graphic_composer_initialized = false;  // TODO: Will be added later

window_manager::window_manager(volatile struct limine_framebuffer *framebuffer)
{
    // TODO: Initialize graphic_composer if needed
    // graphic_composer = new GraphicComposer(framebuffer->width,framebuffer->height);
    //
    // if (graphic_composer) 
    // {
    //     graphic_composer_initialized = true;
    // }
}

window_manager::~window_manager()
{
    // TODO: Cleanup graphic_composer if needed
    // if (graphic_composer) 
    // {
    //     free(graphic_composer);
    //     graphic_composer = nullptr;
    //     graphic_composer_initialized = false;
    // }
}

// Optimized quicksort with 3-way partitioning
void window_manager::partition(window* arr, int low, int high, int& lt, int& gt) {
    const window& pivot = arr[low];
    int i = low + 1;
    lt = low;
    gt = high;
    
    while (i <= gt) {
        int cmp = compare_windows(arr[i], pivot);
        if (cmp < 0) {
            window tmp = arr[lt];
            arr[lt] = arr[i];
            arr[i] = tmp;
            lt++;
            i++;
        } else if (cmp > 0) {
            window tmp = arr[i];
            arr[i] = arr[gt];
            arr[gt] = tmp;
            gt--;
        } else {
            i++;
        }
    }
}

// Recursive quicksort with insertion sort for small subarrays
void window_manager::quicksort(int low, int high) {
    constexpr int INSERTION_THRESHOLD = 10;
    
    while (low < high) {
        int size = high - low + 1;
        
        // Switch to insertion sort for small arrays
        if (size <= INSERTION_THRESHOLD) {
            for (int i = low + 1; i <= high; i++) {
                window key = *arr[i];
                int j = i - 1;
                
                while (j >= low && compare_windows((const window&)arr[j], key) > 0) {
                    arr[j + 1] = arr[j];
                    j--;
                }
                arr[j + 1] = &key;
            }
            break;
        }
        
        // 3-way partition
        int lt, gt;
        partition((window*)arr, low, high, lt, gt);
        
        // Recursively sort the smaller partition first (tail call optimization)
        if (lt - low < high - gt) {
            quicksort(low, lt - 1);
            low = gt + 1;  // Tail call: continue with right partition
        } else {
            quicksort(gt + 1, high);
            high = lt - 1;  // Tail call: continue with left partition
        }
    }
}

// Binary search to find insertion position
int window_manager::binary_search_insert(const window* arr, int count, const window& item) {
    int left = 0, right = count;
    
    while (left < right) {
        int mid = left + ((right - left) >> 1);  // Avoid overflow
        int cmp = compare_windows(arr[mid], item);
        
        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left;
}

// Insert window while maintaining sort order
int window_manager::insert_window(const window& item) {
    // Check if we have space
    if (count >= 64) {
        return -1;  // No space
    }
    
    // Find insertion position using binary search
    int pos = binary_search_insert((const window*)this->arr, count, item);
    
    // Shift elements right to make space
    for (int i = count; i > pos; i--) {
        this->arr[i] = this->arr[i - 1];
    }
    
    // Insert the new window
    window temp = item;
    this->arr[pos] = &temp;
    count++;
    
    return pos;  // Return insertion position
}

// Public API to sort windows by layer and title
void window_manager::sort_windows() {
    if (count <= 1) return;
    quicksort(0, count - 1);
}

// Request a new window
int window_manager::request_window(const char* title, int layer) {
    if (count >= 64 || !title) {
        return -1;  // No space or invalid title
    }
    
    // TODO: Initialize FrameBuffer properly when graphic_composer is available
    FrameBuffer temp_buffer = {0, 0, 0, 0, nullptr};
    
    window new_window;
    new_window.title = title;
    new_window.RGBA_buffer = temp_buffer;
    new_window.layer = layer;
    new_window.is_valid = true;
    
    return insert_window(new_window);
}