# GraphicComposer API Reference

## Table of Contents
1. [Overview](#overview)
2. [Data Types](#data-types)
3. [Constants](#constants)
4. [Class Definition](#class-definition)
5. [Constructor/Destructor](#constructordestructor)
6. [Pixel Operations](#pixel-operations)
7. [Drawing Primitives](#drawing-primitives)
8. [FrameBuffer Operations](#framebuffer-operations)
9. [System Conversion](#system-conversion)
10. [Buffer Information](#buffer-information)
11. [Helper Functions](#helper-functions)

---

## Overview

The `GraphicComposer` class manages graphical rendering through RGBA composition. It maintains an internal 64-bit RGBA buffer and provides conversion to system RGB format for display.

**Key Properties:**
- Internal: 64-bit RGBA per pixel (full color + alpha)
- Output: 32-bit RGB per pixel (system framebuffer format)
- Alpha blending: Full support with per-pixel transparency
- Composable layers: Support for multiple FrameBuffer objects

---

## Data Types

### `RGBAPixel`
```cpp
typedef uint64_t RGBAPixel;
```
A 64-bit integer encoding RGBA color data.

**Layout (little-endian):**
```
Bits 0-7:   Red channel
Bits 8-15:  Green channel
Bits 16-23: Blue channel
Bits 24-31: Alpha channel (0=transparent, 255=opaque)
Bits 32-63: Reserved
```

### `FrameBuffer`
```cpp
typedef struct {
    uint32_t width;           // Width in pixels
    uint32_t height;          // Height in pixels
    uint32_t x_offset;        // X position on composition buffer
    uint32_t y_offset;        // Y position on composition buffer
    RGBAPixel* data;          // Pointer to RGBA pixel array
} FrameBuffer;
```

Individual composable GUI elements. Stores RGBA data that can be positioned and layered.

---

## Constants

No public constants defined. All values are passed as function parameters.

---

## Class Definition

```cpp
class GraphicComposer {
private:
    RGBAPixel* composition_buffer;    // Main RGBA working buffer
    uint32_t* system_buffer;           // System RGB output buffer
    uint32_t buffer_width;
    uint32_t buffer_height;
    uint32_t buffer_size;              // width * height
    
    // Private helper methods
    static inline RGBAPixel rgba_encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static inline void rgba_decode(RGBAPixel pixel, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);
    static inline uint32_t blend_pixels(RGBAPixel fg, RGBAPixel bg);
    bool is_within_bounds(uint32_t x, uint32_t y) const;

public:
    // ... public interface
};
```

---

## Constructor/Destructor

### Constructor
```cpp
GraphicComposer::GraphicComposer(uint32_t width, uint32_t height);
```

**Parameters:**
- `width`: Buffer width in pixels
- `height`: Buffer height in pixels

**Behavior:**
- Allocates composition buffer (RGBA, 64-bit per pixel)
- Allocates system buffer (RGB, 32-bit per pixel)
- Initializes both buffers to zero (transparent/black)
- Allocates: `(width × height × 12) bytes` total

**Example:**
```cpp
GraphicComposer composer(1024, 768);  // 1024×768 display
```

### Destructor
```cpp
GraphicComposer::~GraphicComposer();
```

**Behavior:**
- Frees composition buffer
- Frees system buffer
- Safe to call multiple times (null-checks present)

---

## Pixel Operations

### `set_pixel()`
```cpp
void set_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Set a single pixel with RGBA values.

**Parameters:**
- `x, y`: Pixel coordinates (0,0 = top-left)
- `r, g, b, a`: Color components (0-255 each)

**Bounds:** Silently ignores out-of-bounds coordinates

**Example:**
```cpp
composer.set_pixel(100, 50, 255, 0, 0, 255);  // Red pixel
composer.set_pixel(101, 50, 0, 255, 0, 200);  // Semi-transparent green
```

---

### `get_pixel()`
```cpp
RGBAPixel get_pixel(uint32_t x, uint32_t y) const;
```

**Description:** Retrieve RGBA data for a pixel.

**Returns:** 64-bit RGBA pixel value (0 if out of bounds)

**Example:**
```cpp
RGBAPixel pixel = composer.get_pixel(100, 50);
uint8_t alpha = rgba_get_a(pixel);
```

---

### `set_pixel_encoded()`
```cpp
void set_pixel_encoded(uint32_t x, uint32_t y, RGBAPixel pixel);
```

**Description:** Set pixel using pre-encoded RGBAPixel value.

**Parameters:**
- `x, y`: Pixel coordinates
- `pixel`: Pre-encoded 64-bit RGBA value

**Performance:** Slightly faster than `set_pixel()` (no encoding needed)

**Example:**
```cpp
RGBAPixel cyan = rgba_make(0, 255, 255, 255);
composer.set_pixel_encoded(100, 50, cyan);
```

---

## Drawing Primitives

### `clear_buffer()`
```cpp
void clear_buffer(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Fill entire buffer with a single RGBA color.

**Parameters:** RGBA color components (0-255)

**Performance:** O(width × height)

**Example:**
```cpp
composer.clear_buffer(30, 30, 30, 255);  // Dark gray background
```

---

### `clear_transparent()`
```cpp
void clear_transparent();
```

**Description:** Clear entire buffer to fully transparent (0,0,0,0).

**Equivalent to:**
```cpp
composer.clear_buffer(0, 0, 0, 0);
```

---

### `fill_rect()`
```cpp
void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Fill a rectangular region with RGBA color.

**Parameters:**
- `x, y`: Top-left corner
- `width, height`: Rectangle dimensions
- `r, g, b, a`: Fill color

**Clipping:** Automatically clips to buffer bounds

**Example:**
```cpp
// Draw a 200×150 cyan rectangle at (50, 50)
composer.fill_rect(50, 50, 200, 150, 0, 255, 255, 255);
```

---

### `draw_line()`
```cpp
void draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Draw a line using Bresenham's algorithm.

**Parameters:**
- `x0, y0`: Start point
- `x1, y1`: End point
- `r, g, b, a`: Line color

**Algorithm:** Bresenham (efficient, works with any angle)

**Example:**
```cpp
composer.draw_line(0, 0, 640, 480, 255, 255, 255, 255);  // White diagonal
```

---

### `draw_rect_outline()`
```cpp
void draw_rect_outline(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t thickness);
```

**Description:** Draw a hollow rectangle with specified border thickness.

**Parameters:**
- `x, y, width, height`: Rectangle bounds
- `r, g, b, a`: Border color
- `thickness`: Border width in pixels

**Example:**
```cpp
composer.draw_rect_outline(10, 10, 200, 150, 255, 255, 255, 255, 3);
```

---

### `draw_circle()`
```cpp
void draw_circle(uint32_t cx, uint32_t cy, uint32_t radius,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Draw a filled circle using midpoint circle algorithm.

**Parameters:**
- `cx, cy`: Center coordinates
- `radius`: Circle radius in pixels
- `r, g, b, a`: Fill color

**Algorithm:** Midpoint circle (8-way symmetry for efficiency)

**Example:**
```cpp
composer.draw_circle(320, 240, 50, 255, 0, 0, 255);  // Red circle at center
```

---

## FrameBuffer Operations

### `create_framebuffer()`
```cpp
FrameBuffer* create_framebuffer(uint32_t width, uint32_t height);
```

**Description:** Allocate a new independent FrameBuffer.

**Parameters:**
- `width`: Buffer width
- `height`: Buffer height

**Returns:** Pointer to allocated FrameBuffer, or nullptr on failure

**Memory:** Allocates `(width × height × 8 + sizeof(FrameBuffer))` bytes

**Example:**
```cpp
FrameBuffer* window = composer.create_framebuffer(400, 300);
if (!window) {
    printf("Memory allocation failed\n");
    return;
}
```

---

### `destroy_framebuffer()`
```cpp
void destroy_framebuffer(FrameBuffer* fb);
```

**Description:** Free a FrameBuffer and its allocated memory.

**Parameters:** FrameBuffer pointer (safe to pass nullptr)

**Example:**
```cpp
composer.destroy_framebuffer(window);
window = nullptr;  // Good practice
```

---

### `composite_framebuffer()`
```cpp
void composite_framebuffer(const FrameBuffer* fb);
```

**Description:** Layer a FrameBuffer onto the main composition buffer.

**Behavior:**
- Respects FrameBuffer's x_offset and y_offset
- Opaque pixels replace composition buffer
- Clipping applied at buffer boundaries
- Alpha channel used for blending

**Compositing Order:** Call in order from background to foreground

**Example:**
```cpp
FrameBuffer* window = composer.create_framebuffer(400, 300);
window->x_offset = 100;
window->y_offset = 100;
composer.composite_framebuffer(window);  // Position at (100, 100)
```

---

### `composite_multiple()`
```cpp
void composite_multiple(const FrameBuffer** framebuffers, uint32_t count);
```

**Description:** Composite multiple FrameBuffers in sequence.

**Parameters:**
- `framebuffers`: Array of FrameBuffer pointers
- `count`: Number of buffers to composite

**Behavior:** Composites in array order (first = bottommost)

**Example:**
```cpp
FrameBuffer* layers[3] = {background, foreground, overlay};
composer.composite_multiple(layers, 3);
```

---

### `fill_framebuffer()`
```cpp
void fill_framebuffer(FrameBuffer* fb, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Description:** Fill an entire FrameBuffer with RGBA color.

**Parameters:**
- `fb`: Target FrameBuffer
- `r, g, b, a`: Fill color

**Example:**
```cpp
FrameBuffer* panel = composer.create_framebuffer(800, 600);
composer.fill_framebuffer(panel, 100, 100, 100, 255);  // Medium gray
```

---

### `copy_framebuffer()`
```cpp
void copy_framebuffer(const FrameBuffer* src, FrameBuffer* dst,
                     uint32_t dst_x, uint32_t dst_y);
```

**Description:** Copy pixels from one FrameBuffer to another.

**Parameters:**
- `src`: Source FrameBuffer
- `dst`: Destination FrameBuffer
- `dst_x, dst_y`: Target position in destination
- Clipping applied automatically at dst bounds

**Example:**
```cpp
FrameBuffer* pattern = composer.create_framebuffer(100, 100);
composer.fill_framebuffer(pattern, 255, 0, 0, 255);

FrameBuffer* canvas = composer.create_framebuffer(400, 400);
// Copy pattern to canvas at position (50, 50)
composer.copy_framebuffer(pattern, canvas, 50, 50);
```

---

## System Conversion

### `convert_to_system_format()`
```cpp
void convert_to_system_format(uint32_t bg_color = 0x000000);
```

**Description:** Convert internal RGBA buffer to system RGB format.

**Parameters:**
- `bg_color`: Background color for transparent areas (0xRRGGBB format)

**Behavior:**
- Fully opaque pixels → direct conversion to RGB
- Fully transparent pixels → use bg_color
- Semi-transparent pixels → blend with bg_color

**Must be called before:** `get_system_buffer()`, `copy_to_framebuffer()`

**Example:**
```cpp
// Convert with black background
composer.convert_to_system_format(0x000000);

// Convert with white background
composer.convert_to_system_format(0xFFFFFF);
```

---

### `get_system_buffer()`
```cpp
const uint32_t* get_system_buffer() const;
```

**Description:** Get pointer to converted system RGB buffer.

**Prerequisites:** Must call `convert_to_system_format()` first

**Returns:** Pointer to 32-bit RGB buffer (read-only)

**Layout:** Row-major, 32-bit per pixel (0xRRGGBB)

**Example:**
```cpp
composer.convert_to_system_format(0x000000);
const uint32_t* system_buf = composer.get_system_buffer();

// Access first pixel
uint32_t first_pixel = system_buf[0];
uint8_t red = (first_pixel >> 16) & 0xFF;
```

---

### `copy_to_framebuffer()`
```cpp
void copy_to_framebuffer(void* framebuffer_ptr, uint64_t framebuffer_size);
```

**Description:** Copy converted system buffer to hardware framebuffer.

**Parameters:**
- `framebuffer_ptr`: Target framebuffer address
- `framebuffer_size`: Available space in bytes

**Behavior:**
- Uses `memcpy()` with safe size limiting
- Silently limits copy to smaller of: converted buffer, available space
- Safe with undersized destination

**Prerequisites:** Must call `convert_to_system_format()` first

**Example:**
```cpp
// Assuming 1024×768 RGB framebuffer
extern void* hw_framebuffer_addr;
extern uint64_t hw_framebuffer_size;

composer.clear_buffer(100, 100, 100, 255);
composer.convert_to_system_format(0x000000);
composer.copy_to_framebuffer(hw_framebuffer_addr, hw_framebuffer_size);
```

---

## Buffer Information

### `get_composition_buffer()`
```cpp
RGBAPixel* get_composition_buffer() const;
```

**Returns:** Direct pointer to internal RGBA buffer

**Use case:** Direct buffer manipulation (advanced usage)

**Warning:** Bypass safety checks when using directly

---

### `get_width()` / `get_height()` / `get_size()`
```cpp
uint32_t get_width() const;
uint32_t get_height() const;
uint32_t get_size() const;  // width × height
```

**Returns:** Buffer dimensions

---

### `get_rgba_stride()` / `get_system_stride()`
```cpp
uint64_t get_rgba_stride() const;  // width × 8
uint64_t get_system_stride() const; // width × 4
```

**Returns:** Bytes per row for each buffer type

**Use case:** Manual row-by-row processing

---

## Helper Functions

These are standalone inline functions in the header file.

### `rgba_make()`
```cpp
inline RGBAPixel rgba_make(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Creates:** 64-bit RGBA pixel from components

---

### Channel Extractors
```cpp
inline uint8_t rgba_get_r(RGBAPixel pixel);  // Red
inline uint8_t rgba_get_g(RGBAPixel pixel);  // Green
inline uint8_t rgba_get_b(RGBAPixel pixel);  // Blue
inline uint8_t rgba_get_a(RGBAPixel pixel);  // Alpha
```

---

### Format Converters
```cpp
inline uint32_t rgba_to_rgb(RGBAPixel rgba);   // RGBA → system RGB
inline RGBAPixel rgb_to_rgba(uint32_t rgb);    // System RGB → RGBA
```

**Conversion:** RGBA (0xAABBGGRR) → RGB (0xRRGGBB)

---

## Complete Usage Example

```cpp
#include <graphic_composer.h>

void render_gui() {
    // Create composer
    GraphicComposer composer(1024, 768);
    
    // Background
    composer.clear_buffer(50, 50, 50, 255);
    
    // Create panels
    FrameBuffer* bg_panel = composer.create_framebuffer(900, 700);
    composer.fill_framebuffer(bg_panel, 80, 80, 80, 255);
    bg_panel->x_offset = 62;
    bg_panel->y_offset = 34;
    
    FrameBuffer* header = composer.create_framebuffer(900, 50);
    composer.fill_framebuffer(header, 100, 100, 100, 255);
    header->x_offset = 62;
    header->y_offset = 34;
    
    // Composite
    composer.composite_framebuffer(bg_panel);
    composer.composite_framebuffer(header);
    
    // Draw shapes
    composer.draw_circle(512, 400, 100, 255, 0, 0, 255);
    composer.draw_line(100, 100, 900, 700, 0, 255, 0, 255);
    
    // Convert and display
    composer.convert_to_system_format(0x000000);
    composer.copy_to_framebuffer(framebuffer_address, buffer_size);
    
    // Cleanup
    composer.destroy_framebuffer(bg_panel);
    composer.destroy_framebuffer(header);
}
```

---

## Performance Notes

- **Pixel operations:** O(1)
- **Drawing primitives:** O(pixel count)
- **Compositing:** O(framebuffer size)
- **Conversion:** O(width × height)

Use `composite_multiple()` instead of repeated `composite_framebuffer()` calls for better code clarity.

---

## Error Handling

The GraphicComposer uses **silent failure** for most operations:
- Out-of-bounds accesses are ignored
- Failed allocations don't throw (check pointers)
- Invalid operations (nullptr passed) are skipped

Check return values from `create_framebuffer()` to detect allocation failures.
