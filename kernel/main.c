#include "kernel.h"

#include "acpi/acpi.h"
#include "acpi/madt.h"
#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/boot/qemu_exit.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/idt.h"
#include "arch/x86_64/cpu/io.h"
#include "arch/x86_64/cpu/ioapic.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/mm/vmm.h"
#include "drivers/framebuffer/framebuffer.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "drivers/timer/timer.h"
#include "lib/log.h"
#include "mm/heap.h"
#include "mm/pmm.h"

/* Used only if the MADT is missing or has no IOAPIC entry -- the
 * conventional base every common chipset, including QEMU's q35/i440fx,
 * uses. See docs/ARCHITECTURE.md, "LAPIC / IOAPIC". */
#define DEFAULT_IOAPIC_PHYS_BASE 0xfec00000ULL

static const char *const banner =
    " /$$$$$$            /$$\n"
    " /$$__  $$          |__/\n"
    "| $$  \\ $$ /$$   /$$ /$$ /$$$$$$$\n"
    "| $$  | $$| $$  | $$| $$| $$__  $$\n"
    "| $$  | $$| $$  | $$| $$| $$  \\ $$\n"
    "| $$/$$ $$| $$  | $$| $$| $$  | $$\n"
    "|  $$$$$$/|  $$$$$$/| $$| $$  | $$\n"
    " \\____ $$$ \\______/ |__/|__/  |__/\n"
    "      \\__/\n";

void kmain(void) {
    serial_init();

    struct boot_framebuffer fb_info;
    bool have_fb = boot_get_framebuffer(&fb_info) && fb_init(&fb_info);
    if (have_fb) {
        fb_clear(FB_BLACK);
    }

    serial_write(banner);
    if (have_fb) {
        fb_console_write(banner, FB_GREEN, FB_BLACK);
    }
    if (!have_fb) {
        log_warn("no framebuffer reported by the bootloader, serial only");
    }

    gdt_init();
    idt_init();

    pmm_init();
    vmm_init();
    heap_init();
    log_info("mm: %lu/%lu frames free (%lu MiB)", pmm_free_frame_count(), pmm_total_frame_count(),
              (pmm_free_frame_count() * PAGE_SIZE) / (1024 * 1024));

    acpi_init();

    lapic_init();

    struct madt_info madt;
    uint64_t ioapic_phys_base = DEFAULT_IOAPIC_PHYS_BASE;
    if (madt_parse(&madt)) {
        log_info("acpi: madt found, %u enabled cpu(s)", madt.enabled_cpu_count);
        if (madt.have_ioapic) {
            ioapic_phys_base = madt.ioapic_address;
        }
    } else {
        log_warn("acpi: no MADT, falling back to default IOAPIC base");
    }
    ioapic_init(ioapic_phys_base);

    timer_init();

    keyboard_init();
    ioapic_set_irq(KEYBOARD_IRQ, KEYBOARD_VECTOR, (uint8_t)lapic_get_id(), false);

    io_enable_interrupts();

    log_info("Quin Kernel Template -- Inbora Studio");
    log_info("booted via Limine (UEFI)");

    /* No-op unless QEMU was launched with isa-debug-exit attached (see
     * qemu_exit.h); this is what lets the CI smoke test and
     * `scripts/run.sh test` confirm a successful boot without a screen. */
    qemu_debug_exit(QEMU_EXIT_SUCCESS);

    kernel_halt();
}
