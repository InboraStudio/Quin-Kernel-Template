# Roadmap

This tracks what actually works, what's scaffolded but incomplete, and what's
explicitly out of scope. If something isn't checked off here, don't assume it
works just because the file exists — read the "scaffolded" and "out of scope"
sections before relying on anything.

Status is tracked per phase. A phase is only checked off once it builds with
zero warnings, passes `clang-format --check`, and boots cleanly in the QEMU
smoke test.

## Implemented

- [ ] **Phase 0 — Toolchain & boot skeleton**: Limine boots the kernel via
      UEFI, higher-half linking, banner printed over serial + framebuffer.
- [ ] **Phase 1 — CPU bring-up**: GDT/TSS, IDT with all 32 exception vectors,
      IOAPIC/LAPIC IRQ routing, panic screen with register dump + stack trace.
- [ ] **Phase 2 — Memory management**: bitmap PMM, 4-level paging VMM with
      guard pages, kernel heap, page fault handler.
- [ ] **Phase 3 — Platform services**: ACPI table parsing, LAPIC timer
      calibration, leveled logger, PS/2 keyboard driver.
- [ ] **Phase 4 — Concurrency skeleton**: kernel threads, round-robin
      scheduler, spinlocks.
- [ ] **Phase 5 — Syscall groundwork**: `syscall`/`sysret` entry stub, ring-3
      jump path.
- [ ] **Phase 6 — Test harness & polish**: host-side unit tests, QEMU
      `isa-debug-exit` CI smoke test.

## Scaffolded but incomplete

Nothing yet — this section fills in as phases land. Expect entries like:

- **Syscall dispatch** (Phase 5): the `syscall` entry stub saves state and
  returns `-ENOSYS` for every call number. There is no syscall table, no
  argument marshaling convention, and no per-syscall permission model. This
  is intentional — Quin ends at "you have a safe ring-3 entry point," and
  your kernel's syscall ABI is your own design decision.
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
  start from the MADT parsing already in `kernel/acpi`.
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
