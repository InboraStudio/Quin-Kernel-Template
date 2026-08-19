#ifndef ARCH_X86_64_CPU_LAPIC_H
#define ARCH_X86_64_CPU_LAPIC_H

#include <stdint.h>

/* Requires early_map_init() to have already run (kernel/arch/x86_64/mm/early_map.h) --
 * this maps the LAPIC's MMIO page through it. */
void lapic_init(void);

void lapic_send_eoi(void);

uint32_t lapic_get_id(void);

#endif
