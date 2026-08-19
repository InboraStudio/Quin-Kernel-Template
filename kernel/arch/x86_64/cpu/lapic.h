#ifndef ARCH_X86_64_CPU_LAPIC_H
#define ARCH_X86_64_CPU_LAPIC_H

#include <stdbool.h>
#include <stdint.h>

/* Requires vmm_init() (and, transitively, pmm_init()) to have already
 * run -- this maps the LAPIC's MMIO page through vmm_map_mmio. */
void lapic_init(void);

void lapic_send_eoi(void);

uint32_t lapic_get_id(void);

/* LAPIC timer register access, used only by lapic_timer.c -- kept here
 * rather than duplicated because they're LAPIC registers like any
 * other, and lapic.c is what owns the MMIO base pointer. */
void lapic_timer_set_lvt(uint8_t vector, bool periodic, bool masked);
void lapic_timer_set_divide_16(void);
void lapic_timer_set_initial_count(uint32_t count);
uint32_t lapic_timer_get_current_count(void);

#endif
