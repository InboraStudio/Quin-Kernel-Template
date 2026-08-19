#include "panic.h"

#include "drivers/framebuffer/framebuffer.h"
#include "drivers/serial/serial.h"
#include "lib/printf.h"

/* Intel SDM Vol. 3A, Table 6-1 "Protected-Mode Exceptions and Interrupts". */
static const char *const exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug",
    "NMI",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR BOUND Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Reserved (Coprocessor Segment Overrun)",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 FPU Floating-Point Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV Hypervisor Injection Exception",
    "#VC VMM Communication Exception",
    "#SX Security Exception",
    "Reserved",
};

static void panic_emit(void *ctx, char c) {
    (void)ctx;
    serial_putc(c);
    if (fb_is_available()) {
        fb_console_putc(c, FB_RED, FB_BLACK);
    }
}

static void panic_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(panic_emit, NULL, fmt, ap);
    va_end(ap);
}

/* Walks the RBP chain: at each frame, [rbp+0] holds the caller's saved
 * RBP and [rbp+8] holds the return address (SysV AMD64 prologue). Only
 * walkable because the Makefile passes -fno-omit-frame-pointer --
 * without it this loop would just read garbage. No symbol resolution
 * (no kallsyms-equivalent exists yet), so this prints raw addresses;
 * cross-reference against build/quin-kernel.elf with addr2line/gdb. */
static void print_stack_trace(uint64_t rbp) {
    panic_printf("Stack trace:\n");
    for (int depth = 0; depth < 16 && rbp != 0; depth++) {
        const uint64_t *frame = (const uint64_t *)(uintptr_t)rbp;
        uint64_t saved_rbp = frame[0];
        uint64_t return_addr = frame[1];
        if (return_addr == 0) {
            break;
        }
        panic_printf("  #%d 0x%016lx\n", depth, return_addr);
        if (saved_rbp <= rbp) {
            break;
        }
        rbp = saved_rbp;
    }
}

static NORETURN void panic_halt(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void panic_exception(struct interrupt_frame *frame) {
    const char *name = frame->vector < 32 ? exception_names[frame->vector] : "Unknown";
    panic_printf("[PANIC] %s (vector %lu, error 0x%lx)\n", name, frame->vector, frame->error_code);

    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        panic_printf("CR2=0x%016lx (faulting address)\n", cr2);
    }

    panic_printf("RIP=0x%016lx CS=0x%04lx RFLAGS=0x%016lx\n", frame->rip, frame->cs, frame->rflags);
    panic_printf("RAX=0x%016lx RBX=0x%016lx RCX=0x%016lx\n", frame->rax, frame->rbx, frame->rcx);
    panic_printf("RDX=0x%016lx RSI=0x%016lx RDI=0x%016lx\n", frame->rdx, frame->rsi, frame->rdi);
    panic_printf("RBP=0x%016lx R8= 0x%016lx R9= 0x%016lx\n", frame->rbp, frame->r8, frame->r9);
    panic_printf("R10=0x%016lx R11=0x%016lx R12=0x%016lx\n", frame->r10, frame->r11, frame->r12);
    panic_printf("R13=0x%016lx R14=0x%016lx R15=0x%016lx\n", frame->r13, frame->r14, frame->r15);

    print_stack_trace(frame->rbp);
    panic_halt();
}

void panic(const char *fmt, ...) {
    panic_printf("[PANIC] ");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(panic_emit, NULL, fmt, ap);
    va_end(ap);

    panic_printf("\n");
    print_stack_trace((uint64_t)(uintptr_t)__builtin_frame_address(0));
    panic_halt();
}
