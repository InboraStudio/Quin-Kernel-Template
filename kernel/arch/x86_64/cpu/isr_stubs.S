/* Every IDT entry points at one of these tiny stubs rather than directly
 * at C code: the CPU can't call a C function on interrupt (calling
 * convention, register-clobber, and error-code-or-not all differ per
 * vector), so each stub normalizes the stack into a single layout before
 * handing off to isr_dispatch in isr.c.
 *
 * Only 10 of the 32 exception vectors push a hardware error code (Intel
 * SDM Vol. 3A, Table 6-1): 8, 10, 11, 12, 13, 14, 17, 21, 29, 30. The
 * rest push a dummy 0 themselves so isr_common_stub sees the same shape
 * either way.
 *
 * The kernel never runs in ring 3 yet (that's Phase 5) and no IDT entry
 * uses the IST mechanism, so every interrupt here is a same-privilege,
 * no-stack-switch entry: the CPU pushes exactly RIP/CS/RFLAGS, never
 * RSP/SS. That's what makes a single fixed-size struct interrupt_frame
 * (isr.h) and a single iretq path correct -- see docs/ROADMAP.md for why
 * double-fault doesn't get its own IST stack yet.
 */

.macro isr_no_err vec
.global isr_stub_\vec
isr_stub_\vec:
    push $0
    push $\vec
    jmp isr_common_stub
.endm

.macro isr_err vec
.global isr_stub_\vec
isr_stub_\vec:
    push $\vec
    jmp isr_common_stub
.endm

.macro irq vec
.global irq_stub_\vec
irq_stub_\vec:
    push $0
    push $(32 + \vec)
    jmp isr_common_stub
.endm

.text

isr_no_err 0   /* #DE  Divide Error */
isr_no_err 1   /* #DB  Debug */
isr_no_err 2   /*      NMI */
isr_no_err 3   /* #BP  Breakpoint */
isr_no_err 4   /* #OF  Overflow */
isr_no_err 5   /* #BR  BOUND Range Exceeded */
isr_no_err 6   /* #UD  Invalid Opcode */
isr_no_err 7   /* #NM  Device Not Available */
isr_err    8   /* #DF  Double Fault */
isr_no_err 9   /*      Coprocessor Segment Overrun (reserved) */
isr_err    10  /* #TS  Invalid TSS */
isr_err    11  /* #NP  Segment Not Present */
isr_err    12  /* #SS  Stack-Segment Fault */
isr_err    13  /* #GP  General Protection Fault */
isr_err    14  /* #PF  Page Fault */
isr_no_err 15  /*      Reserved */
isr_no_err 16  /* #MF  x87 FPU Floating-Point Error */
isr_err    17  /* #AC  Alignment Check */
isr_no_err 18  /* #MC  Machine Check */
isr_no_err 19  /* #XM  SIMD Floating-Point Exception */
isr_no_err 20  /* #VE  Virtualization Exception */
isr_err    21  /* #CP  Control Protection Exception */
isr_no_err 22  /*      Reserved */
isr_no_err 23  /*      Reserved */
isr_no_err 24  /*      Reserved */
isr_no_err 25  /*      Reserved */
isr_no_err 26  /*      Reserved */
isr_no_err 27  /*      Reserved */
isr_no_err 28  /* #HV  Hypervisor Injection Exception */
isr_err    29  /* #VC  VMM Communication Exception */
isr_err    30  /* #SX  Security Exception */
isr_no_err 31  /*      Reserved */

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15

/* LAPIC spurious-interrupt vector (SDM Vol. 3A, 11.9): can fire with no
 * real interrupt behind it, and must specifically NOT be EOI'd, so it
 * gets its own trivial stub instead of going through isr_common_stub. */
.global isr_stub_spurious
isr_stub_spurious:
    iretq

.extern isr_dispatch

isr_common_stub:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi
    call isr_dispatch

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp
    iretq
