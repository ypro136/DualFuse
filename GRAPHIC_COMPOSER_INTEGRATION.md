/**
 * GraphicComposer Integration Guide
 * 
 * This document shows how to integrate GraphicComposer with the existing
 * DualFuse kernel components (Console, StateMonitor, framebuffer utilities)
 */

#include <graphic_composer.h>
#include <console.h>
#include <framebufferutil.h>
#include <stdio.h>

// ============================================================================
// Integration Pattern 1: Replacing Direct Framebuffer Drawing
// ============================================================================

/**
 * Old approach (direct framebuffer writes):
 * 
 * void draw_interface() {
 *     draw_pixel(100, 100, 0xFF0000);  // Direct writes
 *     draw_pixel(101, 100, 0x00FF00);
 * }
 */

/**
 * New approach (using GraphicComposer):
 */
void draw_interface_with_composer() {
    // Create composer matching screen dimensions
    GraphicComposer composer(screen_width, screen_height);
    
    // Clear background using system color
    composer.clear_buffer(0x1B, 0x26, 0x2C, 0xFF);  // DualFuse dark blue
    
    // Draw interface elements
    composer.fill_rect(100, 100, 200, 150, 0x18, 0x24, 0x32, 0xFF);
    
    // Convert to system format
    composer.convert_to_system_format(0x1B262C);
    
    // Copy to actual framebuffer
    composer.copy_to_framebuffer(framebuffer->address, buffer_size);
}

// ============================================================================
// Integration Pattern 2: Layered GUI with Multiple Panels
// ============================================================================

/**
 * Create a complete GUI layout with multiple composable sections
 */
class GUILayout {
private:
    GraphicComposer* composer;
    FrameBuffer* main_panel;
    FrameBuffer* header_panel;
    FrameBuffer* status_panel;
    FrameBuffer* content_panel;
    
public:
    GUILayout(uint32_t width, uint32_t height) {
        // Initialize composer
        composer = new GraphicComposer(width, height);
        
        // Create panels
        main_panel = composer->create_framebuffer(width, height);
        header_panel = composer->create_framebuffer(width, 60);
        status_panel = composer->create_framebuffer(width, 40);
        content_panel = composer->create_framebuffer(width - 40, height - 120);
        
        // Initialize panel colors
        composer->fill_framebuffer(main_panel, 40, 40, 40, 255);
        composer->fill_framebuffer(header_panel, 100, 100, 100, 255);
        composer->fill_framebuffer(status_panel, 70, 70, 70, 255);
        composer->fill_framebuffer(content_panel, 60, 60, 60, 255);
        
        // Position panels
        main_panel->x_offset = 0;
        main_panel->y_offset = 0;
        
        header_panel->x_offset = 0;
        header_panel->y_offset = 0;
        
        content_panel->x_offset = 20;
        content_panel->y_offset = 70;
        
        status_panel->x_offset = 0;
        status_panel->y_offset = height - 40;
    }
    
    ~GUILayout() {
        if (composer) {
            composer->destroy_framebuffer(main_panel);
            composer->destroy_framebuffer(header_panel);
            composer->destroy_framebuffer(status_panel);
            composer->destroy_framebuffer(content_panel);
            delete composer;
        }
    }
    
    void render() {
        // Composite all panels
        FrameBuffer* layers[] = {main_panel, header_panel, content_panel, status_panel};
        composer->composite_multiple(layers, 4);
        
        // Convert and display
        composer->convert_to_system_format(0x000000);
        composer->copy_to_framebuffer(framebuffer->address, buffer_size);
    }
};

// ============================================================================
// Integration Pattern 3: Window Manager for Multiple Overlays
// ============================================================================

/**
 * Manager for multiple independent windows (like console and state monitor)
 */
class WindowManager {
private:
    static const uint32_t MAX_WINDOWS = 10;
    
    struct Window {
        FrameBuffer* buffer;
        bool visible;
        uint32_t z_order;  // 0 = bottom, higher = top
    };
    
    GraphicComposer* composer;
    Window windows[MAX_WINDOWS];
    uint32_t window_count;
    
public:
    WindowManager(uint32_t screen_w, uint32_t screen_h) 
        : composer(nullptr), window_count(0) {
        composer = new GraphicComposer(screen_w, screen_h);
    }
    
    ~WindowManager() {
        for (uint32_t i = 0; i < window_count; i++) {
            if (windows[i].buffer) {
                composer->destroy_framebuffer(windows[i].buffer);
            }
        }
        delete composer;
    }
    
    /**
     * Create a new window
     */
    uint32_t create_window(uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
        if (window_count >= MAX_WINDOWS) {
            return UINT32_MAX;  // Error: too many windows
        }
        
        uint32_t id = window_count;
        
        windows[id].buffer = composer->create_framebuffer(width, height);
        windows[id].buffer->x_offset = x;
        windows[id].buffer->y_offset = y;
        windows[id].visible = true;
        windows[id].z_order = window_count;
        
        window_count++;
        return id;
    }
    
    /**
     * Fill window with color
     */
    void fill_window(uint32_t id, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (id < window_count && windows[id].buffer) {
            composer->fill_framebuffer(windows[id].buffer, r, g, b, a);
        }
    }
    
    /**
     * Toggle window visibility
     */
    void set_window_visible(uint32_t id, bool visible) {
        if (id < window_count) {
            windows[id].visible = visible;
        }
    }
    
    /**
     * Move window to front
     */
    void bring_to_front(uint32_t id) {
        if (id < window_count) {
            windows[id].z_order = window_count + id;  // Larger z_order = front
        }
    }
    
    /**
     * Render all visible windows
     */
    void render() {
        // Clear background
        composer->clear_buffer(30, 30, 30, 255);
        
        // Collect visible windows and sort by z_order
        FrameBuffer* visible_windows[MAX_WINDOWS];
        uint32_t visible_count = 0;
        
        for (uint32_t i = 0; i < window_count; i++) {
            if (windows[i].visible && windows[i].buffer) {
                visible_windows[visible_count++] = windows[i].buffer;
            }
        }
        
        // Composite in order
        composer->composite_multiple(visible_windows, visible_count);
        
        // Convert and display
        composer->convert_to_system_format(0x000000);
        composer->copy_to_framebuffer(framebuffer->address, buffer_size);
    }
};

// ============================================================================
// Integration Pattern 4: Console Rendering with Composer
// ============================================================================

/**
 * Adapter to render Console output using GraphicComposer
 * (Complementary to Console class, not replacement)
 */
class ConsoleComposerAdapter {
private:
    GraphicComposer* composer;
    FrameBuffer* console_buffer;
    uint32_t width, height;
    
public:
    ConsoleComposerAdapter(uint32_t w, uint32_t h) 
        : width(w), height(h) {
        composer = new GraphicComposer(w, h);
        console_buffer = composer->create_framebuffer(w, h);
        
        // Dark background like DualFuse theme
        composer->fill_framebuffer(console_buffer, 27, 38, 44, 255);
        console_buffer->x_offset = 0;
        console_buffer->y_offset = 0;
    }
    
    ~ConsoleComposerAdapter() {
        if (console_buffer) {
            composer->destroy_framebuffer(console_buffer);
        }
        delete composer;
    }
    
    /**
     * Draw a simple text frame (placeholder for actual text rendering)
     */
    void draw_frame() {
        composer->clear_buffer(30, 30, 30, 255);
        
        // Draw console window frame
        composer->draw_rect_outline(10, 10, width - 20, height - 20, 
                                   187, 225, 250, 255, 2);
        
        // Composite console buffer
        composer->composite_framebuffer(console_buffer);
        
        // Convert and display
        composer->convert_to_system_format(0x000000);
        composer->copy_to_framebuffer(framebuffer->address, buffer_size);
    }
    
    /**
     * Get direct access to console buffer for drawing text
     */
    FrameBuffer* get_buffer() {
        return console_buffer;
    }
};

// ============================================================================
// Integration Pattern 5: System Initialization
// ============================================================================

/**
 * Example of how to initialize GraphicComposer in kernel_main()
 * (Similar to console and state monitor initialization)
 */

extern "C" void graphic_composer_kernel_init() {
    printf("[GraphicComposer] Initializing\n");
    
    // Create global composer instance
    static GraphicComposer* gui_composer = nullptr;
    gui_composer = new GraphicComposer(screen_width, screen_height);
    
    if (!gui_composer) {
        printf("[GraphicComposer] ERROR: Failed to allocate memory\n");
        return;
    }
    
    // Initialize with dark background
    gui_composer->clear_buffer(25, 38, 44, 255);  // #1B262C
    
    printf("[GraphicComposer] Initialized successfully\n");
    printf("[GraphicComposer] Buffer: %dx%d (%lld bytes RGBA + %d bytes RGB)\n",
           screen_width, screen_height,
           gui_composer->get_size() * sizeof(RGBAPixel),
           gui_composer->get_size() * sizeof(uint32_t));
}

// ============================================================================
// Integration Pattern 6: Rendering Loop
// ============================================================================

/**
 * Example per-frame rendering loop
 */
void render_frame(GraphicComposer& composer, 
                 Console& console, 
                 StateMonitor& state_monitor) {
    // Clear to background
    composer.clear_buffer(25, 38, 44, 255);
    
    // Create framebuffers for each component
    FrameBuffer* console_fb = composer.create_framebuffer(800, 600);
    FrameBuffer* status_fb = composer.create_framebuffer(800, 170);
    
    // Fill with content (would be drawn by respective components)
    composer.fill_framebuffer(console_fb, 27, 38, 44, 255);
    composer.fill_framebuffer(status_fb, 50, 50, 50, 255);
    
    // Position on screen
    console_fb->x_offset = 10;
    console_fb->y_offset = 10;
    status_fb->x_offset = 10;
    status_fb->y_offset = 620;
    
    // Composite
    FrameBuffer* layers[] = {console_fb, status_fb};
    composer.composite_multiple(layers, 2);
    
    // Convert and display
    composer.convert_to_system_format(0x000000);
    composer.copy_to_framebuffer(framebuffer->address, buffer_size);
    
    // Cleanup
    composer.destroy_framebuffer(console_fb);
    composer.destroy_framebuffer(status_fb);
}

// ============================================================================
// Integration Pattern 7: Double Buffering (Advanced)
// ============================================================================

/**
 * Double-buffering implementation for flicker-free rendering
 */
class DoubleBufferedGUI {
private:
    GraphicComposer* front_buffer;
    GraphicComposer* back_buffer;
    uint32_t screen_width, screen_height;
    
public:
    DoubleBufferedGUI(uint32_t width, uint32_t height)
        : screen_width(width), screen_height(height) {
        front_buffer = new GraphicComposer(width, height);
        back_buffer = new GraphicComposer(width, height);
    }
    
    ~DoubleBufferedGUI() {
        delete front_buffer;
        delete back_buffer;
    }
    
    /**
     * Get back buffer for drawing
     */
    GraphicComposer* get_back_buffer() {
        return back_buffer;
    }
    
    /**
     * Swap buffers and display
     */
    void swap_and_display() {
        // Convert back buffer to system format
        back_buffer->convert_to_system_format(0x000000);
        
        // Copy to framebuffer
        back_buffer->copy_to_framebuffer(framebuffer->address, buffer_size);
        
        // Swap for next frame
        GraphicComposer* temp = front_buffer;
        front_buffer = back_buffer;
        back_buffer = temp;
        
        // Clear back buffer for next frame
        back_buffer->clear_transparent();
    }
};

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * INTEGRATION CHECKLIST:
 * 
 * 1. Memory Allocation:
 *    - GraphicComposer uses malloc/free
 *    - Ensure memory manager is initialized
 *    - Monitor heap fragmentation with large buffers
 * 
 * 2. Framebuffer Access:
 *    - Use existing framebuffer_initialize() for access to tempframebuffer
 *    - screen_width, screen_height, pitch globals available
 *    - pitch is in bytes per line
 * 
 * 3. Color Formats:
 *    - Existing system uses: uint32_t RGB (0xRRGGBB)
 *    - GraphicComposer uses: uint64_t RGBA internally
 *    - Conversion handled automatically by convert_to_system_format()
 * 
 * 4. Performance:
 *    - Compositing adds overhead vs. direct drawing
 *    - Benefit: Multiple layers, transparency, better abstraction
 *    - Optimization: Use appropriate buffer sizes for GUI elements
 * 
 * 5. Concurrency:
 *    - GraphicComposer is not thread-safe
 *    - Protect with spinlocks if multiple CPUs write simultaneously
 *    - Use in interrupt handlers with caution
 * 
 * 6. Include Paths:
 *    - Add to kernel Makefile:
 *      SOURCES += arch/x86_64/utility/graphic_composer.cpp
 *      INCLUDES += -I arch/x86_64/include/utility
 * 
 * 7. Cleanup:
 *    - Always destroy framebuffers with destroy_framebuffer()
 *    - Delete composer instances when no longer needed
 *    - Check for memory leaks in long-running kernels
 */
