#ifndef ARCH_X86_64_BOOT_LIMINE_REQUESTS_H
#define ARCH_X86_64_BOOT_LIMINE_REQUESTS_H

#include <stdbool.h>
#include <stdint.h>

#include "boot_info.h"

/* This is the only header in the kernel that callers need in order to get
 * boot-time information; nothing outside kernel/arch/x86_64/boot/ includes
 * <limine.h> directly. That keeps the Limine protocol's data shapes from
 * leaking into arch-independent code and drivers, which only see the plain
 * structs in boot_info.h. */

/* Verifies the bootloader granted the base revision we requested. Must be
 * called before anything else touches a Limine response. Halts on serial
 * if it fails, since the panic subsystem isn't available this early. */
void limine_requests_check(void);

/* Returns false if no framebuffer was made available. */
bool boot_get_framebuffer(struct boot_framebuffer *out);

/* Offset added to a physical address to reach its Higher Half Direct Map
 * alias. Always succeeds -- the bootloader is required to answer this
 * request. */
uint64_t boot_get_hhdm_offset(void);

/* Returns false if the bootloader didn't answer (it always does for the
 * revision this kernel requests, but every boot_get_* here fails
 * explicitly rather than assuming). */
bool boot_get_memmap(struct boot_memmap *out);
bool boot_get_kernel_address(struct boot_kernel_address *out);

#endif
