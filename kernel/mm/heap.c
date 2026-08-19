#include "heap.h"

#include "arch/x86_64/cpu/panic.h"
#include "arch/x86_64/mm/vmm.h"
#include "kernel.h"

/* Fixed virtual range, distinct from vmm.c's own dynamic/MMIO regions --
 * see docs/ARCHITECTURE.md's memory layout table. Registered as a single
 * lazy region rather than mapped upfront: writing the first block header
 * below (heap_init) is itself what faults in the heap's first physical
 * page, through the exact page-fault path vmm_handle_page_fault
 * implements. */
#define HEAP_VBASE 0xffffffff90000000ULL
#define HEAP_SIZE (16ULL * 1024 * 1024)

#define MIN_SPLIT_SIZE 16

/* Doubly-linked, first-fit, splitting and coalescing in both directions.
 * A production allocator would use per-size-class free lists for O(1)
 * common-case allocation; this one is O(n) in the number of blocks,
 * which is the right trade for a template kernel's heap traffic and
 * vastly simpler to read. */
struct block_header {
    uint64_t size; /* size of the usable region after this header */
    bool free;
    struct block_header *prev;
    struct block_header *next;
};

static struct block_header *free_list;

void heap_init(void) {
    if (!vmm_register_lazy_region(HEAP_VBASE, HEAP_VBASE + HEAP_SIZE, VMM_WRITABLE)) {
        panic("heap: failed to register lazy region");
    }

    free_list = (struct block_header *)(uintptr_t)HEAP_VBASE;
    free_list->size = HEAP_SIZE - sizeof(struct block_header);
    free_list->free = true;
    free_list->prev = NULL;
    free_list->next = NULL;
}

void *kmalloc(uint64_t size) {
    if (size == 0) {
        return NULL;
    }
    size = align_up(size, MIN_SPLIT_SIZE);

    for (struct block_header *node = free_list; node != NULL; node = node->next) {
        if (!node->free || node->size < size) {
            continue;
        }

        uint64_t remaining = node->size - size;
        if (remaining >= sizeof(struct block_header) + MIN_SPLIT_SIZE) {
            struct block_header *new_node =
                (struct block_header *)((uint8_t *)node + sizeof(struct block_header) + size);
            new_node->size = remaining - sizeof(struct block_header);
            new_node->free = true;
            new_node->prev = node;
            new_node->next = node->next;
            if (new_node->next != NULL) {
                new_node->next->prev = new_node;
            }
            node->size = size;
            node->next = new_node;
        }

        node->free = false;
        return (void *)((uint8_t *)node + sizeof(struct block_header));
    }

    return NULL; /* heap exhausted -- HEAP_SIZE is a hard ceiling, not a lazy-growth range */
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    struct block_header *node = (struct block_header *)((uint8_t *)ptr - sizeof(struct block_header));
    node->free = true;

    if (node->next != NULL && node->next->free) {
        node->size += sizeof(struct block_header) + node->next->size;
        node->next = node->next->next;
        if (node->next != NULL) {
            node->next->prev = node;
        }
    }

    if (node->prev != NULL && node->prev->free) {
        node->prev->size += sizeof(struct block_header) + node->size;
        node->prev->next = node->next;
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
    }
}
