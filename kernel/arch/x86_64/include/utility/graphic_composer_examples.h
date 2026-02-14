/**
 * GraphicComposer Usage Examples
 * 
 * This file demonstrates practical usage patterns for the GraphicComposer class
 */

#include <graphic_composer.h>
#include <stdio.h>

 
// Example 1: Basic Pixel Drawing
 

void example_basic_pixel_drawing() {
    printf("[GraphicComposer] Example 1: Basic Pixel Drawing\n");
    
    GraphicComposer composer(640, 480);
    
    // Clear to dark background
    composer.clear_buffer(30, 30, 30, 255);
    
    // Draw individual pixels
    composer.set_pixel(100, 100, 255, 0, 0, 255);      // Red
    composer.set_pixel(101, 100, 0, 255, 0, 255);      // Green
    composer.set_pixel(102, 100, 0, 0, 255, 255);      // Blue
    composer.set_pixel(103, 100, 255, 255, 0, 255);    // Yellow
    
    // Get and verify pixel
    RGBAPixel pixel = composer.get_pixel(100, 100);
    printf("  Red pixel RGBA: R=%d G=%d B=%d A=%d\n", 
           rgba_get_r(pixel), rgba_get_g(pixel), 
           rgba_get_b(pixel), rgba_get_a(pixel));
}

 
// Example 2: Drawing Shapes
 

void example_drawing_shapes() {
    printf("[GraphicComposer] Example 2: Drawing Shapes\n");
    
    GraphicComposer composer(800, 600);
    
    // Clear background
    composer.clear_buffer(50, 50, 50, 255);
    
    // Draw filled rectangles
    composer.fill_rect(50, 50, 200, 150, 200, 100, 50, 255);      // Brown rectangle
    composer.fill_rect(300, 100, 150, 150, 100, 200, 255, 255);   // Cyan rectangle
    
    // Draw outlined rectangles
    composer.draw_rect_outline(50, 250, 200, 100, 255, 255, 255, 255, 2);
    composer.draw_rect_outline(300, 250, 200, 100, 255, 255, 255, 255, 3);
    
    // Draw circles
    composer.draw_circle(150, 450, 50, 255, 0, 0, 255);           // Red circle
    composer.draw_circle(400, 450, 75, 0, 255, 0, 255);           // Green circle
    
    // Draw lines
    composer.draw_line(500, 50, 700, 250, 255, 255, 0, 255);      // Yellow line
    composer.draw_line(500, 250, 700, 50, 255, 0, 255, 255);      // Magenta line
    
    printf("  Shapes drawn successfully\n");
}

 
// Example 3: FrameBuffer Composition - Simple Window
 

void example_simple_window() {
    printf("[GraphicComposer] Example 3: Simple Window Composition\n");
    
    GraphicComposer composer(800, 600);
    
    // Clear main background
    composer.clear_buffer(40, 40, 40, 255);
    
    // Create window frame buffer (400x300)
    FrameBuffer* window = composer.create_framebuffer(400, 300);
    if (!window) {
        printf("  ERROR: Failed to create window frame buffer\n");
        return;
    }
    
    // Fill window with light gray background
    composer.fill_framebuffer(window, 200, 200, 200, 255);
    
    // Position window in center
    window->x_offset = 200;
    window->y_offset = 150;
    
    // Composite window onto main buffer
    composer.composite_framebuffer(window);
    
    printf("  Window created and composited at (%d, %d)\n", 
           window->x_offset, window->y_offset);
    
    // Cleanup
    composer.destroy_framebuffer(window);
}

 
// Example 4: Multiple Layered FrameBuffers
 

void example_multiple_layers() {
    printf("[GraphicComposer] Example 4: Multiple Layered FrameBuffers\n");
    
    GraphicComposer composer(1024, 768);
    
    // Clear background
    composer.clear_buffer(25, 38, 44, 255);  // DualFuse dark blue
    
    // Create background panel
    FrameBuffer* bg_panel = composer.create_framebuffer(1000, 700);
    composer.fill_framebuffer(bg_panel, 50, 50, 50, 255);
    bg_panel->x_offset = 12;
    bg_panel->y_offset = 34;
    
    // Create header bar
    FrameBuffer* header = composer.create_framebuffer(1000, 60);
    composer.fill_framebuffer(header, 100, 100, 100, 255);
    header->x_offset = 12;
    header->y_offset = 34;
    
    // Create status bar
    FrameBuffer* status = composer.create_framebuffer(1000, 40);
    composer.fill_framebuffer(status, 70, 70, 70, 255);
    status->x_offset = 12;
    status->y_offset = 694;
    
    // Composite in order (back to front)
    FrameBuffer* layers[] = {bg_panel, header, status};
    composer.composite_multiple(layers, 3);
    
    printf("  %d layers composited\n", 3);
    
    // Cleanup
    composer.destroy_framebuffer(bg_panel);
    composer.destroy_framebuffer(header);
    composer.destroy_framebuffer(status);
}

 
// Example 5: Semi-transparent Overlay
 

void example_semi_transparent_overlay() {
    printf("[GraphicComposer] Example 5: Semi-transparent Overlay\n");
    
    GraphicComposer composer(800, 600);
    
    // Clear with background
    composer.clear_buffer(100, 150, 200, 255);
    
    // Create semi-transparent overlay
    FrameBuffer* overlay = composer.create_framebuffer(400, 200);
    
    // Fill with semi-transparent black (alpha = 128 = 50%)
    composer.fill_framebuffer(overlay, 0, 0, 0, 128);
    
    // Position overlay in center
    overlay->x_offset = 200;
    overlay->y_offset = 200;
    
    // Composite overlay (will blend with background due to alpha)
    composer.composite_framebuffer(overlay);
    
    printf("  Semi-transparent overlay (alpha=128) composited\n");
    
    composer.destroy_framebuffer(overlay);
}

 
// Example 6: System Format Conversion
 

void example_system_format_conversion() {
    printf("[GraphicComposer] Example 6: System Format Conversion\n");
    
    GraphicComposer composer(640, 480);
    
    // Fill with colorful pattern
    for (uint32_t y = 0; y < 480; y += 60) {
        for (uint32_t x = 0; x < 640; x += 80) {
            uint8_t r = (x / 80) * 50;
            uint8_t g = (y / 60) * 50;
            uint8_t b = 128;
            composer.fill_rect(x, y, 80, 60, r, g, b, 255);
        }
    }
    
    // Convert RGBA composition buffer to system RGB format
    composer.convert_to_system_format(0x000000);  // Black background for transparency
    
    // Get the system buffer
    const uint32_t* system_buffer = composer.get_system_buffer();
    if (system_buffer) {
        printf("  Conversion complete. First pixel (RGB): 0x%08x\n", system_buffer[0]);
    } else {
        printf("  Conversion complete. System buffer not available (tempframebuffer missing)\n");
    }
    
    // Example: copy to framebuffer (assuming 640x480 frame)
    // composer.copy_to_framebuffer(framebuffer_address, 640 * 480 * 4);
    printf("  Ready to copy to hardware framebuffer\n");
}

 
// Example 7: Dynamic UI Building
 

void example_dynamic_ui_building() {
    printf("[GraphicComposer] Example 7: Dynamic UI Building\n");
    
    GraphicComposer composer(1024, 768);
    
    // Clear main area
    composer.clear_buffer(30, 30, 30, 255);
    
    // Define UI elements
    struct UIElement {
        uint32_t x, y, w, h;
        uint8_t r, g, b, a;
        const char* name;
    };
    
    UIElement elements[] = {
        {50, 50, 200, 150, 200, 100, 50, 255, "Panel1"},
        {300, 50, 200, 150, 100, 200, 100, 255, "Panel2"},
        {550, 50, 200, 150, 100, 100, 200, 255, "Panel3"},
        {50, 250, 700, 150, 80, 80, 80, 255, "ContentArea"},
        {50, 450, 700, 250, 60, 60, 60, 255, "StatusArea"},
    };
    
    // Create and composite all UI elements
    uint32_t element_count = sizeof(elements) / sizeof(UIElement);
    FrameBuffer* ui_buffers[element_count];
    
    for (uint32_t i = 0; i < element_count; i++) {
        ui_buffers[i] = composer.create_framebuffer(elements[i].w, elements[i].h);
        
        if (ui_buffers[i]) {
            composer.fill_framebuffer(ui_buffers[i], 
                                     elements[i].r, 
                                     elements[i].g, 
                                     elements[i].b, 
                                     elements[i].a);
            ui_buffers[i]->x_offset = elements[i].x;
            ui_buffers[i]->y_offset = elements[i].y;
            
            composer.composite_framebuffer(ui_buffers[i]);
            printf("  UI Element '%s' composited at (%d, %d)\n", 
                   elements[i].name, elements[i].x, elements[i].y);
        }
    }
    
    // Cleanup
    for (uint32_t i = 0; i < element_count; i++) {
        if (ui_buffers[i]) {
            composer.destroy_framebuffer(ui_buffers[i]);
        }
    }
    
    printf("  Total %d UI elements created and composited\n", element_count);
}

 
// Example 8: Color Conversion Utilities
 

void example_color_conversion() {
    printf("[GraphicComposer] Example 8: Color Conversion\n");
    
    // Create RGBA pixels using helper functions
    RGBAPixel red = rgba_make(255, 0, 0, 255);
    RGBAPixel green = rgba_make(0, 255, 0, 255);
    RGBAPixel blue = rgba_make(0, 0, 255, 255);
    
    printf("  Red RGBA:   0x%016llx\n", red);
    printf("  Green RGBA: 0x%016llx\n", green);
    printf("  Blue RGBA:  0x%016llx\n", blue);
    
    // Convert to system RGB format
    uint32_t red_rgb = rgba_to_rgb(red);
    uint32_t green_rgb = rgba_to_rgb(green);
    uint32_t blue_rgb = rgba_to_rgb(blue);
    
    printf("  Red RGB:    0x%06x\n", red_rgb);
    printf("  Green RGB:  0x%06x\n", green_rgb);
    printf("  Blue RGB:   0x%06x\n", blue_rgb);
    
    // Convert back
    RGBAPixel red_restored = rgb_to_rgba(red_rgb);
    printf("  Red restored: R=%d G=%d B=%d A=%d\n",
           rgba_get_r(red_restored),
           rgba_get_g(red_restored),
           rgba_get_b(red_restored),
           rgba_get_a(red_restored));
}

 
// Main Examples Runner
 

extern "C" void graphic_composer_examples() {
    printf("\n========================================\n");
    printf("GraphicComposer Usage Examples\n");
    printf("========================================\n\n");
    
    example_basic_pixel_drawing();
    printf("\n");
    
    example_drawing_shapes();
    printf("\n");
    
    example_simple_window();
    printf("\n");
    
    example_multiple_layers();
    printf("\n");
    
    example_semi_transparent_overlay();
    printf("\n");
    
    example_system_format_conversion();
    printf("\n");
    
    example_dynamic_ui_building();
    printf("\n");
    
    example_color_conversion();
    printf("\n");
    
    printf("========================================\n");
    printf("All examples completed successfully!\n");
    printf("========================================\n\n");
}
