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
#include "arch/x86_64/cpu/syscall.h"
#include "arch/x86_64/mm/vmm.h"
#include "drivers/framebuffer/framebuffer.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "drivers/timer/timer.h"
#include "lib/log.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "sched/sched.h"
#include "sched/thread.h"

/* Used only if the MADT is missing or has no IOAPIC entry -- the
 * conventional base every common chipset, including QEMU's q35/i440fx,
 * uses. See docs/ARCHITECTURE.md, "LAPIC / IOAPIC". */
#define DEFAULT_IOAPIC_PHYS_BASE 0xfec00000ULL

#define DEMO_THREAD_ITERATIONS 3

/* Demonstrates the scheduler skeleton: three threads round-robin
 * through DEMO_THREAD_ITERATIONS voluntary yields each, then settle
 * into an interrupt-driven idle loop (still schedulable, just with
 * nothing left to do -- there's no thread-exit path yet, see
 * thread_exited in kernel/sched/thread.c). */
static void demo_thread(void *arg) {
    const char *name = arg;
    for (int i = 0; i < DEMO_THREAD_ITERATIONS; i++) {
        log_info("sched: thread %s, iteration %d", name, i);
        sched_yield();
    }
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* This kernel has no per-process address spaces yet (kernel/arch/x86_64/mm/vmm.c
 * extends one shared set of page tables), so a "user" mapping is just as
 * directly addressable from kernel code as any other pointer -- no HHDM
 * translation needed to write the demo program into it below. Fixed,
 * arbitrarily-chosen low addresses, picked only to stay well clear of
 * the kernel image and every other reserved range this template uses. */
#define USER_CODE_VADDR 0x400000ULL
#define USER_STACK_VADDR 0x500000ULL

/* Demonstrates the ring-3 jump and a full SYSCALL round trip: maps one
 * user-accessible code page and one stack page, writes a 4-byte program
 * (`syscall; jmp $`), and jumps to it. The syscall_dispatch log line
 * (kernel/arch/x86_64/cpu/syscall.c) is the proof the round trip
 * actually happened; the jmp-to-self afterward is a deliberate dead end
 * -- there's no thread-exit path yet (kernel/sched/thread.c), same as
 * the round-robin demo threads above. */
static void ring3_demo_thread(void *arg) {
    (void)arg;

    uint64_t code_phys = pmm_alloc_frame();
    uint64_t stack_phys = pmm_alloc_frame();
    if (code_phys == 0 || stack_phys == 0 ||
        !vmm_map(USER_CODE_VADDR, code_phys, VMM_USER | VMM_WRITABLE) ||
        !vmm_map(USER_STACK_VADDR, stack_phys, VMM_USER | VMM_WRITABLE | VMM_NO_EXECUTE)) {
        log_warn("ring3 demo: failed to set up the demo mapping, skipping");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    uint8_t *code = (uint8_t *)(uintptr_t)USER_CODE_VADDR;
    code[0] = 0x0f;
    code[1] = 0x05; /* syscall */
    code[2] = 0xeb;
    code[3] = 0xfe; /* jmp $   */

    log_info("ring3 demo: jumping to ring 3 at 0x%lx", USER_CODE_VADDR);
    jump_to_ring3(USER_CODE_VADDR, USER_STACK_VADDR + PAGE_SIZE);
}

static const char *const banner = " /$$$$$$            /$$\n"
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

    /* Before timer_init: the timer ISR calls sched_tick, which
     * dereferences the scheduler's `current` thread. */
    sched_init();
    timer_init();

    keyboard_init();
    ioapic_set_irq(KEYBOARD_IRQ, KEYBOARD_VECTOR, (uint8_t)lapic_get_id(), false);

    syscall_init();

    io_enable_interrupts();

    log_info("Quin Kernel Template -- Inbora Studio");
    log_info("booted via Limine (UEFI)");

    thread_create(demo_thread, "A");
    thread_create(demo_thread, "B");
    thread_create(demo_thread, "C");
    thread_create(ring3_demo_thread, NULL);
    for (int i = 0; i < DEMO_THREAD_ITERATIONS; i++) {
        sched_yield();
    }

    /* No-op unless QEMU was launched with isa-debug-exit attached (see
     * qemu_exit.h); this is what lets the CI smoke test and
     * `scripts/run.sh test` confirm a successful boot without a screen. */
    qemu_debug_exit(QEMU_EXIT_SUCCESS);

    kernel_halt();
}
