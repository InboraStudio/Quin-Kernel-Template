#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <stdint.h>

/* Requires vmm_init() (and pmm_init()) to have already run: the heap's
 * virtual range is registered as a lazy region (kernel/arch/x86_64/mm/vmm.h)
 * rather than eagerly backed, so physical frames are only spent on pages
 * the heap actually touches. */
void heap_init(void);

void *kmalloc(uint64_t size);
void kfree(void *ptr);

#endif
