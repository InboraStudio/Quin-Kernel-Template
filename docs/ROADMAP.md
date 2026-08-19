# Roadmap

This tracks what actually works, what's scaffolded but incomplete, and what's
explicitly out of scope. If something isn't checked off here, don't assume it
works just because the file exists — read the "scaffolded" and "out of scope"
sections before relying on anything.

Status is tracked per phase. A phase is only checked off once it builds with
zero warnings, passes `clang-format --check`, and boots cleanly in the QEMU
smoke test.

## Implemented

- [x] **Phase 0 — Toolchain & boot skeleton**: Limine boots the kernel via
      UEFI, higher-half linking, banner printed over serial + framebuffer.
      `scripts/build.sh`/`scripts/run.sh` work on a clean checkout;
      `scripts/run.sh test` verifies the boot banner and a clean
      `isa-debug-exit` exit headlessly.
- [x] **Phase 1 — CPU bring-up**: GDT/TSS, IDT with all 32 exception vectors,
      IOAPIC/LAPIC IRQ routing (no legacy 8259 PIC), panic screen with
      register dump + stack trace. Verified by deliberately triggering
      `#BP` (`int3`) and confirming the panic screen renders correctly on
      both serial and framebuffer before removing the trigger.
- [x] **Phase 2 — Memory management**: bitmap PMM, 4-level paging VMM with
      guard pages and lazy regions, kernel heap, page fault handler that
      distinguishes real faults from lazy-mapping opportunities. Verified
      by exercising `kmalloc`/`kfree` (including the lazy-fault path) and
      by deliberately writing past a guarded allocation and confirming
      the panic fires with `CR2` pointing at the guard page.
- [x] **Phase 3 — Platform services**: ACPI table parsing (RSDP → XSDT →
      MADT, with an RSDT fallback), LAPIC timer calibrated against the
      PIT, a leveled logger, and a PS/2 keyboard driver. Verified with
      QEMU's monitor `sendkey` (including a shifted key) echoed to
      serial before removing the temporary test loop.
- [x] **Phase 4 — Concurrency skeleton**: kernel threads with hand-rolled
      context switching, a preemptive round-robin scheduler, atomic
      spinlocks with interrupt-safe variants. Verified with two separate
      demos (both removed after confirming the behavior, leaving only a
      permanent 3-thread round-robin demo in `kmain`): voluntary
      round-robin across several `sched_yield` calls, and — specifically
      to catch true preemption — a thread that busy-loops for 500 ticks
      without ever yielding. The second demo caught a real deadlock (see
      `docs/ARCHITECTURE.md`, "Kernel threads and context switching")
      where EOI'd-after-the-handler timer dispatch permanently starved
      the LAPIC of further ticks once a thread was preempted for the
      first time; fixed by EOI'ing before the handler runs instead.
- [ ] **Phase 5 — Syscall groundwork**: `syscall`/`sysret` entry stub, ring-3
      jump path.
- [ ] **Phase 6 — Test harness & polish**: host-side unit tests, QEMU
      `isa-debug-exit` CI smoke test.

## Scaffolded but incomplete

- **Boot-time failure handling** (Phase 0): if the bootloader doesn't
  grant the requested Limine base revision, `limine_requests_check()`
  writes a message to serial and halts in an infinite `cli; hlt` loop.
  This is not the kernel's panic subsystem — there isn't one yet, since a
  real panic handler needs the IDT and register-dump machinery Phase 1
  adds. Once that lands, this early check still can't use it (the panic
  path itself may depend on subsystems not yet initialized this early),
  so expect it to stay a minimal serial-only fallback by design, not an
  oversight.
- **Syscall dispatch** (Phase 5): the `syscall` entry stub saves state and
  returns `-ENOSYS` for every call number. There is no syscall table, no
  argument marshaling convention, and no per-syscall permission model. This
  is intentional — Quin ends at "you have a safe ring-3 entry point," and
  your kernel's syscall ABI is your own design decision.
- **Double-fault safety net** (Phase 1): vector 8 (#DF) is a normal IDT
  gate like any other, with no IST (Interrupt Stack Table) entry, so a
  double fault caused by kernel stack overflow runs its handler on the
  same already-overflowed stack — which can fault again and triple-fault
  the machine instead of producing a clean panic screen. Fixing this
  needs an IST-backed stack for vector 8 *and* a second exception-frame
  shape in `isr_common_stub`/`isr.h` to account for the RSP/SS the CPU
  additionally pushes on an IST-induced stack switch (see
  `docs/ARCHITECTURE.md`, "IDT and interrupt dispatch") — deferred rather
  than risking getting that interaction subtly wrong.
- **Panic stack traces have no symbol resolution** (Phase 1): frame
  addresses are printed raw; there's no kernel-side symbol table to turn
  them into function names. Cross-reference against
  `build/quin-kernel.elf` with `addr2line` or gdb.
- **Bootloader-reclaimable memory is never reclaimed** (Phase 2): the
  PMM permanently excludes it, since Limine's own page tables live there
  and this kernel keeps extending those tables rather than building an
  independent address space (see `docs/ARCHITECTURE.md`, "Physical
  memory manager"). Reclaiming it safely needs a fork's own address
  space first. On a small guest this is a few hundred KiB to a few MiB
  left on the table, not a correctness problem.
- **`vmm_alloc_guarded` leaks frames on partial failure** (Phase 2): if
  the PMM runs out of frames partway through a multi-page guarded
  allocation, the pages already mapped in that call are not freed.
  `thread_create` (Phase 4) is the first real consumer, allocating a
  4-page stack per thread; `kmalloc`/`vmm_alloc_guarded` returning
  `NULL` there just fails the `thread_create` call rather than
  corrupting anything, but the leaked pages (if any were mapped before
  the failure) aren't recovered. Worth a rollback path before anything
  allocates enough pages per call for a partial failure to be likely.
- **The kernel heap has a fixed 16MiB ceiling** (Phase 2): `kmalloc`
  returns `NULL` once the reserved lazy region is exhausted; there's no
  growth path (extending the reserved virtual range, or falling back to
  a second region). Fine for a template kernel's own allocation volume,
  a real limitation for anything heap-heavy.
- **No FADT/DSDT/AML parsing** (Phase 3): `kernel/acpi` reads fixed-layout
  tables (RSDP, RSDT/XSDT, MADT) only. The FADT and AML (ACPI Machine
  Language, inside the DSDT/SSDT) are a fundamentally different and much
  larger undertaking — a bytecode interpreter, not a struct-layout parser
  — and nothing this template does needs them (no power management, no
  device enumeration beyond the MADT's fixed entries).
- **PS/2 keyboard driver has no 8042 controller initialization** (Phase 3):
  `keyboard_init` assumes the controller and its first port are already
  enabled, which every checked QEMU machine type's firmware does by
  default. A driver aiming at real, potentially-uninitialized hardware
  needs the actual init sequence (self-test, port enable, ACK'd `0xff`
  reset) first.
- **PS/2 keyboard scancode coverage is partial** (Phase 3): the
  translation table (`kernel/drivers/keyboard/keyboard.c`) covers the
  alphanumeric block, punctuation, space, enter, tab, and backspace.
  F-keys, arrows, the numpad, and the `0xE0`-prefixed extended scancodes
  (right Ctrl/Alt, the cursor cluster, ...) are silently dropped, and
  Caps Lock isn't tracked (only Shift is).
- **No thread exit or reaping** (Phase 4): a thread that finishes its
  work has nowhere to go — the demo threads in `kmain` fall into an
  infinite `hlt` loop, still present in the ready queue forever, rather
  than being removed and having their stack/struct freed. If the entry
  function actually *returns* instead, `thread_exited`
  (`kernel/sched/thread.c`) panics; there's no supported exit path yet,
  only a backstop against silently executing garbage.
- **No blocking/sleep primitive** (Phase 4): threads can only run or
  spin; there's no `sched_block`/`sched_wake` pair or wait queue, so
  anything that needs to wait for an event (I/O, a timer, another
  thread) has to busy-loop calling `sched_yield` itself. A real
  scheduler wants blocked threads out of the ready rotation entirely.
- **Fixed per-thread stack size, no overflow beyond the guard page**
  (Phase 4): every thread gets exactly `THREAD_STACK_PAGES` (4, 16KiB)
  via `vmm_alloc_guarded` — correct enough to catch an overflow as a
  clean page fault instead of silent corruption (that's the whole point
  of the guard page), but there's no way to request a larger stack for
  a thread that needs one.
- **SMP**: `kernel/arch/x86_64/cpu` brings up the boot processor (BSP) only.
  Application processor (AP) bring-up via the MADT's LAPIC entries, the
  INIT-SIPI-SIPI sequence, and per-CPU scheduler runqueues are not
  implemented. Flagged as an advanced/optional milestone — see below.

## Explicitly out of scope

- **Real hardware.** Quin targets QEMU (`qemu-system-x86_64` + OVMF) only.
  No ACPI quirks tables for physical chipsets, no driver bloat for hardware
  you can't test in CI. See `docs/FAQ.md`.
- **BIOS/legacy boot, GRUB.** One boot path (UEFI via Limine), done
  correctly, rather than two done halfway.
- **Legacy 8259 PIC.** IRQs route through IOAPIC/LAPIC from Phase 1 onward.
  The PIC is never programmed, not even to mask it — Limine/UEFI leaves it
  unused and we don't touch it.
- **aarch64 / riscv64.** `kernel/arch/x86_64` is deliberately the only arch
  directory. Porting is a real project on its own; this template doesn't
  pretend to have solved it. The seam is the directory boundary itself —
  arch-independent code lives outside `kernel/arch/x86_64`.
- **A filesystem.** No VFS, no block device driver, no on-disk format.
  Everything the kernel needs (itself, modules) comes from Limine-loaded
  files handed off at boot.
- **SMP (multi-core).** Explicitly flagged as an advanced/optional
  milestone, not a broken promise: AP bring-up is real work (trampoline
  code in low memory, per-CPU GDT/TSS/IDT, a scheduler that's actually
  SMP-safe rather than just spinlock-protected) and half-implementing it
  would be worse than a clear boundary. If you take this on in your fork,
  start from the MADT's LAPIC entries (`kernel/acpi/madt.c` already
  counts them) for AP discovery, and `kernel/sched/spinlock.c`'s atomic
  (not `cli`/`sti`-based) locks, which were written to already be
  SMP-correct even though nothing exercises that yet.
- **A hardened security model.** No KASLR, no SMEP/SMAP enforcement audit,
  no userspace isolation guarantees. See `SECURITY.md`.
- **FPU/SSE state.** The kernel builds with `-mno-sse -mno-sse2 -mno-mmx`
  and never touches FPU state. If your fork needs floating point (e.g. for
  a userspace), you must add explicit `FXSAVE`/`FXRSTOR` (or `XSAVE`) state
  save/restore on every context switch before enabling SSE codegen —
  enabling it without that will silently corrupt FPU state across
  preemption.

## Advanced / optional milestones

Not part of the phase sequence — pick these up in a fork once Phase 6 is
solid, if you want to push further:

- [ ] SMP: AP bring-up, per-CPU structures, an SMP-safe scheduler.
- [ ] A real syscall table and userspace ABI.
- [ ] A minimal VFS + ramdisk.
