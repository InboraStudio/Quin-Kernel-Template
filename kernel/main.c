#include "kernel.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/boot/qemu_exit.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/idt.h"
#include "arch/x86_64/cpu/io.h"
#include "arch/x86_64/cpu/ioapic.h"
#include "arch/x86_64/cpu/lapic.h"
#include "arch/x86_64/mm/vmm.h"
#include "drivers/framebuffer/framebuffer.h"
#include "drivers/serial/serial.h"
#include "lib/printf.h"
#include "mm/heap.h"
#include "mm/pmm.h"

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

static void serial_emit(void *ctx, char c) {
    (void)ctx;
    serial_putc(c);
}

static void serial_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(serial_emit, NULL, fmt, ap);
    va_end(ap);
}

void kmain(void) {
    serial_init();

    struct boot_framebuffer fb_info;
    bool have_fb = boot_get_framebuffer(&fb_info) && fb_init(&fb_info);
    if (have_fb) {
        fb_clear(FB_BLACK);
    } else {
        serial_write("framebuffer unavailable, serial only\n");
    }

    gdt_init();
    idt_init();

    pmm_init();
    vmm_init();
    heap_init();

    lapic_init();
    ioapic_init();
    io_enable_interrupts();

    serial_write(banner);
    serial_write("Quin Kernel Template -- Inbora Studio\n");
    serial_write("booted via Limine (UEFI)\n");
    serial_printf("mm: %lu/%lu frames free (%lu MiB)\n", pmm_free_frame_count(),
                  pmm_total_frame_count(), (pmm_free_frame_count() * PAGE_SIZE) / (1024 * 1024));
    if (have_fb) {
        fb_console_write(banner, FB_GREEN, FB_BLACK);
        fb_console_write("Quin Kernel Template -- Inbora Studio\n", FB_WHITE, FB_BLACK);
        fb_console_write("booted via Limine (UEFI)\n", FB_GRAY, FB_BLACK);
    }

    /* No-op unless QEMU was launched with isa-debug-exit attached (see
     * qemu_exit.h); this is what lets the CI smoke test and
     * `scripts/run.sh test` confirm a successful boot without a screen. */
    qemu_debug_exit(QEMU_EXIT_SUCCESS);

    kernel_halt();
}
