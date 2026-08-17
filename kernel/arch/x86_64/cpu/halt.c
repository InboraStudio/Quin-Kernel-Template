#include "kernel.h"

#include "io.h"

NORETURN void kernel_halt(void) {
    io_disable_interrupts();
    for (;;) {
        io_halt();
    }
}
