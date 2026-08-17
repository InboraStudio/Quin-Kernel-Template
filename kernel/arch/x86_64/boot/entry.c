#include "kernel.h"

#include "limine_requests.h"

/* The linker script's ENTRY(kernel_entry) makes this the ELF entry point.
 * Limine hands off in long mode with paging, its own GDT, and a valid
 * stack already set up (see third_party/limine/PROTOCOL.md, "Machine
 * State at Entry" / x86-64) -- there is no need for a hand-written
 * assembly stub before we can run C. */
void kernel_entry(void) {
    limine_requests_check();
    kmain();
    kernel_halt();
}
