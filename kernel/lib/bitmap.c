#include "bitmap.h"

void bitmap_set(uint8_t *bitmap, uint64_t bit) {
    bitmap[bit / 8] |= (uint8_t)(1U << (bit % 8));
}

void bitmap_clear(uint8_t *bitmap, uint64_t bit) {
    bitmap[bit / 8] &= (uint8_t)~(1U << (bit % 8));
}

bool bitmap_test(const uint8_t *bitmap, uint64_t bit) {
    return (bitmap[bit / 8] & (uint8_t)(1U << (bit % 8))) != 0;
}

uint64_t bitmap_find_first_clear(const uint8_t *bitmap, uint64_t bit_count, uint64_t start_hint) {
    if (bit_count == 0) {
        return 0;
    }
    uint64_t start = start_hint % bit_count;
    for (uint64_t offset = 0; offset < bit_count; offset++) {
        uint64_t bit = (start + offset) % bit_count;
        if (!bitmap_test(bitmap, bit)) {
            return bit;
        }
    }
    return bit_count;
}
