#include "limine_requests.h"

#include <limine.h>
#include <stddef.h>

#include "drivers/serial/serial.h"
#include "kernel.h"

/* Base revision 4: restrictive HHDM (only usable/bootloader-reclaimable/
 * executable-and-modules/framebuffer/ACPI/reserved-mapped regions get
 * mapped, instead of a blind identity map of the first 4GiB), and --
 * critically for kernel/acpi -- the RSDP address is virtual (HHDM), and
 * revision 4 is the first revision that *guarantees* the RSDP, RSDT/XSDT,
 * and every table they point to are actually mapped into HHDM at all
 * (via ACPI-reclaimable, ACPI-NVS, or the new Reserved-Mapped memmap
 * type). Started this kernel on revision 3 during Phase 1/2, before
 * anything needed ACPI; revision 4 is a strict superset of what 3
 * guarantees, so bumping it here didn't require touching any earlier
 * code. See third_party/limine/PROTOCOL.md, "Base Revision Changes
 * Summary". */
__attribute__((used, section(".limine_requests"))) static volatile uint64_t
    limine_base_revision[] = LIMINE_BASE_REVISION(4);

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

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {
        .id = LIMINE_HHDM_REQUEST_ID,
        .revision = 0,
        .response = NULL,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request
    memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST_ID,
        .revision = 0,
        .response = NULL,
};

__attribute__((used,
                section(".limine_requests"))) static volatile struct
    limine_executable_address_request executable_address_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
        .revision = 0,
        .response = NULL,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_rsdp_request
    rsdp_request = {
        .id = LIMINE_RSDP_REQUEST_ID,
        .revision = 0,
        .response = NULL,
};

void limine_requests_check(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        serial_write("[PANIC] limine: bootloader does not support base revision 4\n");
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

uint64_t boot_get_hhdm_offset(void) {
    return hhdm_request.response->offset;
}

bool boot_get_memmap(struct boot_memmap *out) {
    struct limine_memmap_response *response = memmap_request.response;
    if (response == NULL) {
        return false;
    }

    /* struct limine_memmap_entry and struct boot_memmap_entry have
     * identical layout (base/length/type, same order, same widths) by
     * construction, but boot_memmap_entry is our own type so kernel/mm
     * never names a Limine struct. response->entries is an array of
     * pointers, not a contiguous array of structs, so this has to copy
     * rather than just cast the pointer.
     *
     * The copy lives in a static buffer (overwritten by the next call)
     * rather than being heap-allocated, since this runs before any
     * allocator exists and the kernel only ever needs one live memmap
     * snapshot at a time during early boot. */
    static struct boot_memmap_entry entries[256];
    uint64_t count = response->entry_count;
    if (count > ARRAY_LEN(entries)) {
        count = ARRAY_LEN(entries);
    }
    for (uint64_t i = 0; i < count; i++) {
        entries[i].base = response->entries[i]->base;
        entries[i].length = response->entries[i]->length;
        entries[i].type = response->entries[i]->type;
    }

    out->entries = entries;
    out->count = count;
    return true;
}

bool boot_get_kernel_address(struct boot_kernel_address *out) {
    struct limine_executable_address_response *response = executable_address_request.response;
    if (response == NULL) {
        return false;
    }
    out->physical_base = response->physical_base;
    out->virtual_base = response->virtual_base;
    return true;
}

bool boot_get_rsdp(void **out) {
    struct limine_rsdp_response *response = rsdp_request.response;
    if (response == NULL) {
        return false;
    }
    /* Already HHDM-mapped and directly dereferenceable at base revision
     * 4 (physical only at revision 3 -- see the base-revision comment
     * above), so unlike boot_get_kernel_address there's no separate
     * physical field to translate. */
    *out = response->address;
    return true;
}
