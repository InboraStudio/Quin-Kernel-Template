#ifndef ARCH_X86_64_CPU_LAPIC_H
#define ARCH_X86_64_CPU_LAPIC_H

#include <stdint.h>

/* Requires vmm_init() (and, transitively, pmm_init()) to have already
 * run -- this maps the LAPIC's MMIO page through vmm_map_mmio. */
void lapic_init(void);

void lapic_send_eoi(void);

uint32_t lapic_get_id(void);

#endif
