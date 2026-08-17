#include "kernel.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/boot/qemu_exit.h"
#include "drivers/framebuffer/framebuffer.h"
#include "drivers/serial/serial.h"

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
    serial_write(banner);
    serial_write("Quin Kernel Template -- Inbora Studio\n");
    serial_write("booted via Limine (UEFI)\n");

    struct boot_framebuffer fb_info;
    if (boot_get_framebuffer(&fb_info) && fb_init(&fb_info)) {
        fb_clear(FB_BLACK);
        fb_console_write(banner, FB_GREEN, FB_BLACK);
        fb_console_write("Quin Kernel Template -- Inbora Studio\n", FB_WHITE, FB_BLACK);
        fb_console_write("booted via Limine (UEFI)\n", FB_GRAY, FB_BLACK);
    } else {
        serial_write("framebuffer unavailable, serial only\n");
    }

    /* No-op unless QEMU was launched with isa-debug-exit attached (see
     * qemu_exit.h); this is what lets the CI smoke test and
     * `scripts/run.sh test` confirm a successful boot without a screen. */
    qemu_debug_exit(QEMU_EXIT_SUCCESS);

    kernel_halt();
}
