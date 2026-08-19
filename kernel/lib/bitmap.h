#ifndef LIB_BITMAP_H
#define LIB_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

/* A plain bit array over caller-owned storage -- allocates nothing
 * itself, so it works identically whether the backing memory came from
 * a physical frame (kernel/mm/pmm.c) or a host malloc (tests/unit/test_bitmap.c).
 * That's also why this lives in kernel/lib rather than kernel/mm: it has
 * no dependency on anything freestanding-specific, which is what makes
 * it host-compilable for the unit test in the first place. */

void bitmap_set(uint8_t *bitmap, uint64_t bit);
void bitmap_clear(uint8_t *bitmap, uint64_t bit);
bool bitmap_test(const uint8_t *bitmap, uint64_t bit);

/* Scans forward from `start_hint`, wrapping around at `bit_count` at
 * most once, for the first clear bit. Returns `bit_count` if every bit
 * is set. `start_hint` doesn't need to be less than `bit_count` -- it's
 * taken mod `bit_count` -- which is what lets pmm.c pass a monotonically
 * increasing search cursor without wrapping it itself. */
uint64_t bitmap_find_first_clear(const uint8_t *bitmap, uint64_t bit_count, uint64_t start_hint);

#endif
