#include <memtest.h>
#include <liballoc.h>
#include <string.h>
#include <stdio.h>

#define MEMTEST_GUARD_MAGIC_HEAD   0xDEADBEEF
#define MEMTEST_GUARD_MAGIC_TAIL   0xCAFEBABE
#define MEMTEST_FILL_PATTERN       0xA5
#define MEMTEST_REALLOC_PATTERN    0x5A

#define MEMTEST_GUARD_BYTES        16
#define MEMTEST_MAX_SIMULTANEOUS   8

typedef struct {
    uint32_t  guard_head[4];
    uint8_t*  payload;
    uint32_t  payload_size;
    uint8_t   fill_pattern;
} MemtestAllocation;

static uint32_t memtest_tests_passed = 0;
static uint32_t memtest_tests_failed = 0;

static void memtest_report_pass(const char* test_name)
{
    memtest_tests_passed++;
    printf("[MEMTEST] PASS: %s\n", test_name);
}

static void memtest_report_fail(const char* test_name, const char* reason)
{
    memtest_tests_failed++;
    printf("[MEMTEST] FAIL: %s - %s\n", test_name, reason);
}

static uint8_t* memtest_allocate_guarded(uint32_t payload_size, uint8_t fill_pattern)
{
    uint32_t total_size = MEMTEST_GUARD_BYTES + payload_size + MEMTEST_GUARD_BYTES;
    uint8_t* raw_block  = (uint8_t*)malloc(total_size);
    if (!raw_block) return (uint8_t*)0;

    uint32_t* head_guard = (uint32_t*)raw_block;
    head_guard[0] = MEMTEST_GUARD_MAGIC_HEAD;
    head_guard[1] = payload_size;
    head_guard[2] = (uint32_t)(uint64_t)raw_block;
    head_guard[3] = fill_pattern;

    uint8_t* payload = raw_block + MEMTEST_GUARD_BYTES;
    memset(payload, fill_pattern, payload_size);

    uint32_t* tail_guard = (uint32_t*)(payload + payload_size);
    tail_guard[0] = MEMTEST_GUARD_MAGIC_TAIL;
    tail_guard[1] = payload_size;
    tail_guard[2] = (uint32_t)(uint64_t)raw_block;
    tail_guard[3] = ~fill_pattern;

    return raw_block;
}

static bool memtest_verify_guarded(uint8_t* raw_block, const char* test_name)
{
    uint32_t* head_guard  = (uint32_t*)raw_block;
    uint32_t  payload_size = head_guard[1];
    uint8_t   fill_pattern = (uint8_t)head_guard[3];
    uint8_t*  payload      = raw_block + MEMTEST_GUARD_BYTES;
    uint32_t* tail_guard   = (uint32_t*)(payload + payload_size);

    if (head_guard[0] != MEMTEST_GUARD_MAGIC_HEAD) {
        memtest_report_fail(test_name, "head guard overwritten");
        return false;
    }
    if (tail_guard[0] != MEMTEST_GUARD_MAGIC_TAIL) {
        memtest_report_fail(test_name, "tail guard overwritten - BUFFER OVERRUN");
        return false;
    }
    if (tail_guard[1] != payload_size) {
        memtest_report_fail(test_name, "tail size field corrupted");
        return false;
    }
    if (head_guard[2] != tail_guard[2]) {
        memtest_report_fail(test_name, "base pointer mismatch between guards");
        return false;
    }

    for (uint32_t i = 0; i < payload_size; i++) {
        if (payload[i] != fill_pattern) {
            printf("[MEMTEST] FAIL: %s - payload corrupted at byte %u (expected 0x%02X got 0x%02X)\n",
                   test_name, i, fill_pattern, payload[i]);
            memtest_tests_failed++;
            return false;
        }
    }

    return true;
}

static void memtest_free_guarded(uint8_t* raw_block)
{
    free(raw_block);
}

static void memtest_run_basic_small_allocations()
{
    printf("[MEMTEST] --- basic small allocations ---\n");

    static const uint32_t small_sizes[] = { 1, 7, 16, 32, 64, 128, 256, 512, 1024 };
    static const int small_size_count = 9;

    for (int i = 0; i < small_size_count; i++) {
        uint32_t size = small_sizes[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "small_alloc_%u_bytes", size);

        uint8_t* block = memtest_allocate_guarded(size, MEMTEST_FILL_PATTERN);
        if (!block) {
            memtest_report_fail(test_name, "malloc returned NULL");
            continue;
        }
        if (memtest_verify_guarded(block, test_name))
            memtest_report_pass(test_name);
        memtest_free_guarded(block);
    }
}

static void memtest_run_large_allocations()
{
    printf("[MEMTEST] --- large allocations (image-sized) ---\n");

    static const uint32_t large_sizes[] = {
        1024 * 1024,             // 1MB
        4 * 1024 * 1024,         // 4MB
        8 * 1024 * 1024,         // 8MB (1920x1080 ARGB)
        8 * 1024 * 1024 * 4,     // 32MB (RGBA byte buffer for lodepng)
    };
    static const int large_size_count = 4;

    for (int i = 0; i < large_size_count; i++) {
        uint32_t size = large_sizes[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "large_alloc_%u_bytes", size);

        uint8_t* block = memtest_allocate_guarded(size, (uint8_t)(0xA5 + i));
        if (!block) {
            memtest_report_fail(test_name, "malloc returned NULL - heap exhausted?");
            continue;
        }
        if (memtest_verify_guarded(block, test_name))
            memtest_report_pass(test_name);
        memtest_free_guarded(block);
    }
}

static void memtest_run_simultaneous_large_allocations()
{
    printf("[MEMTEST] --- simultaneous large allocations (filter pipeline pattern) ---\n");

    const uint32_t image_pixel_buffer_size = 1920 * 1080 * 4;
    const uint32_t rgba_byte_buffer_size   = 1920 * 1080 * 4;

    uint8_t* original_image_data  = memtest_allocate_guarded(image_pixel_buffer_size, 0x11);
    uint8_t* working_pixel_buffer = memtest_allocate_guarded(image_pixel_buffer_size, 0x22);
    uint8_t* rgba_byte_buffer     = memtest_allocate_guarded(rgba_byte_buffer_size,   0x33);

    if (!original_image_data) {
        memtest_report_fail("simultaneous_3x8mb", "original_image_data alloc failed");
        goto simultaneous_cleanup;
    }
    if (!working_pixel_buffer) {
        memtest_report_fail("simultaneous_3x8mb", "working_pixel_buffer alloc failed");
        goto simultaneous_cleanup;
    }
    if (!rgba_byte_buffer) {
        memtest_report_fail("simultaneous_3x8mb", "rgba_byte_buffer alloc failed");
        goto simultaneous_cleanup;
    }

    printf("[MEMTEST] 3x 8MB allocated simultaneously - verifying all three...\n");

    if (!memtest_verify_guarded(original_image_data,  "simultaneous_original"))  goto simultaneous_cleanup;
    if (!memtest_verify_guarded(working_pixel_buffer, "simultaneous_working"))   goto simultaneous_cleanup;
    if (!memtest_verify_guarded(rgba_byte_buffer,     "simultaneous_rgba"))      goto simultaneous_cleanup;

    memtest_report_pass("simultaneous_3x8mb_all_verified");

simultaneous_cleanup:
    if (rgba_byte_buffer)     memtest_free_guarded(rgba_byte_buffer);
    if (working_pixel_buffer) memtest_free_guarded(working_pixel_buffer);
    if (original_image_data)  memtest_free_guarded(original_image_data);
}

static void memtest_run_realloc_growth_chain()
{
    printf("[MEMTEST] --- realloc growth chain (lodepng output buffer pattern) ---\n");

    uint32_t initial_size = 4096;
    void*    growing_buffer = malloc(initial_size);

    if (!growing_buffer) {
        memtest_report_fail("realloc_growth_chain", "initial malloc failed");
        return;
    }

    memset(growing_buffer, MEMTEST_REALLOC_PATTERN, initial_size);

    static const uint32_t growth_sizes[] = {
        64  * 1024,
        256 * 1024,
        512 * 1024,
        1024 * 1024,
        2   * 1024 * 1024,
        4   * 1024 * 1024,
        8   * 1024 * 1024,
    };
    static const int growth_step_count = 7;

    uint32_t previous_size = initial_size;

    for (int step = 0; step < growth_step_count; step++) {
        uint32_t new_size = growth_sizes[step];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "realloc_growth_%u_to_%u", previous_size, new_size);

        void* reallocated_buffer = realloc(growing_buffer, new_size);
        if (!reallocated_buffer) {
            memtest_report_fail(test_name, "realloc returned NULL");
            free(growing_buffer);
            return;
        }

        uint8_t* byte_buffer = (uint8_t*)reallocated_buffer;
        bool preserved_ok = true;
        for (uint32_t i = 0; i < previous_size && i < new_size; i++) {
            if (byte_buffer[i] != MEMTEST_REALLOC_PATTERN) {
                printf("[MEMTEST] FAIL: %s - data not preserved at byte %u\n", test_name, i);
                memtest_tests_failed++;
                preserved_ok = false;
                break;
            }
        }

        if (preserved_ok) {
            memset(reallocated_buffer, MEMTEST_REALLOC_PATTERN, new_size);
            memtest_report_pass(test_name);
        }

        growing_buffer  = reallocated_buffer;
        previous_size   = new_size;
    }

    free(growing_buffer);
}

static void memtest_run_fragmentation_stress()
{
    printf("[MEMTEST] --- fragmentation stress (interleaved alloc/free) ---\n");

    uint8_t* allocation_slots[MEMTEST_MAX_SIMULTANEOUS] = {};
    static const uint32_t fragmentation_sizes[MEMTEST_MAX_SIMULTANEOUS] = {
        64, 1024, 256, 8192, 128, 4096, 512, 2048
    };

    for (int slot = 0; slot < MEMTEST_MAX_SIMULTANEOUS; slot++) {
        allocation_slots[slot] = memtest_allocate_guarded(fragmentation_sizes[slot], (uint8_t)(0x10 + slot));
        if (!allocation_slots[slot]) {
            printf("[MEMTEST] fragmentation slot %d alloc failed for size %u\n", slot, fragmentation_sizes[slot]);
        }
    }

    for (int slot = 0; slot < MEMTEST_MAX_SIMULTANEOUS; slot += 2) {
        if (allocation_slots[slot]) {
            memtest_free_guarded(allocation_slots[slot]);
            allocation_slots[slot] = (uint8_t*)0;
        }
    }

    for (int slot = 0; slot < MEMTEST_MAX_SIMULTANEOUS; slot += 2) {
        allocation_slots[slot] = memtest_allocate_guarded(fragmentation_sizes[slot] * 2, (uint8_t)(0x20 + slot));
    }

    bool all_ok = true;
    for (int slot = 0; slot < MEMTEST_MAX_SIMULTANEOUS; slot++) {
        if (!allocation_slots[slot]) continue;
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "fragmentation_slot_%d", slot);
        if (!memtest_verify_guarded(allocation_slots[slot], test_name))
            all_ok = false;
    }

    if (all_ok)
        memtest_report_pass("fragmentation_stress_all_slots");

    for (int slot = 0; slot < MEMTEST_MAX_SIMULTANEOUS; slot++) {
        if (allocation_slots[slot])
            memtest_free_guarded(allocation_slots[slot]);
    }
}

static void memtest_run_filter_pipeline_simulation()
{
    printf("[MEMTEST] --- filter pipeline simulation (3 iterations) ---\n");

    const uint32_t pixel_count       = 1920 * 1080;
    const uint32_t argb_buffer_size  = pixel_count * 4;

    for (int iteration = 0; iteration < 3; iteration++) {
        printf("[MEMTEST] filter pipeline iteration %d\n", iteration);

        char test_name[64];

        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_working", iteration);
        uint8_t* working_buffer = memtest_allocate_guarded(argb_buffer_size, (uint8_t)(0xAA + iteration));
        if (!working_buffer) { memtest_report_fail(test_name, "alloc failed"); continue; }

        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_rgba", iteration);
        uint8_t* rgba_buffer = memtest_allocate_guarded(argb_buffer_size, (uint8_t)(0xBB + iteration));
        if (!rgba_buffer) {
            memtest_report_fail(test_name, "alloc failed");
            memtest_free_guarded(working_buffer);
            continue;
        }

        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_encoded_png", iteration);
        uint32_t encoded_size   = 640 * 1024;
        uint8_t* encoded_buffer = memtest_allocate_guarded(encoded_size, (uint8_t)(0xCC + iteration));
        if (!encoded_buffer) {
            memtest_report_fail(test_name, "alloc failed");
            memtest_free_guarded(rgba_buffer);
            memtest_free_guarded(working_buffer);
            continue;
        }

        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_new_image", iteration);
        uint8_t* new_image_buffer = memtest_allocate_guarded(argb_buffer_size, (uint8_t)(0xDD + iteration));
        if (!new_image_buffer) {
            memtest_report_fail(test_name, "alloc failed");
            memtest_free_guarded(encoded_buffer);
            memtest_free_guarded(rgba_buffer);
            memtest_free_guarded(working_buffer);
            continue;
        }

        bool iter_ok = true;
        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_verify_working", iteration);
        if (!memtest_verify_guarded(working_buffer,    test_name))  iter_ok = false;
        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_verify_rgba", iteration);
        if (!memtest_verify_guarded(rgba_buffer,       test_name))  iter_ok = false;
        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_verify_encoded", iteration);
        if (!memtest_verify_guarded(encoded_buffer,    test_name))  iter_ok = false;
        snprintf(test_name, sizeof(test_name), "pipeline_iter%d_verify_new_image", iteration);
        if (!memtest_verify_guarded(new_image_buffer,  test_name))  iter_ok = false;

        if (iter_ok) {
            snprintf(test_name, sizeof(test_name), "pipeline_iter%d_all_verified", iteration);
            memtest_report_pass(test_name);
        }

        memtest_free_guarded(encoded_buffer);
        memtest_free_guarded(rgba_buffer);
        memtest_free_guarded(working_buffer);

        uint8_t* old_image_buffer = new_image_buffer;
        memtest_free_guarded(old_image_buffer);
    }
}

void cmd_memtest()
{
    memtest_tests_passed = 0;
    memtest_tests_failed = 0;

    printf("[MEMTEST] starting allocator stress test\n");

    memtest_run_basic_small_allocations();
    memtest_run_large_allocations();
    memtest_run_simultaneous_large_allocations();
    memtest_run_realloc_growth_chain();
    memtest_run_fragmentation_stress();
    memtest_run_filter_pipeline_simulation();

    printf("[MEMTEST] done. passed:%u failed:%u\n",
           memtest_tests_passed, memtest_tests_failed);

    if (memtest_tests_failed == 0)
        printf("[MEMTEST] allocator appears healthy\n");
    else
        printf("[MEMTEST] ALLOCATOR HAS BUGS - %u failures above\n", memtest_tests_failed);
}