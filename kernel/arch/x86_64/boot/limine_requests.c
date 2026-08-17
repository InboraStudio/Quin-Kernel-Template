#include "limine_requests.h"

#include <limine.h>
#include <stddef.h>

#include "drivers/serial/serial.h"

/* Base revision 3: restrictive HHDM (only usable/bootloader-reclaimable/
 * executable-and-modules/framebuffer regions get mapped, instead of a blind
 * identity map of the first 4GiB), and the RSDP is handed back as a
 * physical address. See third_party/limine/PROTOCOL.md, "Base Revision
 * Changes Summary". Revisions 4+ add guarantees (LAPIC/IOAPIC masked state,
 * stricter cr0/cr4/EFER) we don't currently depend on; revision 3 is the
 * lowest revision with the HHDM behavior kernel/mm's design assumes. */
__attribute__((used, section(".limine_requests"))) static volatile uint64_t
    limine_base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used,
                section(".limine_requests_start_marker"))) static volatile uint64_t
    limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used,
                section(".limine_requests_end_marker"))) static volatile uint64_t
    limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests"))) static volatile struct
    limine_framebuffer_request framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = NULL,
};

void limine_requests_check(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        serial_write("[PANIC] limine: bootloader does not support base revision 3\n");
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }
}

bool boot_get_framebuffer(struct boot_framebuffer *out) {
    struct limine_framebuffer_response *response = framebuffer_request.response;
    if (response == NULL || response->framebuffer_count == 0) {
        return false;
    }

    struct limine_framebuffer *fb = response->framebuffers[0];
    out->address = fb->address;
    out->width = fb->width;
    out->height = fb->height;
    out->pitch = fb->pitch;
    out->bpp = fb->bpp;
    out->red_mask_size = fb->red_mask_size;
    out->red_mask_shift = fb->red_mask_shift;
    out->green_mask_size = fb->green_mask_size;
    out->green_mask_shift = fb->green_mask_shift;
    out->blue_mask_size = fb->blue_mask_size;
    out->blue_mask_shift = fb->blue_mask_shift;
    return true;
}
