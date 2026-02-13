# GraphicComposer - GUI Composition and Framebuffer Management System

## Overview

The `GraphicComposer` class is a sophisticated graphics management system designed to handle RGBA data composition and conversion to system framebuffer format. It serves as the central manager for GUI rendering, supporting multiple composable layers with alpha blending.

## Architecture

### Core Concepts

1. **RGBA Encoding (64-bit)**: All graphics data is internally stored as 64-bit `RGBAPixel` values
   - Bits 0-7: Red (R)
   - Bits 8-15: Green (G)
   - Bits 16-23: Blue (B)
   - Bits 24-31: Alpha (A)
   - Bits 32-63: Reserved for future use

2. **Dual-Buffer System**:
   - **Composition Buffer**: Internal RGBA buffer (64-bit per pixel) for all graphics operations
   - **System Buffer**: Output RGB buffer (32-bit per pixel) for system framebuffer compatibility

3. **FrameBuffer Structures**: Individual composable GUI elements stored as separate RGBA buffers with positioning information

### Memory Layout

```
┌─────────────────────────────────┐
│  Composition Buffer (RGBA)      │  64-bit per pixel
│  [width × height]               │  Full color + transparency
└─────────────────────────────────┘
           ↓ (Convert)
┌─────────────────────────────────┐
│  System Buffer (RGB)            │  32-bit per pixel
│  [width × height]               │  System framebuffer format
└─────────────────────────────────┘
           ↓ (Copy)
┌─────────────────────────────────┐
│  System Framebuffer             │  Hardware display
└─────────────────────────────────┘
```

## Key Features

### 1. RGBA Pixel Manipulation

```cpp
// Set individual pixels with RGBA values
composer.set_pixel(100, 50, 255, 128, 0, 200);  // Semi-transparent orange

// Get pixel data
RGBAPixel pixel = composer.get_pixel(100, 50);

// Direct encoded pixel access
composer.set_pixel_encoded(100, 50, rgba_make(255, 128, 0, 200));
```

### 2. Drawing Primitives

```cpp
// Filled rectangle
composer.fill_rect(10, 10, 200, 100, 0, 200, 255, 255);  // Opaque cyan

// Rectangle outline
composer.draw_rect_outline(10, 10, 200, 100, 0, 0, 0, 255, 3);  // Black, 3px thick

// Circle
composer.draw_circle(150, 150, 50, 255, 0, 0, 255);  // Opaque red circle

// Line
composer.draw_line(0, 0, 200, 200, 255, 255, 255, 255);  // White diagonal line
```

### 3. Buffer Management

```cpp
// Clear entire buffer
composer.clear_buffer(30, 30, 30, 255);  // Dark gray background

// Clear to transparent
composer.clear_transparent();
```

### 4. Composable GUI Elements

```cpp
// Create a FrameBuffer for a GUI element
FrameBuffer* window = composer.create_framebuffer(400, 300);

// Fill the frame buffer with a background color
composer.fill_framebuffer(window, 200, 200, 200, 255);  // Light gray

// Position it on the main composition
window->x_offset = 50;
window->y_offset = 50;

// Composite onto main buffer
composer.composite_framebuffer(window);

// Composite multiple frames at once
FrameBuffer* frames[] = {window, another_frame};
composer.composite_multiple(frames, 2);

// Clean up
composer.destroy_framebuffer(window);
```

### 5. Alpha Blending

The composer automatically applies alpha blending when:
- Compositing FrameBuffers with transparency
- Converting RGBA to RGB system format
- Multiple elements overlap

Alpha blending formula:
```
output = foreground × alpha + background × (1 - alpha)
```

### 6. System Framebuffer Conversion

```cpp
// Convert internal RGBA buffer to system RGB format
// Uses black (0x000000) as background for transparent areas
composer.convert_to_system_format(0x000000);

// Get the converted buffer
const uint32_t* system_buffer = composer.get_system_buffer();

// Copy to actual hardware framebuffer
composer.copy_to_framebuffer(framebuffer_address, framebuffer_size);
```

## Usage Example

### Complete GUI Rendering Pipeline

```cpp
// Initialize
GraphicComposer composer(800, 600);

// Clear background
composer.clear_buffer(25, 38, 44, 255);  // Dark background (#1B262C)

// Create window frame buffer
FrameBuffer* console_window = composer.create_framebuffer(800, 600);
composer.fill_framebuffer(console_window, 27, 38, 44, 255);
console_window->x_offset = 10;
console_window->y_offset = 10;

// Draw window border
composer.draw_rect_outline(0, 0, 790, 590, 187, 225, 250, 255, 2);

// Create another element (e.g., status bar)
FrameBuffer* status_bar = composer.create_framebuffer(800, 50);
composer.fill_framebuffer(status_bar, 50, 50, 50, 255);
status_bar->x_offset = 10;
status_bar->y_offset = 610;

// Composite all elements
composer.composite_framebuffer(console_window);
composer.composite_framebuffer(status_bar);

// Convert to system format and display
composer.convert_to_system_format(0x000000);
composer.copy_to_framebuffer(system_framebuffer_addr, frame_size);

// Cleanup
composer.destroy_framebuffer(console_window);
composer.destroy_framebuffer(status_bar);
```

## RGBA Helper Functions

The header provides inline helper functions for working with RGBA pixels:

```cpp
// Create RGBA pixel
RGBAPixel color = rgba_make(255, 128, 0, 200);

// Extract channels
uint8_t red = rgba_get_r(color);
uint8_t green = rgba_get_g(color);
uint8_t blue = rgba_get_b(color);
uint8_t alpha = rgba_get_a(color);

// Convert between formats
RGBAPixel rgba = rgb_to_rgba(0xFF8000);  // Convert system RGB to RGBA
uint32_t rgb = rgba_to_rgb(color);       // Convert RGBA to system RGB
```

## Color Formats

### RGBA Internal Format (64-bit)
```
Bits:  24-31    16-23    8-15    0-7
      ┌─────┬─────────┬─────────┬─────┐
      │  A  │    B    │    G    │  R  │
      └─────┴─────────┴─────────┴─────┘
```

### RGB System Format (32-bit)
```
Bits:  16-23    8-15    0-7
      ┌─────────┬─────────┬─────┐
      │    R    │    G    │  B  │
      └─────────┴─────────┴─────┘
```

## API Reference

### Constructors & Destructors
- `GraphicComposer(uint32_t width, uint32_t height)` - Create composer
- `~GraphicComposer()` - Destroy and free resources

### Pixel Operations
- `void set_pixel(x, y, r, g, b, a)` - Set pixel with RGBA
- `RGBAPixel get_pixel(x, y)` - Get pixel
- `void set_pixel_encoded(x, y, pixel)` - Set encoded pixel

### Drawing Primitives
- `void clear_buffer(r, g, b, a)` - Clear all pixels
- `void clear_transparent()` - Clear to transparent
- `void fill_rect(x, y, width, height, r, g, b, a)` - Fill rectangle
- `void draw_line(x0, y0, x1, y1, r, g, b, a)` - Draw line (Bresenham)
- `void draw_rect_outline(x, y, width, height, r, g, b, a, thickness)` - Rectangle outline
- `void draw_circle(cx, cy, radius, r, g, b, a)` - Filled circle

### FrameBuffer Operations
- `FrameBuffer* create_framebuffer(width, height)` - Create frame buffer
- `void destroy_framebuffer(fb)` - Free frame buffer
- `void composite_framebuffer(fb)` - Layer frame buffer
- `void composite_multiple(fbs[], count)` - Layer multiple buffers
- `void fill_framebuffer(fb, r, g, b, a)` - Fill frame buffer
- `void copy_framebuffer(src, dst, dst_x, dst_y)` - Copy between buffers

### Conversion & Output
- `void convert_to_system_format(bg_color)` - Convert RGBA to RGB
- `const uint32_t* get_system_buffer()` - Get RGB buffer
- `void copy_to_framebuffer(ptr, size)` - Copy to hardware framebuffer

### Getters
- `RGBAPixel* get_composition_buffer()` - Get RGBA buffer
- `uint32_t get_width()`, `get_height()`, `get_size()` - Buffer dimensions
- `uint64_t get_rgba_stride()`, `get_system_stride()` - Buffer strides

## Performance Considerations

1. **Memory Usage**:
   - Composition buffer: width × height × 8 bytes
   - System buffer: width × height × 4 bytes
   - Total: width × height × 12 bytes

2. **Optimization Tips**:
   - Use smaller FrameBuffers for individual GUI elements
   - Composite in layers from back to front
   - Call `convert_to_system_format()` only once per frame
   - Use `clear_transparent()` when starting fresh

3. **Alpha Blending**:
   - Fully opaque (alpha=255) skips blending
   - Fully transparent (alpha=0) uses background
   - Partial transparency incurs blending cost

## Integration with Console

The GraphicComposer can work alongside the existing Console class:

```cpp
// Composer for main GUI
GraphicComposer gui_composer(screen_width, screen_height);

// Console for text output
console = Console(800, 600, 10, 10);

// Both can render to the same final framebuffer with proper layering
```

## Future Extensions

Possible enhancements:
- Texture support
- Text rendering integration
- Hardware acceleration hooks
- Double-buffering with page flipping
- Dirty region tracking for partial updates
- Rotation and scaling transforms
- Clipping regions
