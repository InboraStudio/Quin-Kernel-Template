#include "gdt.h"

#include "kernel.h"
#include "lib/string.h"

/* OSDev Wiki: https://wiki.osdev.org/Global_Descriptor_Table and
 * https://wiki.osdev.org/Task_State_Segment. Base/limit are meaningless
 * for 64-bit code/data segments (the CPU always treats base as 0 and
 * ignores limit checking once in long mode) but are still filled in with
 * the conventional full-range values, since some virtualizers and the
 * SYSRET/SYSCALL fast paths are pickier about a segment "looking"
 * complete than the spec strictly requires. */
struct PACKED gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

/* SDM Vol. 3A, 8.2.3 "TSS Descriptor in 64-bit mode": twice the width of
 * a normal segment descriptor because the base address is 64-bit. */
struct PACKED tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
};

/* SDM Vol. 3A, Figure 8-11 "64-Bit TSS Format". Only rsp0 and the IST
 * slots are meaningful to us; the I/O permission bitmap is unused (no
 * ring-3 port I/O is granted), so iopb_offset just points past the end
 * of the structure, which disables it. */
struct PACKED tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
};

struct PACKED gdt_table {
    struct gdt_entry null;
    struct gdt_entry kernel_code;
    struct gdt_entry kernel_data;
    struct gdt_entry user_data;
    struct gdt_entry user_code;
    struct tss_descriptor tss;
};

struct PACKED gdtr {
    uint16_t limit;
    uint64_t base;
};

static struct gdt_table gdt ALIGNED(16);
static struct tss tss ALIGNED(16);

static void set_entry(struct gdt_entry *entry, uint8_t access, uint8_t flags) {
    entry->limit_low = 0xffff;
    entry->base_low = 0;
    entry->base_mid = 0;
    entry->access = access;
    entry->granularity = (uint8_t)((flags << 4) | 0x0f);
    entry->base_high = 0;
}

static void set_tss_descriptor(struct tss_descriptor *desc, uint64_t base, uint32_t limit) {
    desc->limit_low = (uint16_t)(limit & 0xffff);
    desc->base_low = (uint16_t)(base & 0xffff);
    desc->base_mid = (uint8_t)((base >> 16) & 0xff);
    desc->access = 0x89; /* P=1, DPL=0, type=1001 (64-bit TSS, available) */
    desc->granularity = (uint8_t)((limit >> 16) & 0x0f);
    desc->base_high = (uint8_t)((base >> 24) & 0xff);
    desc->base_upper = (uint32_t)(base >> 32);
    desc->reserved = 0;
}

extern void gdt_flush(uint64_t gdtr_addr, uint16_t code_selector, uint16_t data_selector);

void gdt_init(void) {
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    /* access byte: P | DPL | S | Exec | DC | RW | Accessed.
     * flags nibble: G | DB | L | AVL. */
    set_entry(&gdt.kernel_code, 0x9a, 0xa); /* DPL0 code: G=1,L=1 */
    set_entry(&gdt.kernel_data, 0x92, 0xc); /* DPL0 data: G=1,DB=1 */
    set_entry(&gdt.user_data, 0xf2, 0xc);   /* DPL3 data */
    set_entry(&gdt.user_code, 0xfa, 0xa);   /* DPL3 code: G=1,L=1 */

    tss.iopb_offset = sizeof(tss);
    set_tss_descriptor(&gdt.tss, (uint64_t)(uintptr_t)&tss, sizeof(tss) - 1);

    struct gdtr gdtr = {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)(uintptr_t)&gdt,
    };
    gdt_flush((uint64_t)(uintptr_t)&gdtr, GDT_KERNEL_CODE, GDT_KERNEL_DATA);

    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));
}

void gdt_set_kernel_stack(void *rsp0) {
    tss.rsp0 = (uint64_t)(uintptr_t)rsp0;
}
