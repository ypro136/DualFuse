#pragma once

#include <types.h>
#include <stdint.h>
#include <string.h>

/**
 * GraphicComposer - GUI Composition and Frame Buffer Management
 * 
 * This class manages RGBA data encoded in 64-bit (long long) integers,
 * allowing for efficient storage and manipulation of graphical data.
 * Supports composing multiple layers and converting to system framebuffer format.
 * 
 * RGBA Encoding in uint64_t (little-endian):
 *   - Bits 0-7:   Red (R)
 *   - Bits 8-15:  Green (G)
 *   - Bits 16-23: Blue (B)
 *   - Bits 24-31: Alpha (A)
 *   - Bits 32-63: Reserved for future use
 */

typedef uint64_t RGBAPixel;  // 64-bit encoded RGBA pixel




/**
 * FrameBuffer structure for composable GUI elements
 * Stores a 2D array of RGBAPixel data with position and dimensions
 */
typedef struct {
    uint32_t width;           // Width in pixels
    uint32_t height;          // Height in pixels
    uint32_t x_offset;        // X position on screen
    uint32_t y_offset;        // Y position on screen
    RGBAPixel* data;          // Pointer to RGBA pixel array (width * height)
} FrameBuffer;

class GraphicComposer {
private:
    // Master composition buffer - stores composed RGBA data
    RGBAPixel* composition_buffer;
    
    // Note: System output is written directly to the kernel's temporary
    // framebuffer (`tempframebuffer->address`). No private system buffer
    // is allocated by GraphicComposer to avoid duplicating large frame data.
    
    // Dimensions
    uint32_t buffer_width;
    uint32_t buffer_height;
    uint32_t buffer_size;
    
    // Helper methods for RGBA encoding/decoding
    static inline RGBAPixel rgba_encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static inline void rgba_decode(RGBAPixel pixel, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);
    
    // Blending with alpha compositing
    static inline uint32_t blend_pixels(RGBAPixel fg, RGBAPixel bg);
    
    // Bounds checking
    bool is_within_bounds(uint32_t x, uint32_t y) const;

public:
    /**
     * Constructor
     * @param width Width of the composition buffer
     * @param height Height of the composition buffer
     */
    GraphicComposer(uint32_t width, uint32_t height);
    
    /**
     * Destructor - cleans up allocated memory
     */
    ~GraphicComposer();
    
    // ========================================================================
    // RGBA Pixel Manipulation
    // ========================================================================
    
    /**
     * Set a single pixel at (x, y) with RGBA values
     */
    void set_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * Get a single pixel at (x, y)
     */
    RGBAPixel get_pixel(uint32_t x, uint32_t y) const;
    
    /**
     * Set pixel directly with encoded RGBAPixel
     */
    void set_pixel_encoded(uint32_t x, uint32_t y, RGBAPixel pixel);
    
    // ========================================================================
    // Buffer Operations
    // ========================================================================
    
    /**
     * Clear the entire composition buffer to specified RGBA color
     */
    void clear_buffer(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * Clear to transparent (0, 0, 0, 0)
     */
    void clear_transparent();
    
    /**
     * Fill a rectangular region with RGBA color
     */
    void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * Draw a line with RGBA color (Bresenham algorithm)
     */
    void draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * Draw a rectangle outline with RGBA color
     */
    void draw_rect_outline(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t thickness);
    
    /**
     * Draw a filled circle with RGBA color
     */
    void draw_circle(uint32_t cx, uint32_t cy, uint32_t radius,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    // ========================================================================
    // Frame Buffer Composition
    // ========================================================================
    
    /**
     * Create a new FrameBuffer with specified dimensions
     * Caller must free the returned pointer
     */
    FrameBuffer* create_framebuffer(uint32_t width, uint32_t height);
    
    /**
     * Destroy a FrameBuffer and free its memory
     */
    void destroy_framebuffer(FrameBuffer* fb);
    
    /**
     * Composite a FrameBuffer onto the composition buffer at its specified offset
     * Uses alpha blending for transparency
     */
    void composite_framebuffer(const FrameBuffer* fb);
    
    /**
     * Composite multiple FrameBuffers in sequence
     */
    void composite_multiple(const FrameBuffer** framebuffers, uint32_t count);
    
    /**
     * Fill a FrameBuffer with RGBA color
     */
    void fill_framebuffer(FrameBuffer* fb, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * Copy one FrameBuffer's data to another at specified offset
     */
    void copy_framebuffer(const FrameBuffer* src, FrameBuffer* dst,
                         uint32_t dst_x, uint32_t dst_y);
    
    // ========================================================================
    // System Framebuffer Conversion
    // ========================================================================
    
    /**
     * Convert the composed RGBA buffer to system RGB format
     * Applies alpha blending with background if specified
     * @param bg_color System format color to use as background (0xRRGGBB)
     */
    void convert_to_system_format(uint32_t bg_color = 0x000000);
    
    /**
     * Get the system format buffer (RGB 32-bit)
     * Must call convert_to_system_format() first
     */
    const uint32_t* get_system_buffer() const;
    
    /**
     * Copy system buffer to actual framebuffer
     * @param framebuffer_ptr Pointer to system framebuffer address
     * @param framebuffer_size Size of framebuffer in bytes
     */
    void copy_to_framebuffer(void* framebuffer_ptr, uint64_t framebuffer_size);
    
    // ========================================================================
    // Buffer Access and Information
    // ========================================================================
    
    /**
     * Get the composition buffer pointer
     */
    RGBAPixel* get_composition_buffer() const;
    
    /**
     * Get buffer dimensions
     */
    uint32_t get_width() const { return buffer_width; }
    uint32_t get_height() const { return buffer_height; }
    uint32_t get_size() const { return buffer_size; }
    
    /**
     * Get stride in bytes for composition buffer
     */
    uint64_t get_rgba_stride() const { return buffer_width * sizeof(RGBAPixel); }
    
    /**
     * Get stride in bytes for system buffer
     */
    uint64_t get_system_stride() const { return buffer_width * sizeof(uint32_t); }
};

// ============================================================================
// Inline Helper Functions for RGBA Encoding
// ============================================================================

/**
 * Encode RGBA values into a 64-bit pixel
 */
inline RGBAPixel rgba_make(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint64_t)a << 24) | ((uint64_t)b << 16) | ((uint64_t)g << 8) | (uint64_t)r;
}

/**
 * Extract red channel
 */
inline uint8_t rgba_get_r(RGBAPixel pixel) {
    return (uint8_t)(pixel & 0xFF);
}

/**
 * Extract green channel
 */
inline uint8_t rgba_get_g(RGBAPixel pixel) {
    return (uint8_t)((pixel >> 8) & 0xFF);
}

/**
 * Extract blue channel
 */
inline uint8_t rgba_get_b(RGBAPixel pixel) {
    return (uint8_t)((pixel >> 16) & 0xFF);
}

/**
 * Extract alpha channel
 */
inline uint8_t rgba_get_a(RGBAPixel pixel) {
    return (uint8_t)((pixel >> 24) & 0xFF);
}

/**
 * Convert RGBA to system RGB format (0xRRGGBB as uint32_t)
 */
inline uint32_t rgba_to_rgb(RGBAPixel rgba) {
    uint8_t r = rgba_get_r(rgba);
    uint8_t g = rgba_get_g(rgba);
    uint8_t b = rgba_get_b(rgba);
    return (uint32_t)((r << 16) | (g << 8) | b);
}

/**
 * Convert system RGB format to RGBA (with full opacity)
 */
inline RGBAPixel rgb_to_rgba(uint32_t rgb) {
    uint8_t r = (uint8_t)((rgb >> 16) & 0xFF);
    uint8_t g = (uint8_t)((rgb >> 8) & 0xFF);
    uint8_t b = (uint8_t)(rgb & 0xFF);
    return rgba_make(r, g, b, 0xFF);
}

// extern GraphicComposer* GC;
// extern bool graphic_composer_initialized;  // TODO: Will be added later


