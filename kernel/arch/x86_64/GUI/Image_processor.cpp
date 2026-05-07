#include <image_processor.h>
#include <liballoc.h>
#include <string.h>

static inline uint8_t clamp_to_byte(int32_t value)
{
    if (value < 0)   return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

static inline uint8_t pixel_red_channel(uint32_t pixel)   { return (pixel >> 16) & 0xFF; }
static inline uint8_t pixel_green_channel(uint32_t pixel) { return (pixel >> 8)  & 0xFF; }
static inline uint8_t pixel_blue_channel(uint32_t pixel)  { return  pixel        & 0xFF; }

static inline uint32_t pack_argb_channels(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue)
{
    return ((uint32_t)alpha << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
}

static inline uint8_t pixel_luminance(uint32_t pixel)
{
    return (uint8_t)(
        (pixel_red_channel(pixel)   * 299u +
         pixel_green_channel(pixel) * 587u +
         pixel_blue_channel(pixel)  * 114u) / 1000u);
}

static void filter_apply_red_channel_as_grayscale(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t red_value = pixel_red_channel(pixels[i]);
        pixels[i] = pack_argb_channels(0xFF, red_value, red_value, red_value);
    }
}

static void filter_apply_green_channel_as_grayscale(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t green_value = pixel_green_channel(pixels[i]);
        pixels[i] = pack_argb_channels(0xFF, green_value, green_value, green_value);
    }
}

static void filter_apply_blue_channel_as_grayscale(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t blue_value = pixel_blue_channel(pixels[i]);
        pixels[i] = pack_argb_channels(0xFF, blue_value, blue_value, blue_value);
    }
}

static void filter_apply_luminance_grayscale(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t luma = pixel_luminance(pixels[i]);
        pixels[i] = pack_argb_channels(0xFF, luma, luma, luma);
    }
}

static void filter_apply_histogram_equalization(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;

    uint32_t luminance_histogram[256] = {};
    for (uint32_t i = 0; i < pixel_count; i++)
        luminance_histogram[pixel_luminance(pixels[i])]++;

    uint32_t cumulative_distribution[256] = {};
    cumulative_distribution[0] = luminance_histogram[0];
    for (int bin = 1; bin < 256; bin++)
        cumulative_distribution[bin] = cumulative_distribution[bin - 1] + luminance_histogram[bin];

    uint32_t cdf_minimum_nonzero = 0;
    for (int bin = 0; bin < 256; bin++) {
        if (cumulative_distribution[bin] > 0) {
            cdf_minimum_nonzero = cumulative_distribution[bin];
            break;
        }
    }

    uint8_t equalization_lut[256];
    uint32_t cdf_range = pixel_count - cdf_minimum_nonzero;
    for (int bin = 0; bin < 256; bin++) {
        if (cdf_range == 0) {
            equalization_lut[bin] = (uint8_t)bin;
        } else {
            int32_t mapped = ((int32_t)(cumulative_distribution[bin] - cdf_minimum_nonzero) * 255)
                           / (int32_t)cdf_range;
            equalization_lut[bin] = clamp_to_byte(mapped);
        }
    }

    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t original_luma = pixel_luminance(pixels[i]);
        uint8_t equalized_luma = equalization_lut[original_luma];
        if (original_luma == 0) {
            pixels[i] = pack_argb_channels(0xFF, 0, 0, 0);
        } else {
            uint8_t r = clamp_to_byte((int32_t)pixel_red_channel(pixels[i])   * equalized_luma / original_luma);
            uint8_t g = clamp_to_byte((int32_t)pixel_green_channel(pixels[i]) * equalized_luma / original_luma);
            uint8_t b = clamp_to_byte((int32_t)pixel_blue_channel(pixels[i])  * equalized_luma / original_luma);
            pixels[i] = pack_argb_channels(0xFF, r, g, b);
        }
    }
}

static void filter_apply_gaussian_blur_3x3(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t* source_copy = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (!source_copy) return;
    memcpy(source_copy, pixels, width * height * sizeof(uint32_t));

    static const int gaussian_kernel_3x3_weights[3][3] = { {1, 2, 1}, {2, 4, 2}, {1, 2, 1} };
    const int gaussian_kernel_weight_sum = 16;

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int32_t weighted_red = 0, weighted_green = 0, weighted_blue = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint32_t neighbor = source_copy[(y + ky) * width + (x + kx)];
                    int kernel_weight = gaussian_kernel_3x3_weights[ky + 1][kx + 1];
                    weighted_red   += pixel_red_channel(neighbor)   * kernel_weight;
                    weighted_green += pixel_green_channel(neighbor) * kernel_weight;
                    weighted_blue  += pixel_blue_channel(neighbor)  * kernel_weight;
                }
            }
            pixels[y * width + x] = pack_argb_channels(0xFF,
                clamp_to_byte(weighted_red   / gaussian_kernel_weight_sum),
                clamp_to_byte(weighted_green / gaussian_kernel_weight_sum),
                clamp_to_byte(weighted_blue  / gaussian_kernel_weight_sum));
        }
    }
    free(source_copy);
}

static void filter_apply_sobel_edge_detection(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    uint8_t* grayscale_buffer = (uint8_t*)malloc(pixel_count);
    uint32_t* edge_output_buffer = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!grayscale_buffer || !edge_output_buffer) {
        free(grayscale_buffer);
        free(edge_output_buffer);
        return;
    }

    for (uint32_t i = 0; i < pixel_count; i++)
        grayscale_buffer[i] = pixel_luminance(pixels[i]);

    memset(edge_output_buffer, 0, pixel_count * sizeof(uint32_t));

    static const int sobel_kernel_x[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
    static const int sobel_kernel_y[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int32_t gradient_x = 0, gradient_y = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint8_t gray_sample = grayscale_buffer[(y + ky) * width + (x + kx)];
                    gradient_x += gray_sample * sobel_kernel_x[ky + 1][kx + 1];
                    gradient_y += gray_sample * sobel_kernel_y[ky + 1][kx + 1];
                }
            }
            int32_t gradient_magnitude = (gradient_x < 0 ? -gradient_x : gradient_x)
                                       + (gradient_y < 0 ? -gradient_y : gradient_y);
            if (gradient_magnitude > 255) gradient_magnitude = 255;
            uint8_t edge_intensity = (uint8_t)gradient_magnitude;
            edge_output_buffer[y * width + x] = pack_argb_channels(0xFF, edge_intensity, edge_intensity, edge_intensity);
        }
    }

    memcpy(pixels, edge_output_buffer, pixel_count * sizeof(uint32_t));
    free(grayscale_buffer);
    free(edge_output_buffer);
}

static void filter_apply_brightness_increase(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        pixels[i] = pack_argb_channels(0xFF,
            clamp_to_byte((int32_t)pixel_red_channel(pixels[i])   + 30),
            clamp_to_byte((int32_t)pixel_green_channel(pixels[i]) + 30),
            clamp_to_byte((int32_t)pixel_blue_channel(pixels[i])  + 30));
    }
}

static void filter_apply_negative(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        pixels[i] = pack_argb_channels(0xFF,
            255 - pixel_red_channel(pixels[i]),
            255 - pixel_green_channel(pixels[i]),
            255 - pixel_blue_channel(pixels[i]));
    }
}

static uint8_t compute_median_of_9_bytes(uint8_t values[9])
{
    for (int i = 1; i < 9; i++) {
        uint8_t key = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = key;
    }
    return values[4];
}

static void filter_apply_median_3x3(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    uint32_t* output_buffer = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!output_buffer) return;
    memcpy(output_buffer, pixels, pixel_count * sizeof(uint32_t));

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint8_t red_window[9], green_window[9], blue_window[9];
            int window_index = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint32_t neighbor = pixels[(y + ky) * width + (x + kx)];
                    red_window[window_index]   = pixel_red_channel(neighbor);
                    green_window[window_index] = pixel_green_channel(neighbor);
                    blue_window[window_index]  = pixel_blue_channel(neighbor);
                    window_index++;
                }
            }
            output_buffer[y * width + x] = pack_argb_channels(0xFF,
                compute_median_of_9_bytes(red_window),
                compute_median_of_9_bytes(green_window),
                compute_median_of_9_bytes(blue_window));
        }
    }

    memcpy(pixels, output_buffer, pixel_count * sizeof(uint32_t));
    free(output_buffer);
}

static void filter_apply_laplacian_4neighbor(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    uint8_t* grayscale_input = (uint8_t*)malloc(pixel_count);
    uint32_t* output_buffer  = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!grayscale_input || !output_buffer) {
        free(grayscale_input);
        free(output_buffer);
        return;
    }

    for (uint32_t i = 0; i < pixel_count; i++)
        grayscale_input[i] = pixel_luminance(pixels[i]);

    memset(output_buffer, 0, pixel_count * sizeof(uint32_t));

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int32_t laplacian_response =
                  (int32_t)grayscale_input[(y - 1) * width + x]
                + (int32_t)grayscale_input[(y + 1) * width + x]
                + (int32_t)grayscale_input[y * width + (x - 1)]
                + (int32_t)grayscale_input[y * width + (x + 1)]
                - 4 * (int32_t)grayscale_input[y * width + x];

            uint8_t output_intensity = clamp_to_byte(laplacian_response + 128);
            output_buffer[y * width + x] = pack_argb_channels(0xFF, output_intensity, output_intensity, output_intensity);
        }
    }

    memcpy(pixels, output_buffer, pixel_count * sizeof(uint32_t));
    free(grayscale_input);
    free(output_buffer);
}

static void filter_apply_sharpen_3x3(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    uint32_t* source_copy = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!source_copy) return;
    memcpy(source_copy, pixels, pixel_count * sizeof(uint32_t));

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint32_t center = source_copy[y * width + x];
            uint32_t up     = source_copy[(y - 1) * width + x];
            uint32_t down   = source_copy[(y + 1) * width + x];
            uint32_t left   = source_copy[y * width + (x - 1)];
            uint32_t right  = source_copy[y * width + (x + 1)];

            int32_t sharpened_red   = 5 * pixel_red_channel(center)   - pixel_red_channel(up)   - pixel_red_channel(down)   - pixel_red_channel(left)   - pixel_red_channel(right);
            int32_t sharpened_green = 5 * pixel_green_channel(center) - pixel_green_channel(up) - pixel_green_channel(down) - pixel_green_channel(left) - pixel_green_channel(right);
            int32_t sharpened_blue  = 5 * pixel_blue_channel(center)  - pixel_blue_channel(up)  - pixel_blue_channel(down)  - pixel_blue_channel(left)  - pixel_blue_channel(right);

            pixels[y * width + x] = pack_argb_channels(0xFF,
                clamp_to_byte(sharpened_red),
                clamp_to_byte(sharpened_green),
                clamp_to_byte(sharpened_blue));
        }
    }
    free(source_copy);
}

static void filter_apply_horizontal_motion_blur_5px(uint32_t* pixels, uint32_t width, uint32_t height)
{
    const int motion_kernel_half_width = 2;
    const int motion_kernel_total_width = 5;

    uint32_t pixel_count = width * height;
    uint32_t* source_copy = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!source_copy) return;
    memcpy(source_copy, pixels, pixel_count * sizeof(uint32_t));

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = (uint32_t)motion_kernel_half_width; x < width - (uint32_t)motion_kernel_half_width; x++) {
            int32_t sum_red = 0, sum_green = 0, sum_blue = 0;
            for (int kx = -motion_kernel_half_width; kx <= motion_kernel_half_width; kx++) {
                uint32_t neighbor = source_copy[y * width + x + kx];
                sum_red   += pixel_red_channel(neighbor);
                sum_green += pixel_green_channel(neighbor);
                sum_blue  += pixel_blue_channel(neighbor);
            }
            pixels[y * width + x] = pack_argb_channels(0xFF,
                clamp_to_byte(sum_red   / motion_kernel_total_width),
                clamp_to_byte(sum_green / motion_kernel_total_width),
                clamp_to_byte(sum_blue  / motion_kernel_total_width));
        }
    }
    free(source_copy);
}

static void filter_apply_bilateral_5x5(uint32_t* pixels, uint32_t width, uint32_t height)
{
    static const uint32_t bilateral_gaussian_spatial_weights_5x5[5][5] = {
        {169, 329, 411, 329, 169},
        {329, 641, 800, 641, 329},
        {411, 800, 1000, 800, 411},
        {329, 641, 800, 641, 329},
        {169, 329, 411, 329, 169}
    };

    uint32_t pixel_count = width * height;
    uint32_t* output_buffer = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!output_buffer) return;
    memcpy(output_buffer, pixels, pixel_count * sizeof(uint32_t));

    const int bilateral_half_kernel = 2;
    const int bilateral_range_sigma_squared = 2500; // sigma_r = 50

    for (uint32_t y = (uint32_t)bilateral_half_kernel; y < height - (uint32_t)bilateral_half_kernel; y++) {
        for (uint32_t x = (uint32_t)bilateral_half_kernel; x < width - (uint32_t)bilateral_half_kernel; x++) {
            uint32_t center_pixel = pixels[y * width + x];
            int32_t center_r = pixel_red_channel(center_pixel);
            int32_t center_g = pixel_green_channel(center_pixel);
            int32_t center_b = pixel_blue_channel(center_pixel);

            int64_t weighted_sum_red = 0, weighted_sum_green = 0, weighted_sum_blue = 0;
            int64_t total_weight = 0;

            for (int ky = -bilateral_half_kernel; ky <= bilateral_half_kernel; ky++) {
                for (int kx = -bilateral_half_kernel; kx <= bilateral_half_kernel; kx++) {
                    uint32_t neighbor_pixel = pixels[(y + ky) * width + (x + kx)];
                    int32_t neighbor_r = pixel_red_channel(neighbor_pixel);
                    int32_t neighbor_g = pixel_green_channel(neighbor_pixel);
                    int32_t neighbor_b = pixel_blue_channel(neighbor_pixel);

                    int32_t color_diff_r = neighbor_r - center_r;
                    int32_t color_diff_g = neighbor_g - center_g;
                    int32_t color_diff_b = neighbor_b - center_b;
                    int32_t color_distance_squared = color_diff_r * color_diff_r
                                                   + color_diff_g * color_diff_g
                                                   + color_diff_b * color_diff_b;

                    uint32_t range_weight = (uint32_t)(256 * bilateral_range_sigma_squared)
                                         / (uint32_t)(bilateral_range_sigma_squared + color_distance_squared + 1);

                    uint32_t spatial_weight = bilateral_gaussian_spatial_weights_5x5[ky + bilateral_half_kernel][kx + bilateral_half_kernel];
                    uint64_t combined_weight = (uint64_t)spatial_weight * range_weight;

                    weighted_sum_red   += neighbor_r * combined_weight;
                    weighted_sum_green += neighbor_g * combined_weight;
                    weighted_sum_blue  += neighbor_b * combined_weight;
                    total_weight       += combined_weight;
                }
            }

            if (total_weight > 0) {
                output_buffer[y * width + x] = pack_argb_channels(0xFF,
                    clamp_to_byte((int32_t)(weighted_sum_red   / total_weight)),
                    clamp_to_byte((int32_t)(weighted_sum_green / total_weight)),
                    clamp_to_byte((int32_t)(weighted_sum_blue  / total_weight)));
            }
        }
    }

    memcpy(pixels, output_buffer, pixel_count * sizeof(uint32_t));
    free(output_buffer);
}

static void filter_apply_emboss_nw_lightsource(uint32_t* pixels, uint32_t width, uint32_t height)
{
    static const int emboss_kernel_3x3[3][3] = { {-2, -1, 0}, {-1, 1, 1}, {0, 1, 2} };

    uint32_t pixel_count = width * height;
    uint8_t* grayscale_input = (uint8_t*)malloc(pixel_count);
    uint32_t* output_buffer  = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!grayscale_input || !output_buffer) {
        free(grayscale_input);
        free(output_buffer);
        return;
    }

    for (uint32_t i = 0; i < pixel_count; i++)
        grayscale_input[i] = pixel_luminance(pixels[i]);

    for (uint32_t i = 0; i < pixel_count; i++)
        output_buffer[i] = pack_argb_channels(0xFF, 128, 128, 128);

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int32_t emboss_response = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    emboss_response += grayscale_input[(y + ky) * width + (x + kx)]
                                     * emboss_kernel_3x3[ky + 1][kx + 1];
                }
            }
            uint8_t emboss_intensity = clamp_to_byte(emboss_response + 128);
            output_buffer[y * width + x] = pack_argb_channels(0xFF, emboss_intensity, emboss_intensity, emboss_intensity);
        }
    }

    memcpy(pixels, output_buffer, pixel_count * sizeof(uint32_t));
    free(grayscale_input);
    free(output_buffer);
}

static uint32_t compute_integer_square_root(uint32_t n)
{
    if (n == 0) return 0;
    uint32_t estimate = n;
    uint32_t refined  = (estimate + 1) / 2;
    while (refined < estimate) {
        estimate = refined;
        refined  = (estimate + n / estimate) / 2;
    }
    return estimate;
}

static void filter_apply_canny_edge_detection(uint32_t* pixels, uint32_t width, uint32_t height)
{
    const uint32_t canny_low_threshold_value  = 50;
    const uint32_t canny_high_threshold_value = 150;
    const uint8_t  canny_strong_edge_marker   = 255;
    const uint8_t  canny_weak_edge_marker     = 128;

    uint32_t pixel_count = width * height;

    uint8_t*  grayscale_input_buffer    = (uint8_t*)malloc(pixel_count);
    uint8_t*  gaussian_smoothed_buffer  = (uint8_t*)malloc(pixel_count);
    int16_t*  sobel_gradient_x_buffer   = (int16_t*)malloc(pixel_count * sizeof(int16_t));
    int16_t*  sobel_gradient_y_buffer   = (int16_t*)malloc(pixel_count * sizeof(int16_t));
    uint32_t* gradient_magnitude_buffer = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    uint8_t*  gradient_direction_buffer = (uint8_t*)malloc(pixel_count);
    uint8_t*  nonmax_suppressed_buffer  = (uint8_t*)malloc(pixel_count);
    uint8_t*  hysteresis_edge_buffer    = (uint8_t*)malloc(pixel_count);

    if (!grayscale_input_buffer || !gaussian_smoothed_buffer || !sobel_gradient_x_buffer ||
        !sobel_gradient_y_buffer || !gradient_magnitude_buffer || !gradient_direction_buffer ||
        !nonmax_suppressed_buffer || !hysteresis_edge_buffer) {
        free(grayscale_input_buffer);   free(gaussian_smoothed_buffer);
        free(sobel_gradient_x_buffer);  free(sobel_gradient_y_buffer);
        free(gradient_magnitude_buffer); free(gradient_direction_buffer);
        free(nonmax_suppressed_buffer); free(hysteresis_edge_buffer);
        return;
    }

    memset(grayscale_input_buffer,    0, pixel_count);
    memset(gaussian_smoothed_buffer,  0, pixel_count);
    memset(nonmax_suppressed_buffer,  0, pixel_count);
    memset(hysteresis_edge_buffer,    0, pixel_count);
    memset(gradient_magnitude_buffer, 0, pixel_count * sizeof(uint32_t));

    for (uint32_t i = 0; i < pixel_count; i++)
        grayscale_input_buffer[i] = pixel_luminance(pixels[i]);

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint32_t gaussian_weighted_sum =
                grayscale_input_buffer[(y-1)*width+(x-1)] * 1 + grayscale_input_buffer[(y-1)*width+x] * 2 + grayscale_input_buffer[(y-1)*width+(x+1)] * 1 +
                grayscale_input_buffer[ y   *width+(x-1)] * 2 + grayscale_input_buffer[ y   *width+x] * 4 + grayscale_input_buffer[ y   *width+(x+1)] * 2 +
                grayscale_input_buffer[(y+1)*width+(x-1)] * 1 + grayscale_input_buffer[(y+1)*width+x] * 2 + grayscale_input_buffer[(y+1)*width+(x+1)] * 1;
            gaussian_smoothed_buffer[y * width + x] = (uint8_t)(gaussian_weighted_sum / 16);
        }
    }

    static const int sobel_kernel_x_3x3[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
    static const int sobel_kernel_y_3x3[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int32_t gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint8_t sample = gaussian_smoothed_buffer[(y + ky) * width + (x + kx)];
                    gx += sample * sobel_kernel_x_3x3[ky + 1][kx + 1];
                    gy += sample * sobel_kernel_y_3x3[ky + 1][kx + 1];
                }
            }
            sobel_gradient_x_buffer[y * width + x] = (int16_t)gx;
            sobel_gradient_y_buffer[y * width + x] = (int16_t)gy;
            gradient_magnitude_buffer[y * width + x] = compute_integer_square_root((uint32_t)(gx * gx + gy * gy));

            int32_t abs_gx = gx < 0 ? -gx : gx;
            int32_t abs_gy = gy < 0 ? -gy : gy;

            if      (abs_gx == 0 && abs_gy == 0)  gradient_direction_buffer[y * width + x] = 0;
            else if (abs_gx >= abs_gy * 2)         gradient_direction_buffer[y * width + x] = 0;
            else if (abs_gy >= abs_gx * 2)         gradient_direction_buffer[y * width + x] = 2;
            else if ((gx > 0 && gy > 0) || (gx < 0 && gy < 0)) gradient_direction_buffer[y * width + x] = 1;
            else                                   gradient_direction_buffer[y * width + x] = 3;
        }
    }

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint32_t current_magnitude = gradient_magnitude_buffer[y * width + x];
            uint8_t  gradient_direction = gradient_direction_buffer[y * width + x];
            uint32_t neighbor_magnitude_a, neighbor_magnitude_b;

            switch (gradient_direction) {
                case 0:
                    neighbor_magnitude_a = gradient_magnitude_buffer[y * width + (x - 1)];
                    neighbor_magnitude_b = gradient_magnitude_buffer[y * width + (x + 1)];
                    break;
                case 1:
                    neighbor_magnitude_a = gradient_magnitude_buffer[(y - 1) * width + (x + 1)];
                    neighbor_magnitude_b = gradient_magnitude_buffer[(y + 1) * width + (x - 1)];
                    break;
                case 2:
                    neighbor_magnitude_a = gradient_magnitude_buffer[(y - 1) * width + x];
                    neighbor_magnitude_b = gradient_magnitude_buffer[(y + 1) * width + x];
                    break;
                default:
                    neighbor_magnitude_a = gradient_magnitude_buffer[(y - 1) * width + (x - 1)];
                    neighbor_magnitude_b = gradient_magnitude_buffer[(y + 1) * width + (x + 1)];
                    break;
            }

            nonmax_suppressed_buffer[y * width + x] =
                (current_magnitude >= neighbor_magnitude_a && current_magnitude >= neighbor_magnitude_b)
                ? (uint8_t)(current_magnitude > 255 ? 255 : current_magnitude)
                : 0;
        }
    }

    for (uint32_t i = 0; i < pixel_count; i++) {
        if      (nonmax_suppressed_buffer[i] >= (uint8_t)canny_high_threshold_value) hysteresis_edge_buffer[i] = canny_strong_edge_marker;
        else if (nonmax_suppressed_buffer[i] >= (uint8_t)canny_low_threshold_value)  hysteresis_edge_buffer[i] = canny_weak_edge_marker;
        else                                                                          hysteresis_edge_buffer[i] = 0;
    }

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            if (hysteresis_edge_buffer[y * width + x] == canny_weak_edge_marker) {
                bool connected_to_strong_edge = false;
                for (int ky = -1; ky <= 1 && !connected_to_strong_edge; ky++) {
                    for (int kx = -1; kx <= 1 && !connected_to_strong_edge; kx++) {
                        if (hysteresis_edge_buffer[(y + ky) * width + (x + kx)] == canny_strong_edge_marker)
                            connected_to_strong_edge = true;
                    }
                }
                hysteresis_edge_buffer[y * width + x] = connected_to_strong_edge ? canny_strong_edge_marker : 0;
            }
        }
    }

    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t final_edge_value = (hysteresis_edge_buffer[i] == canny_strong_edge_marker) ? 255 : 0;
        pixels[i] = pack_argb_channels(0xFF, final_edge_value, final_edge_value, final_edge_value);
    }

    free(grayscale_input_buffer);   free(gaussian_smoothed_buffer);
    free(sobel_gradient_x_buffer);  free(sobel_gradient_y_buffer);
    free(gradient_magnitude_buffer); free(gradient_direction_buffer);
    free(nonmax_suppressed_buffer); free(hysteresis_edge_buffer);
}

static void filter_apply_contrast_scale_1_5x(uint32_t* pixels, uint32_t width, uint32_t height)
{
    uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        int32_t r = ((int32_t)pixel_red_channel(pixels[i])   - 128) * 3 / 2 + 128;
        int32_t g = ((int32_t)pixel_green_channel(pixels[i]) - 128) * 3 / 2 + 128;
        int32_t b = ((int32_t)pixel_blue_channel(pixels[i])  - 128) * 3 / 2 + 128;
        pixels[i] = pack_argb_channels(0xFF, clamp_to_byte(r), clamp_to_byte(g), clamp_to_byte(b));
    }
}

static const char* filter_suffix_lookup_table[IMAGE_FILTER_COUNT] = {
    "_red", "_green", "_blue", "_gray",
    "",
    "_histeq", "_gaussian", "_sobel", "_bright", "_negative",
    "_median", "_laplacian", "_sharpen", "_motblur",
    "_bilateral", "_emboss", "_canny", "_contrast"
};

void image_processor_apply(ImageFilterType filter, uint32_t* pixels, uint32_t width, uint32_t height)
{
    switch (filter) {
        case IMAGE_FILTER_RED_GRAYSCALE:      filter_apply_red_channel_as_grayscale(pixels, width, height);    break;
        case IMAGE_FILTER_GREEN_GRAYSCALE:    filter_apply_green_channel_as_grayscale(pixels, width, height);  break;
        case IMAGE_FILTER_BLUE_GRAYSCALE:     filter_apply_blue_channel_as_grayscale(pixels, width, height);   break;
        case IMAGE_FILTER_GRAYSCALE:          filter_apply_luminance_grayscale(pixels, width, height);         break;
        case IMAGE_FILTER_HISTOGRAM_EQUALIZE: filter_apply_histogram_equalization(pixels, width, height);      break;
        case IMAGE_FILTER_GAUSSIAN_BLUR:      filter_apply_gaussian_blur_3x3(pixels, width, height);           break;
        case IMAGE_FILTER_SOBEL_EDGES:        filter_apply_sobel_edge_detection(pixels, width, height);        break;
        case IMAGE_FILTER_BRIGHTNESS:         filter_apply_brightness_increase(pixels, width, height);         break;
        case IMAGE_FILTER_NEGATIVE:           filter_apply_negative(pixels, width, height);                    break;
        case IMAGE_FILTER_MEDIAN:             filter_apply_median_3x3(pixels, width, height);                  break;
        case IMAGE_FILTER_LAPLACIAN:          filter_apply_laplacian_4neighbor(pixels, width, height);         break;
        case IMAGE_FILTER_SHARPEN:            filter_apply_sharpen_3x3(pixels, width, height);                 break;
        case IMAGE_FILTER_MOTION_BLUR:        filter_apply_horizontal_motion_blur_5px(pixels, width, height);  break;
        case IMAGE_FILTER_BILATERAL:          filter_apply_bilateral_5x5(pixels, width, height);               break;
        case IMAGE_FILTER_EMBOSS:             filter_apply_emboss_nw_lightsource(pixels, width, height);       break;
        case IMAGE_FILTER_CANNY:              filter_apply_canny_edge_detection(pixels, width, height);        break;
        case IMAGE_FILTER_CONTRAST:           filter_apply_contrast_scale_1_5x(pixels, width, height);         break;
        default: break;
    }
}

void image_processor_compute_luminance_histogram(const uint32_t* pixels, uint32_t pixel_count, uint32_t out_histogram_bins[256])
{
    memset(out_histogram_bins, 0, 256 * sizeof(uint32_t));
    for (uint32_t i = 0; i < pixel_count; i++)
        out_histogram_bins[pixel_luminance(pixels[i])]++;
}

const char* image_processor_filter_file_suffix(ImageFilterType filter)
{
    if ((int)filter < 0 || filter >= IMAGE_FILTER_COUNT) return "";
    return filter_suffix_lookup_table[filter];
}

bool image_processor_filter_produces_saved_image(ImageFilterType filter)
{
    return filter != IMAGE_FILTER_HISTOGRAM_TOGGLE;
}