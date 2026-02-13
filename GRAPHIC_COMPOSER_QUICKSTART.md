# GraphicComposer Quick Start Guide

## 30-Second Overview

The GraphicComposer is a graphics manager that:
- Stores graphics data as 64-bit RGBA pixels internally
- Supports composable GUI elements (FrameBuffers)
- Converts to system RGB format for display
- Provides pixel drawing, shapes, and alpha blending

---

## Basic Setup

```cpp
#include <graphic_composer.h>

// Create a composer for your screen resolution
GraphicComposer composer(1024, 768);

// Draw something
composer.clear_buffer(30, 30, 30, 255);  // Dark background
composer.fill_rect(100, 100, 200, 150, 255, 0, 0, 255);  // Red rectangle

// Convert to system format and display
composer.convert_to_system_format(0x000000);  // Black background for transparent areas
composer.copy_to_framebuffer(framebuffer_address, framebuffer_size);
```

---

## Key Concepts

### RGBA Pixels
- Internally stored as 64-bit integers: `RGBAPixel`
- R, G, B, A each 0-255
- Helper functions: `rgba_make()`, `rgba_get_r()`, etc.

### FrameBuffers
- Independent graphics buffers for GUI elements
- Can be positioned with `x_offset`, `y_offset`
- Support transparency and layering

### Composition Pipeline
```
1. Draw into composition buffer (RGBA)
   ↓
2. Composite multiple FrameBuffers (layering)
   ↓
3. Convert to system format (RGBA → RGB)
   ↓
4. Copy to hardware framebuffer (display)
```

---

## Common Tasks

### Task 1: Draw a Colored Rectangle
```cpp
composer.fill_rect(
    50, 50,        // x, y position
    200, 100,      // width, height
    255, 0, 0,     // R, G, B
    255            // A (alpha) - 255 = fully opaque
);
```

### Task 2: Create a Window
```cpp
// Create a 400×300 framebuffer for a window
FrameBuffer* window = composer.create_framebuffer(400, 300);

// Fill it with a background color
composer.fill_framebuffer(window, 100, 100, 100, 255);

// Position it on screen
window->x_offset = 300;
window->y_offset = 200;

// Add it to the display
composer.composite_framebuffer(window);

// Remember to clean up when done
composer.destroy_framebuffer(window);
```

### Task 3: Draw Multiple Layers
```cpp
// Create multiple framebuffers
FrameBuffer* bg = composer.create_framebuffer(800, 600);
FrameBuffer* fg = composer.create_framebuffer(400, 300);

// Set positions
bg->x_offset = 0;
bg->y_offset = 0;
fg->x_offset = 200;
fg->y_offset = 150;

// Composite all at once
FrameBuffer* layers[] = {bg, fg};
composer.composite_multiple(layers, 2);
```

### Task 4: Transparency
```cpp
// Create a semi-transparent overlay (50% transparent = alpha 128)
FrameBuffer* overlay = composer.create_framebuffer(300, 200);
composer.fill_framebuffer(overlay, 0, 0, 0, 128);  // Black, 50% transparent

overlay->x_offset = 200;
overlay->y_offset = 100;

// When composited, will blend with what's underneath
composer.composite_framebuffer(overlay);
```

### Task 5: Draw Shapes
```cpp
// Rectangle outline
composer.draw_rect_outline(50, 50, 200, 150, 255, 255, 255, 255, 3);

// Circle
composer.draw_circle(400, 300, 50, 255, 0, 0, 255);

// Line
composer.draw_line(0, 0, 640, 480, 0, 255, 0, 255);
```

### Task 6: Complete Frame Rendering
```cpp
// Clear to background
composer.clear_buffer(25, 38, 44, 255);

// Create and position UI elements
FrameBuffer* console = composer.create_framebuffer(800, 600);
composer.fill_framebuffer(console, 27, 38, 44, 255);
console->x_offset = 10;
console->y_offset = 10;

FrameBuffer* status = composer.create_framebuffer(800, 50);
composer.fill_framebuffer(status, 50, 50, 50, 255);
status->x_offset = 10;
status->y_offset = 620;

// Composite all
FrameBuffer* ui[] = {console, status};
composer.composite_multiple(ui, 2);

// Display
composer.convert_to_system_format(0x000000);
composer.copy_to_framebuffer(framebuffer_address, framebuffer_size);

// Cleanup
composer.destroy_framebuffer(console);
composer.destroy_framebuffer(status);
```

---

## Color Values

### In RGBA (0-255 per channel)
```cpp
// Red
composer.fill_rect(10, 10, 100, 100, 255, 0, 0, 255);

// Green
composer.fill_rect(120, 10, 100, 100, 0, 255, 0, 255);

// Blue
composer.fill_rect(230, 10, 100, 100, 0, 0, 255, 255);

// Yellow (R+G)
composer.fill_rect(10, 120, 100, 100, 255, 255, 0, 255);

// Cyan (G+B)
composer.fill_rect(120, 120, 100, 100, 0, 255, 255, 255);

// Magenta (R+B)
composer.fill_rect(230, 120, 100, 100, 255, 0, 255, 255);

// White
composer.fill_rect(10, 230, 100, 100, 255, 255, 255, 255);

// Black
composer.fill_rect(120, 230, 100, 100, 0, 0, 0, 255);

// Gray
composer.fill_rect(230, 230, 100, 100, 128, 128, 128, 255);
```

### System RGB Format (Hex: 0xRRGGBB)
```cpp
composer.convert_to_system_format(0xFF0000);  // Red background
composer.convert_to_system_format(0x00FF00);  // Green background
composer.convert_to_system_format(0x0000FF);  // Blue background
composer.convert_to_system_format(0x000000);  // Black background
composer.convert_to_system_format(0xFFFFFF);  // White background
```

---

## Alpha (Transparency) Values

```cpp
255 → Fully opaque (solid)
200 → 78% opaque
128 → 50% transparent (semi-transparent)
50  → 20% opaque (mostly transparent)
0   → Fully transparent (invisible)
```

---

## Buffer Information

```cpp
// Get dimensions
uint32_t width = composer.get_width();
uint32_t height = composer.get_height();
uint32_t total_pixels = composer.get_size();

// Get memory info
uint64_t rgba_stride = composer.get_rgba_stride();  // Bytes per row (RGBA)
uint64_t rgb_stride = composer.get_system_stride(); // Bytes per row (RGB)

// Example: Estimate memory usage
uint64_t rgba_memory = width * height * 8;      // 8 bytes per pixel
uint64_t rgb_memory = width * height * 4;       // 4 bytes per pixel
uint64_t total_memory = rgba_memory + rgb_memory;
```

---

## Common Gotchas

### ❌ Mistake 1: Not calling convert_to_system_format()
```cpp
// WRONG - nothing shows up!
composer.fill_rect(100, 100, 200, 150, 255, 0, 0, 255);
composer.copy_to_framebuffer(fb_addr, fb_size);

// RIGHT
composer.fill_rect(100, 100, 200, 150, 255, 0, 0, 255);
composer.convert_to_system_format(0x000000);  // ← Don't forget this!
composer.copy_to_framebuffer(fb_addr, fb_size);
```

### ❌ Mistake 2: Forgetting to destroy framebuffers
```cpp
// WRONG - memory leak
FrameBuffer* window = composer.create_framebuffer(400, 300);
// ... use window ...
// Forgot to clean up!

// RIGHT
FrameBuffer* window = composer.create_framebuffer(400, 300);
// ... use window ...
composer.destroy_framebuffer(window);  // ← Don't forget!
```

### ❌ Mistake 3: Not setting FrameBuffer position
```cpp
// Window won't show up at expected location
FrameBuffer* window = composer.create_framebuffer(400, 300);
composer.composite_framebuffer(window);  // Appears at (0, 0)!

// RIGHT
FrameBuffer* window = composer.create_framebuffer(400, 300);
window->x_offset = 100;  // ← Set position
window->y_offset = 100;
composer.composite_framebuffer(window);  // Now at (100, 100)
```

---

## Performance Tips

1. **Create large buffers once** - Don't recreate every frame
2. **Use appropriate sizes** - Only allocate what you need
3. **Composite in order** - Back to front for correct layering
4. **Call convert once per frame** - Not every pixel operation
5. **Reuse FrameBuffers** - Create once, update content, redisplay

---

## Helper Functions (Inline)

```cpp
// Create RGBA pixel
RGBAPixel color = rgba_make(255, 128, 64, 200);

// Extract channels
uint8_t r = rgba_get_r(color);
uint8_t g = rgba_get_g(color);
uint8_t b = rgba_get_b(color);
uint8_t a = rgba_get_a(color);

// Convert formats
RGBAPixel rgba = rgb_to_rgba(0xFF8000);  // System RGB to RGBA
uint32_t rgb = rgba_to_rgb(rgba);        // RGBA to system RGB
```

---

## Real-World Example

```cpp
#include <graphic_composer.h>

void create_gui() {
    // Initialize
    GraphicComposer composer(1024, 768);
    
    // Dark background (DualFuse theme)
    composer.clear_buffer(25, 38, 44, 255);
    
    // Create console window
    FrameBuffer* console = composer.create_framebuffer(800, 600);
    composer.fill_framebuffer(console, 27, 38, 44, 255);
    console->x_offset = 100;
    console->y_offset = 50;
    
    // Add border
    composer.draw_rect_outline(100, 50, 800, 600, 187, 225, 250, 255, 2);
    
    // Create status bar
    FrameBuffer* status = composer.create_framebuffer(800, 50);
    composer.fill_framebuffer(status, 50, 50, 50, 255);
    status->x_offset = 100;
    status->y_offset = 660;
    
    // Composite everything
    FrameBuffer* ui[] = {console, status};
    composer.composite_multiple(ui, 2);
    
    // Display
    composer.convert_to_system_format(0x000000);
    composer.copy_to_framebuffer(framebuffer->address, buffer_size);
    
    // Cleanup
    composer.destroy_framebuffer(console);
    composer.destroy_framebuffer(status);
}
```

---

## Next Steps

1. Read **GRAPHIC_COMPOSER_GUIDE.md** for detailed concepts
2. Review **GRAPHIC_COMPOSER_API.md** for all available functions
3. Check **GRAPHIC_COMPOSER_INTEGRATION.md** for integration patterns
4. Run examples from **graphic_composer_examples.cpp**
5. Start building your GUI!

---

## Quick Reference Card

```
SETUP:
  GraphicComposer composer(width, height);

DRAWING:
  composer.clear_buffer(r, g, b, a);
  composer.fill_rect(x, y, w, h, r, g, b, a);
  composer.draw_line(x0, y0, x1, y1, r, g, b, a);
  composer.draw_circle(cx, cy, radius, r, g, b, a);
  composer.draw_rect_outline(x, y, w, h, r, g, b, a, thickness);

FRAMEBUFFERS:
  FrameBuffer* fb = composer.create_framebuffer(w, h);
  composer.fill_framebuffer(fb, r, g, b, a);
  fb->x_offset = x;
  fb->y_offset = y;
  composer.composite_framebuffer(fb);
  composer.destroy_framebuffer(fb);

DISPLAY:
  composer.convert_to_system_format(bg_color);
  composer.copy_to_framebuffer(fb_addr, fb_size);

COLORS:
  RGBA: (0-255, 0-255, 0-255, 0-255)
  System RGB: 0xRRGGBB
```

Happy graphics composing! 🎨
