# Host-side unit tests: built and run with the host's own native
# compiler, not the freestanding cross-target the top-level Makefile
# uses -- these need a working libc (stdio, in particular) that the
# kernel itself doesn't have. Only architecture-independent logic with
# zero freestanding-specific dependencies belongs under test here; see
# kernel/lib/bitmap.c for why it qualifies and kernel/mm/pmm.c (which
# uses it, but itself depends on boot-time data no host test can
# provide) for what doesn't.

CC := cc
CFLAGS := -std=c17 -Wall -Wextra -Werror -Wshadow -Wconversion -g -O1
CPPFLAGS := -I. -I../../kernel -I../../kernel/include

KERNEL_DIR := ../../kernel
BUILD_DIR := build

TESTS := $(BUILD_DIR)/test_bitmap

.PHONY: all
all: $(TESTS)
	@for t in $(TESTS); do \
		echo "--- $$t ---"; \
		./$$t || exit 1; \
	done

$(BUILD_DIR)/test_bitmap: test_bitmap.c test.h $(KERNEL_DIR)/lib/bitmap.c $(KERNEL_DIR)/lib/bitmap.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) test_bitmap.c $(KERNEL_DIR)/lib/bitmap.c -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
