#include "idt.h"

#include "gdt.h"
#include "kernel.h"
#include "lib/string.h"

/* SDM Vol. 3A, 6.14.1 "64-Bit Mode IDT". Interrupt-gate (type 0xE) rather
 * than trap-gate: the CPU clears IF on entry, so one interrupt handler
 * can't be interrupted by another before it finishes. Nothing in this
 * kernel re-enables interrupts inside a handler (no nested-interrupt
 * support yet), so that's exactly the semantics wanted. */
struct PACKED idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct PACKED idtr {
    uint16_t limit;
    uint64_t base;
};

static struct idt_entry idt[256] ALIGNED(16);

#define ISR(n) extern void isr_stub_##n(void)
ISR(0);
ISR(1);
ISR(2);
ISR(3);
ISR(4);
ISR(5);
ISR(6);
ISR(7);
ISR(8);
ISR(9);
ISR(10);
ISR(11);
ISR(12);
ISR(13);
ISR(14);
ISR(15);
ISR(16);
ISR(17);
ISR(18);
ISR(19);
ISR(20);
ISR(21);
ISR(22);
ISR(23);
ISR(24);
ISR(25);
ISR(26);
ISR(27);
ISR(28);
ISR(29);
ISR(30);
ISR(31);
#undef ISR

#define IRQ(n) extern void irq_stub_##n(void)
IRQ(0);
IRQ(1);
IRQ(2);
IRQ(3);
IRQ(4);
IRQ(5);
IRQ(6);
IRQ(7);
IRQ(8);
IRQ(9);
IRQ(10);
IRQ(11);
IRQ(12);
IRQ(13);
IRQ(14);
IRQ(15);
#undef IRQ

extern void isr_stub_spurious(void);

static void (*const isr_stubs[32])(void) = {
    isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,  isr_stub_4,  isr_stub_5,  isr_stub_6,
    isr_stub_7,  isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11, isr_stub_12, isr_stub_13,
    isr_stub_14, isr_stub_15, isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19, isr_stub_20,
    isr_stub_21, isr_stub_22, isr_stub_23, isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
    isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31,
};

static void (*const irq_stubs[16])(void) = {
    irq_stub_0, irq_stub_1, irq_stub_2,  irq_stub_3,  irq_stub_4,  irq_stub_5,
    irq_stub_6, irq_stub_7, irq_stub_8,  irq_stub_9,  irq_stub_10, irq_stub_11,
    irq_stub_12, irq_stub_13, irq_stub_14, irq_stub_15,
};

static void set_gate(uint8_t vector, void (*handler)(void)) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)(addr & 0xffff);
    idt[vector].selector = GDT_KERNEL_CODE;
    idt[vector].ist = 0;
    idt[vector].type_attr = 0x8e; /* present, DPL0, 64-bit interrupt gate */
    idt[vector].offset_mid = (uint16_t)((addr >> 16) & 0xffff);
    idt[vector].offset_high = (uint32_t)(addr >> 32);
    idt[vector].reserved = 0;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));

    for (unsigned i = 0; i < ARRAY_LEN(isr_stubs); i++) {
        set_gate((uint8_t)i, isr_stubs[i]);
    }
    for (unsigned i = 0; i < ARRAY_LEN(irq_stubs); i++) {
        set_gate((uint8_t)(32 + i), irq_stubs[i]);
    }
    set_gate(0xff, isr_stub_spurious); /* must match LAPIC_SPURIOUS_VECTOR in lapic.c */

    struct idtr idtr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)(uintptr_t)&idt,
    };
    __asm__ volatile("lidt %0" : : "m"(idtr));
}
