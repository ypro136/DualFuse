#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    IMAGE_FILTER_RED_GRAYSCALE = 0,
    IMAGE_FILTER_GREEN_GRAYSCALE,
    IMAGE_FILTER_BLUE_GRAYSCALE,
    IMAGE_FILTER_GRAYSCALE,
    IMAGE_FILTER_HISTOGRAM_TOGGLE,
    IMAGE_FILTER_HISTOGRAM_EQUALIZE,
    IMAGE_FILTER_GAUSSIAN_BLUR,
    IMAGE_FILTER_SOBEL_EDGES,
    IMAGE_FILTER_BRIGHTNESS,
    IMAGE_FILTER_NEGATIVE,
    IMAGE_FILTER_MEDIAN,
    IMAGE_FILTER_LAPLACIAN,
    IMAGE_FILTER_SHARPEN,
    IMAGE_FILTER_MOTION_BLUR,
    IMAGE_FILTER_BILATERAL,
    IMAGE_FILTER_EMBOSS,
    IMAGE_FILTER_CANNY,
    IMAGE_FILTER_CONTRAST,
    IMAGE_FILTER_COUNT
} ImageFilterType;

void        image_processor_apply(ImageFilterType filter, uint32_t* pixels, uint32_t width, uint32_t height);
void        image_processor_compute_luminance_histogram(const uint32_t* pixels, uint32_t pixel_count, uint32_t out_histogram_bins[256]);
const char* image_processor_filter_file_suffix(ImageFilterType filter);
bool        image_processor_filter_produces_saved_image(ImageFilterType filter);

#endif