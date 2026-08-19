#include "lapic.h"

#include "arch/x86_64/mm/early_map.h"
#include "io.h"
#include "kernel.h"

/* SDM Vol. 3A, 11.4.1 "The Local APIC Block Diagram" and 11.4.4 "Local
 * APIC Status and Location". The base address in the MSR is almost
 * always 0xfee00000 in practice (QEMU never changes it), but reading it
 * rather than hardcoding costs nothing and is correct if it's ever not. */
#define IA32_APIC_BASE_MSR 0x1b
#define APIC_BASE_ENABLE (1ULL << 11)
#define APIC_BASE_ADDR_MASK 0xfffff000ULL

#define LAPIC_REG_ID 0x20
#define LAPIC_REG_EOI 0xb0
#define LAPIC_REG_SVR 0xf0
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_LVT_LINT0 0x350
#define LAPIC_REG_LVT_LINT1 0x360
#define LAPIC_REG_LVT_ERROR 0x370

#define LAPIC_SVR_SOFTWARE_ENABLE (1U << 8)
/* Bits 0-3 of the spurious vector are hardwired to 1 on some CPUs, so by
 * convention the spurious vector is chosen from {0x?F}; 0xff also keeps
 * it safely above every real IRQ vector this kernel hands out (32-47).
 * isr_stub_spurious (isr_stubs.S) is wired to exactly this vector in
 * idt.c and must never call lapic_send_eoi -- SDM Vol. 3A, 11.9. */
#define LAPIC_SPURIOUS_VECTOR 0xff
#define LAPIC_LVT_MASKED (1U << 16)

static volatile uint32_t *lapic_base;

static uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

static void lapic_write(uint32_t reg, uint32_t value) {
    lapic_base[reg / 4] = value;
}

void lapic_init(void) {
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    uint64_t phys_base = apic_base_msr & APIC_BASE_ADDR_MASK;
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr | APIC_BASE_ENABLE);

    lapic_base = (volatile uint32_t *)early_map_mmio(phys_base);

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_ERROR, LAPIC_LVT_MASKED);

    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_SOFTWARE_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

void lapic_send_eoi(void) {
    lapic_write(LAPIC_REG_EOI, 0);
}

uint32_t lapic_get_id(void) {
    return lapic_read(LAPIC_REG_ID) >> 24;
}
