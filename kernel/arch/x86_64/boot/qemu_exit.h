#ifndef ARCH_X86_64_BOOT_QEMU_EXIT_H
#define ARCH_X86_64_BOOT_QEMU_EXIT_H

#include <stdint.h>

/* QEMU exits with ((code << 1) | 1), so these show up as odd process exit
 * codes: 0x21 (33) on success, 0x23 (35) on failure. scripts/run.sh test
 * and .github/workflows/build.yml check for 33. */
#define QEMU_EXIT_SUCCESS 0x10
#define QEMU_EXIT_FAILURE 0x11

/* Writes to QEMU's isa-debug-exit device (port 0xf4), which -- only if
 * QEMU was launched with `-device isa-debug-exit,iobase=0xf4,iosize=0x04`
 * -- immediately terminates the emulator with the exit code derived from
 * `code`. That flag is only present in scripts/run.sh's `test` mode and
 * in CI; a normal interactive boot doesn't attach the device, so this
 * write lands on an unmapped I/O port, is silently discarded by the
 * platform, and execution continues right past it. Safe to call
 * unconditionally for that reason. */
void qemu_debug_exit(uint8_t code);

#endif
