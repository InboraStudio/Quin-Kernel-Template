#include "lib/bitmap.h"
#include "test.h"

static void test_set_clear_are_independent_bits(void) {
    uint8_t bitmap[4] = {0};

    CHECK(!bitmap_test(bitmap, 5));
    bitmap_set(bitmap, 5);
    CHECK(bitmap_test(bitmap, 5));
    CHECK(!bitmap_test(bitmap, 4));
    CHECK(!bitmap_test(bitmap, 6));

    bitmap_clear(bitmap, 5);
    CHECK(!bitmap_test(bitmap, 5));
}

static void test_set_spans_byte_boundaries_correctly(void) {
    uint8_t bitmap[4] = {0};

    bitmap_set(bitmap, 0);
    bitmap_set(bitmap, 7);
    bitmap_set(bitmap, 8);
    bitmap_set(bitmap, 31);

    CHECK(bitmap[0] == 0x81); /* bits 0 and 7 */
    CHECK(bitmap[1] == 0x01); /* bit 8 */
    CHECK(bitmap[2] == 0x00);
    CHECK(bitmap[3] == 0x80); /* bit 31 */
}

static void test_find_first_clear_returns_bit_count_when_full(void) {
    uint8_t bitmap[4] = {0xff, 0xff, 0xff, 0xff};
    CHECK(bitmap_find_first_clear(bitmap, 32, 0) == 32);
}

static void test_find_first_clear_finds_the_only_gap(void) {
    uint8_t bitmap[4] = {0xff, 0xff, 0xff, 0xff};
    bitmap_clear(bitmap, 20);
    CHECK(bitmap_find_first_clear(bitmap, 32, 0) == 20);
}

static void test_find_first_clear_starts_at_the_hint_not_bit_zero(void) {
    /* Everything is free; searching from hint 10 should return 10
     * itself, not scan back to bit 0 first. */
    uint8_t bitmap[4] = {0};
    CHECK(bitmap_find_first_clear(bitmap, 32, 10) == 10);
}

static void test_find_first_clear_wraps_around_once(void) {
    uint8_t bitmap[4] = {0};
    /* Only bit 3 is free; searching from hint 10 has to wrap past the
     * end back to the start to find it. */
    for (uint64_t bit = 0; bit < 32; bit++) {
        if (bit != 3) {
            bitmap_set(bitmap, bit);
        }
    }
    CHECK(bitmap_find_first_clear(bitmap, 32, 10) == 3);
}

static void test_find_first_clear_hint_beyond_bit_count_wraps_via_modulo(void) {
    uint8_t bitmap[4] = {0};
    /* start_hint isn't required to already be < bit_count. */
    CHECK(bitmap_find_first_clear(bitmap, 32, 32 + 5) == 5);
}

int main(void) {
    RUN(test_set_clear_are_independent_bits);
    RUN(test_set_spans_byte_boundaries_correctly);
    RUN(test_find_first_clear_returns_bit_count_when_full);
    RUN(test_find_first_clear_finds_the_only_gap);
    RUN(test_find_first_clear_starts_at_the_hint_not_bit_zero);
    RUN(test_find_first_clear_wraps_around_once);
    RUN(test_find_first_clear_hint_beyond_bit_count_wraps_via_modulo);
    return test_report();
}
