# Quin Kernel Template

```
 /$$$$$$            /$$
 /$$__  $$          |__/
| $$  \ $$ /$$   /$$ /$$ /$$$$$$$
| $$  | $$| $$  | $$| $$| $$__  $$
| $$  | $$| $$  | $$| $$| $$  \ $$
| $$/$$ $$| $$  | $$| $$| $$  | $$
|  $$$$$$/|  $$$$$$/| $$| $$  | $$
 \____ $$$ \______/ |__/|__/  |__/
      \__/
```

A clean, correct, documented starting point for writing your own x86_64
kernel from scratch. Boots via UEFI + [Limine](https://github.com/limine-bootloader/limine)
in QEMU with a real physical/virtual memory manager, interrupts, and a
scheduler skeleton already working — so you start writing *your* kernel
instead of fighting a linker script.

[![build](https://github.com/InboraStudio/Quin-Kernel-Template/actions/workflows/build.yml/badge.svg)](https://github.com/InboraStudio/Quin-Kernel-Template/actions/workflows/build.yml)
[![lint](https://github.com/InboraStudio/Quin-Kernel-Template/actions/workflows/lint.yml/badge.svg)](https://github.com/InboraStudio/Quin-Kernel-Template/actions/workflows/lint.yml)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Built by [Inbora Studio](https://github.com/InboraStudio).

This README is the long-form reference: what every piece does, why it's
built the way it is, and — the part most templates skip — concretely
where to start when you want to extend or replace each piece. The
`docs/` folder holds the same information split into focused files if
you'd rather jump straight to one topic; see the [documentation
index](#documentation-index).

## Table of contents

- [Quick start](#quick-start)
- [What's implemented](#whats-implemented)
- [Architecture](#architecture)
  - [Boot sequence](#boot-sequence)
  - [Memory layout](#memory-layout)
  - [Toolchain and build system](#toolchain-and-build-system)
  - [Phase 1 — CPU bring-up](#phase-1-cpu-bring-up)
  - [Phase 2 — Memory management](#phase-2-memory-management)
  - [Phase 3 — Platform services](#phase-3-platform-services)
  - [Phase 4 — Concurrency](#phase-4-concurrency)
  - [Phase 5 — Syscalls and ring 3](#phase-5-syscalls-and-ring-3)
- [Directory structure](#directory-structure)
- [Developer workflow](#developer-workflow)
- [Extending the template](#extending-the-template)
  - [Adding a driver](#adding-a-driver)
  - [Thread exit and reaping](#thread-exit-and-reaping)
  - [A blocking/sleep primitive](#a-blockingsleep-primitive)
  - [SMP (multi-core)](#smp-multi-core)
  - [A real syscall table](#a-real-syscall-table)
  - [A filesystem](#a-filesystem)
  - [Porting to another architecture](#porting-to-another-architecture)
  - [Double-fault hardening (IST)](#double-fault-hardening-ist)
  - [Deeper ACPI (FADT / AML)](#deeper-acpi-fadt-aml)
  - [More keyboard coverage](#more-keyboard-coverage)
  - [Growing the heap](#growing-the-heap)
  - [Reclaiming bootloader memory](#reclaiming-bootloader-memory)
  - [Symbolicated panic stack traces](#symbolicated-panic-stack-traces)
- [Known limitations](#known-limitations)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Documentation index](#documentation-index)
- [License](#license)

## Quick start

```bash
git clone --recursive https://github.com/InboraStudio/Quin-Kernel-Template.git
cd Quin-Kernel-Template
./scripts/setup-toolchain.sh
./scripts/build.sh && ./scripts/run.sh
```

Linux and macOS are supported natively; on Windows, run this inside WSL2 —
`setup-toolchain.sh` will tell you so if you run it on native Windows.

Already cloned without `--recursive`? Run `git submodule update --init` to
fetch the pinned Limine submodule before building.

## What's implemented

Everything below is real, booted-in-QEMU, working code — not a stub with
a matching name. `docs/ROADMAP.md` is the single kept-current source of
truth (implemented / scaffolded / explicitly out of scope); this table
is a snapshot of it.

| Area | What you get | Owning code |
|---|---|---|
| Boot | UEFI via Limine, higher-half kernel at `-2GiB`, banner on serial + framebuffer | `kernel/arch/x86_64/boot/` |
| CPU | GDT/TSS, full IDT (32 exception vectors + 16 IRQs), IOAPIC/LAPIC routing (no legacy PIC), panic screen with register dump + stack trace | `kernel/arch/x86_64/cpu/` |
| Memory | Bitmap PMM, 4-level paging VMM, guard pages, lazy (demand-paged) regions, kernel heap, page fault handler | `kernel/mm/`, `kernel/arch/x86_64/mm/` |
| Platform | ACPI (RSDP→XSDT→MADT), LAPIC timer calibrated against the PIT, leveled logger, PS/2 keyboard | `kernel/acpi/`, `kernel/arch/x86_64/cpu/lapic_timer.c`, `kernel/lib/log.c`, `kernel/drivers/` |
| Concurrency | Kernel threads, hand-rolled context switching, preemptive round-robin scheduler, atomic spinlocks | `kernel/sched/`, `kernel/arch/x86_64/cpu/context_switch.S` |
| Syscalls | `SYSCALL`/`SYSRET` entry stub, ring-3 jump path (every call returns `-ENOSYS` — intentionally) | `kernel/arch/x86_64/cpu/syscall.c` |
| Testing | Host-side unit tests (no QEMU), QEMU boot smoke test in CI | `tests/unit/`, `tests/integration/` |

Not implemented, on purpose: SMP, a real syscall ABI, a filesystem. See
[Known limitations](#known-limitations) and [Extending the
template](#extending-the-template) for what's involved in adding each.

## Architecture

### Boot sequence

1. Firmware (OVMF) loads Limine as a UEFI application from the ISO's
   `/EFI/BOOT/BOOTX64.EFI`.
2. Limine reads `/boot/limine/limine.conf`, loads `/boot/quin-kernel`,
   sets up long mode, its own GDT, paging, and a stack exactly per
   `third_party/limine/PROTOCOL.md` ("Machine State at Entry"), then
   jumps to the ELF entry point — no hand-written assembly stub needed
   first, since Limine already hands off in long mode with paging on.
3. `kernel_entry` (`kernel/arch/x86_64/boot/entry.c`) verifies the
   bootloader honored the requested base revision, then calls `kmain`.
4. `kmain` (`kernel/main.c`) brings up every subsystem in dependency
   order and never returns. Reading it top to bottom *is* a tour of the
   whole boot process — it's intentionally kept as one flat, readable
   sequence rather than scattered init hooks.

The kernel requests **Limine protocol base revision 4** (bumped up from
revision 3 partway through development — see `kernel/arch/x86_64/boot/limine_requests.c`
for the exact reasoning): revision 4 is the first revision that
guarantees ACPI tables are actually reachable through the Higher Half
Direct Map at all.

### Memory layout

Higher-half kernel at `-2GiB`, matching `-mcmodel=kernel`.

| Range (virtual) | Contents | Permissions |
|---|---|---|
| `0x0000000000000000`–`0x00007fffffffffff` | Userspace (Phase 5 demo only; no general per-process address spaces) | user, per-mapping |
| `0xffff800000000000`–... | HHDM (Higher Half Direct Map); offset varies per boot, read via `boot_get_hhdm_offset()` | supervisor, RW, NX |
| `0xffffffff80000000` (`KERNEL_VMA`) | `.text` | supervisor, R+X |
| next page | `.rodata` | supervisor, R only |
| next page | `.data`, `.bss`, Limine request structs | supervisor, RW, NX |
| `0xffffffff90000000` (`HEAP_VBASE`) | Kernel heap, 16MiB reserved, lazily backed | supervisor, RW, NX |
| `0xffffffffb0000000` (`VMM_DYNAMIC_VBASE`) | `vmm_alloc_guarded` allocations (thread stacks, ...) | supervisor, RW, NX |
| `0xffffffffc0000000` (`VMM_MMIO_VBASE`) | Fixed-purpose MMIO (LAPIC, IOAPIC) | supervisor, RW, cache-disabled |

Full detail, including why each boundary is where it is, in
`docs/ARCHITECTURE.md`.

### Toolchain and build system

Clang + LLD targeting `x86_64-none-elf` — no GCC cross-compiler build
step (see `docs/FAQ.md`). Plain `Makefile`s, no CMake/Meson. The exact,
non-negotiable compile flags live at the top of the top-level
`Makefile`; `-nostdinc` plus an explicit `-isystem` into Clang's own
resource directory keeps the host's glibc headers completely
unreachable, so a stray `#include "string.h"` from the wrong directory
fails loudly instead of silently linking against the wrong `memcpy`.

`third_party/limine` is a git submodule of `limine-protocol` (the
header only, pinned by commit — that repo has no tags), while the
prebuilt Limine bootloader binaries are fetched by the `Makefile` from a
pinned GitHub release into `.cache/` (gitignored). See
`docs/ARCHITECTURE.md`, "Toolchain provenance," for the full reasoning
— short version: the current Limine release line doesn't ship those
binaries in a git-trackable form, so they get the same "fetched by
exact version, not vendored" treatment `scripts/setup-toolchain.sh`
already gives OVMF and QEMU.

### Phase 1 — CPU bring-up

**GDT** (`kernel/arch/x86_64/cpu/gdt.c`): kernel code/data, user
code/data, and a TSS, in an order that isn't arbitrary — `SYSCALL`/
`SYSRET` (Phase 5) derive CS/SS from fixed offsets into a single `STAR`
MSR field, which only produces the right selectors given this exact
ordering.

| Selector | Segment | DPL |
|---|---|---|
| `0x00` | Null | — |
| `0x08` | Kernel code | 0 |
| `0x10` | Kernel data | 0 |
| `0x18` | User data | 3 |
| `0x20` | User code | 3 |
| `0x28` | TSS (16 bytes) | 0 |

**IDT** (`idt.c` + `isr_stubs.S` + `isr.c`): all 256 gates are interrupt
gates. Vectors 0–31 are CPU exceptions (ten of them carry a hardware
error code; the assembly stubs push a dummy 0 for the rest so every
exception reaches the C dispatcher with the same frame shape). Vectors
32–47 are the legacy IRQ lines, routed through the **IOAPIC** — this
template never programs the 8259 PIC, not even to mask it. Vector
`0xff` is the LAPIC's spurious vector, handled by a bare `iretq` with no
EOI (SDM Vol. 3A, 11.9 — EOI'ing a spurious interrupt is specifically
wrong).

Every interrupt here happens at CPL0→CPL0, no IST — so the CPU always
pushes exactly `RIP`/`CS`/`RFLAGS`, and `iretq` pops correctly for that
shape. (This assumption gets tested for real by Phase 5's ring-3 demo —
see [below](#phase-5-syscalls-and-ring-3).)

**Panic** (`panic.c`): `panic_exception(frame)` for an unhandled CPU
exception (full register dump, `CR2` on a page fault) and `panic(fmt,
...)` for a kernel-detected fatal condition. Both walk the RBP chain for
a stack trace (`-fno-omit-frame-pointer` in the `Makefile` keeps it
walkable) and halt. No symbol resolution — see
[here](#symbolicated-panic-stack-traces) if you want to add it.

### Phase 2 — Memory management

**PMM** (`kernel/mm/pmm.c`): a bitmap, one bit per 4KiB frame, built
from the bootloader's memory map. Every frame starts "in use"; only
frames inside a usable region get cleared. The actual bit-twiddling
lives in `kernel/lib/bitmap.c` — deliberately dependency-free so
`tests/unit/test_bitmap.c` can exercise it on the host, no QEMU needed.

**VMM** (`kernel/arch/x86_64/mm/vmm.c`): 4-level paging, continuing to
extend the page tables Limine already built rather than switching to a
fresh address space. Four primitives:

```c
bool  vmm_map(uint64_t virt, uint64_t phys, uint32_t flags);
void  vmm_unmap(uint64_t virt);
bool  vmm_register_lazy_region(uint64_t start, uint64_t end, uint32_t flags);
void *vmm_alloc_guarded(uint64_t page_count, uint32_t flags);
void *vmm_map_mmio(uint64_t phys_addr);
```

`vmm_alloc_guarded` maps `page_count` pages with one unmapped guard page
immediately before and after, so an off-by-one write faults cleanly
instead of corrupting a neighbor — this is what `thread_create` uses for
every kernel stack. `vmm_register_lazy_region` backs a virtual range
on-demand: `kernel/mm/heap.c`'s entire 16MiB range is registered this
way, so the heap only spends physical frames on pages it actually
touches, discovered through the page fault handler
(`vmm_handle_page_fault`, registered against vector 14 in `vmm_init`).

**Heap** (`kernel/mm/heap.c`): a doubly-linked, first-fit,
splitting-and-coalescing allocator — `void *kmalloc(uint64_t size)` /
`void kfree(void *ptr)`.

### Phase 3 — Platform services

**ACPI** (`kernel/acpi/acpi.c` + `madt.c`): validates the RSDP, walks to
the XSDT (RSDT fallback for ACPI 1.0), and exposes tables by signature:

```c
void acpi_init(void);
struct acpi_table_header *acpi_find_table(const char *signature); /* e.g. "APIC" */

bool madt_parse(struct madt_info *out); /* enabled CPU count, IOAPIC base */
```

`kmain` uses `madt_parse` to find the real IOAPIC base address, falling
back to the conventional `0xfec00000` only if the MADT is unavailable.

**Timer** (`kernel/arch/x86_64/cpu/lapic_timer.c` + `kernel/drivers/timer/timer.c`):
calibrates the LAPIC timer against PIT channel 2 (polled via port
`0x61`, no IRQ0 needed for calibration itself), then runs it periodically
at 1000Hz, driving the scheduler's preemption. `timer_get_ticks()` for
anything that needs a monotonic tick count.

**Logger** (`kernel/lib/log.c`): `log_debug`/`log_info`/`log_warn`, each
writing a `[LEVEL]` tag plus the message to serial and (if available)
the framebuffer console, color-coded per level.

**Keyboard** (`kernel/drivers/keyboard/keyboard.c`): PS/2, Scan Code Set
1, Shift tracking, non-blocking `char keyboard_read_char(void)`.

### Phase 4 — Concurrency

**Spinlocks** (`kernel/sched/spinlock.c`): real atomic
test-and-test-and-set (`__atomic_exchange_n`/`__atomic_store_n`), not a
`cli`/`sti` stand-in — written to already be SMP-correct even though
nothing exercises that yet. `_irqsave`/`_irqrestore` variants for locks
touched from interrupt context (the scheduler's ready queue needs
these, or a same-CPU interrupt could deadlock against its own
interrupted owner).

**Threads and context switching** (`kernel/sched/thread.c` +
`kernel/arch/x86_64/cpu/context_switch.S`): a context switch is treated
as an unusual function call — `context_switch(&old->stack_pointer,
new->stack_pointer)` saves the SysV callee-saved registers onto the
outgoing stack and loads the incoming one. A thread that's never run
gets a fake initial stack frame built by `thread_create`, landing in
`thread_trampoline` on its first switch-in.

```c
struct thread *thread_create(thread_entry_fn entry, void *arg);
```

**Scheduler** (`kernel/sched/sched.c`): a circular ready list,
round-robin, preempted every `SCHED_TIME_SLICE_TICKS` (10ms) by the
timer ISR, or immediately via `sched_yield()`.

A real deadlock was found and fixed building this: the timer ISR used
to send EOI *after* the handler returned, but `sched_tick` can
context-switch away instead of returning — so a newly-scheduled thread
(which re-enables interrupts in `thread_trampoline`) could start running
*before* the interrupt that triggered the switch was ever acknowledged,
permanently starving the LAPIC of further ticks. Fixed by EOI'ing
before the handler runs. Full account in `docs/ARCHITECTURE.md`.

### Phase 5 — Syscalls and ring 3

**SYSCALL/SYSRET** (`kernel/arch/x86_64/cpu/syscall.c` + `syscall_entry.S`):
`SYSCALL` doesn't touch `RSP` or consult the TSS — `syscall_entry`'s
first job is swapping onto a known-good kernel stack before touching
memory for anything else. The dispatcher reads a call number from
`%rax` and returns `-ENOSYS` unconditionally:

```c
int64_t syscall_dispatch(uint64_t syscall_number); /* always -ENOSYS */
```

**Ring 3** (`jump_to_ring3` in `syscall_entry.S`): builds a five-item
`iretq` frame by hand for the *initial* jump into ring 3 (as opposed to
`SYSRET`, which only makes sense returning from a syscall already in
flight).

```c
void syscall_init(void);
NORETURN void jump_to_ring3(uint64_t entry, uint64_t user_stack);
```

`kmain` demonstrates the whole path: maps one user-accessible page,
writes a 4-byte program (`syscall; jmp $`), and jumps to it — the
`syscall_dispatch` log line is the proof the round trip works. Building
this demo caught a real bug shipped since Phase 2: `vmm.c`'s
intermediate page-table entries never got the User bit, so every
mapping in the kernel had been silently supervisor-only the whole time,
invisible until something finally tried to execute from a page whose
leaf PTE genuinely said `User=1`.

## Directory structure

```
kernel/
├── arch/x86_64/
│   ├── boot/     Limine request structs, ELF entry point, linker script
│   ├── cpu/      GDT/TSS/IDT, port I/O, panic, LAPIC/IOAPIC, syscall/ring-3, context switch
│   └── mm/       4-level paging VMM
├── mm/           Arch-independent PMM, kernel heap
├── drivers/      serial, framebuffer, keyboard, timer
├── acpi/         RSDP → XSDT/RSDT → MADT parsing
├── sched/        scheduler, threads, spinlocks
├── lib/          freestanding string/printf/log/bitmap
├── include/      cross-cutting headers (kernel.h, boot_info.h)
└── main.c        kmain — the whole boot sequence, top to bottom

tests/
├── unit/         host-side, no QEMU (make -C tests/unit)
└── integration/  the QEMU boot smoke test (scripts/run.sh test)

scripts/          setup-toolchain.sh, build.sh, run.sh, debug.sh, clean.sh
docs/             ARCHITECTURE.md, ROADMAP.md, GETTING_STARTED.md, CODING_STYLE.md, FAQ.md
```

Module headers live next to their `.c` file (`kernel/drivers/serial/serial.h`
alongside `serial.c`); `kernel/include/` is only for headers genuinely
shared across subsystem boundaries. There's no `kernel/syscall/` — the
`SYSCALL`/`SYSRET` mechanism is a different CPU instruction on every
architecture, so unlike the timer/keyboard split there's no meaningful
arch-independent syscall layer to carve out yet; it all lives in
`kernel/arch/x86_64/cpu/`.

## Developer workflow

```bash
./scripts/build.sh            # compiles the kernel, assembles build/quin-kernel.iso
./scripts/run.sh               # boots it in QEMU with a graphical window
./scripts/run.sh test          # headless boot + isa-debug-exit smoke test (what CI runs)
./scripts/debug.sh             # boots QEMU paused, attaches a terminal gdb session
./scripts/clean.sh             # removes build/
./scripts/clean.sh --all       # also drops the cached Limine binary download
make -C tests/unit              # host-side unit tests
./scripts/build.sh fmt-check   # clang-format --dry-run (what lint.yml runs)
./scripts/build.sh fmt         # clang-format -i -- fixes formatting in place
```

VS Code: open the folder, `.vscode/tasks.json` has build/run/test tasks
(`Ctrl+Shift+B` for build), `.vscode/launch.json` attaches the debugger
to a paused QEMU instance (F5). No local setup? `.devcontainer/` has the
whole toolchain baked into a Dockerfile.

## Extending the template

This is the part most kernel templates skip: concretely, where do you
start? Every entry below names the actual files and functions involved
— not just "you'll need to add X."

### Adding a driver

Follow the pattern in `kernel/drivers/keyboard/` (the newest, most
complete example): a `driver_init(void)` that registers an interrupt
handler, plus whatever public API the driver exposes.

1. Create `kernel/drivers/<name>/<name>.h` + `.c`.
2. If it's interrupt-driven, register a handler:
   ```c
   void interrupt_register_handler(uint8_t vector, interrupt_handler_fn handler);
   ```
   from `kernel/arch/x86_64/cpu/isr.h`. Pick an unused vector in the
   32–47 IRQ range (check `kmain` for what's already claimed: 32 is the
   timer, 33 is the keyboard).
3. Route the IRQ through the IOAPIC:
   ```c
   void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked);
   ```
   from `kernel/arch/x86_64/cpu/ioapic.h`, called from `kmain` (see how
   the keyboard does it, right after `keyboard_init()`).
4. For MMIO-backed devices, map the register window with `vmm_map_mmio`
   (`kernel/arch/x86_64/mm/vmm.h`) — the same call LAPIC/IOAPIC bring-up
   uses.
5. Call your `driver_init()` from `kmain` (`kernel/main.c`) in the right
   place relative to its dependencies (after `vmm_init`/`pmm_init` if it
   needs to map anything, after `sched_init` if it's expected to run
   before threads exist, etc.).

### Thread exit and reaping

Right now a thread that finishes just falls into an infinite `hlt` loop
(see the demo threads in `kernel/main.c`), permanently occupying a slot
in the ready ring. If the entry function actually *returns* instead,
`thread_exited` (`kernel/sched/thread.c`) panics — it's a bug backstop,
not a supported exit path.

To add real exit:

1. Add a `THREAD_DEAD` (or similar) state to `struct thread`
   (`kernel/sched/thread.h`).
2. In `kernel/sched/sched.c`, teach `switch_to_next` (or a new
   `sched_reap` step) to unlink a dead thread from the ready ring —
   it's a plain circular singly-linked list, so removal means finding
   the predecessor and repointing `->next`.
3. Freeing the thread's stack (`vmm_alloc_guarded`'s return value,
   stored in `thread->stack_base`) and the `struct thread` itself
   (`kfree`) has to happen *after* you've switched away from that
   thread's own stack — never free memory you're still executing on.
   The cleanest place is from whichever thread runs next, right after
   `context_switch` returns.
4. Change `thread_exited` to call your new reap path instead of
   `panic()`.

### A blocking/sleep primitive

Threads can currently only run or spin — there's no `sched_block`/
`sched_wake` pair, so anything waiting on an event has to busy-loop
calling `sched_yield()`. To add one:

1. Add a `THREAD_BLOCKED` state and, likely, a wait-queue pointer to
   `struct thread`.
2. `sched_block(struct thread **wait_queue)`: remove the current thread
   from the ready ring (same unlink logic as thread reaping above), push
   it onto `*wait_queue`, then `context_switch` to the next ready
   thread — note you can't use `sched_yield` for this, since that
   assumes the current thread stays ready.
3. `sched_wake(struct thread **wait_queue)`: pop a thread off the wait
   queue and re-insert it into the ready ring (`sched_add_thread` already
   does the insertion half).
4. Protect the wait queue with a `spinlock_acquire_irqsave` — the same
   reasoning as the ready queue's own lock applies (Phase 4's actual
   deadlock came from getting exactly this kind of interrupt-context
   interaction wrong once already; read that account in
   `docs/ARCHITECTURE.md` before touching this).

### SMP (multi-core)

Explicitly out of scope for the template itself (see
`docs/ROADMAP.md`), but the groundwork is there:

- `kernel/acpi/madt.c` already counts enabled Local APIC entries
  (`madt_info.enabled_cpu_count`) — extend it to also collect each
  entry's APIC ID, which is what AP bring-up (the INIT-SIPI-SIPI
  sequence) needs to target each core.
- `kernel/sched/spinlock.c`'s locks are already real atomics, not
  `cli`/`sti`, specifically so they'd still be correct once a second
  core exists.
- What's missing: a trampoline in low (<1MB) identity-mapped memory for
  APs to start executing at, per-CPU GDT/TSS/IDT (or at least per-CPU
  TSS.RSP0), a way to address per-CPU data (traditionally `GS`-relative,
  via `swapgs`), and a scheduler that's actually safe with multiple
  cores pulling from (or contending on) the same ready list —
  `kernel/sched/sched.c`'s current single global ready ring would need
  to become per-CPU runqueues with a load-balancing story, not just a
  bigger lock.

### A real syscall table

`syscall_dispatch(uint64_t syscall_number)` in
`kernel/arch/x86_64/cpu/syscall.c` currently ignores its argument beyond
logging it. To build a real table:

1. Decide your ABI. This template deliberately doesn't — arguments
   *would* conventionally arrive in `%rdi`/`%rsi`/`%rdx`/`%r10`/`%r8`/`%r9`
   (Linux's convention, and for the same reason: `%rcx` is clobbered by
   `SYSCALL` itself), but `syscall_entry.S` currently doesn't preserve or
   marshal them — only `%rax` (the call number) crosses into C today.
2. Extend `syscall_entry` (`syscall_entry.S`) to build a frame with the
   argument registers and pass a pointer to it, the same pattern
   `isr_common_stub` already uses for interrupts.
3. Replace the single `-ENOSYS` return in `syscall_dispatch` with a
   table lookup (an array of function pointers indexed by call number is
   the simplest starting point) and add real handlers.
4. Think about validation: every pointer a syscall receives is
   user-controlled and must be checked against what's actually mapped
   and actually user-accessible before the kernel dereferences it — this
   template's page fault handler (`vmm_handle_page_fault`) currently
   assumes any fault is either a lazy-mapping opportunity or a kernel
   bug; a real syscall boundary needs to distinguish "user handed me a
   bad pointer" (return an error) from those two cases as well.

### A filesystem

No VFS, no block device driver, no on-disk format exists. Starting
points if you add one:

- A block device driver would live in `kernel/drivers/` (following the
  existing pattern) — AHCI/NVMe/virtio-blk are the realistic QEMU-testable
  options.
- A VFS layer is architecturally similar to `kernel/acpi`'s relationship
  to `kernel/drivers`: a thin, protocol-specific parser underneath an
  abstraction other code depends on. `kernel/include/boot_info.h`'s
  boot-protocol-agnostic structs are the template's existing example of
  that pattern, worth reading before designing the VFS's own interface.
- `kernel/mm/heap.c`'s `kmalloc`/`kfree` are there for whatever
  in-memory structures a filesystem cache needs.

### Porting to another architecture

`kernel/arch/x86_64` is deliberately the *only* arch directory — nothing
here pretends porting is easy, but the seam is real:

- Everything outside `kernel/arch/x86_64` is meant to be
  arch-independent already. `kernel/include/boot_info.h` is the
  clearest example: `kernel/arch/<arch>/boot/` would be responsible for
  filling in the same plain structs from whatever boot protocol that
  architecture uses (Limine supports aarch64 and riscv64 too, so
  reusing the boot protocol is realistic even if the CPU bring-up isn't).
- `kernel/drivers/timer/timer.c` and `kernel/drivers/keyboard/keyboard.c`
  already show the split: arch-independent policy (tick counting, a
  scancode buffer) calling into an arch-specific backend
  (`kernel/arch/x86_64/cpu/lapic_timer.c`, raw PS/2 port I/O). A new
  arch's timer/interrupt-controller bring-up slots in the same way.
- CPU bring-up itself (GDT/IDT-equivalents, paging, context switching)
  is not portable in any meaningful sense — aarch64's exception model,
  page table format, and calling convention are different enough that
  `kernel/arch/x86_64/cpu/` and `kernel/arch/x86_64/mm/` are closer to a
  worked example to reference than code to adapt.

### Double-fault hardening (IST)

Vector 8 (`#DF`) is currently a normal IDT gate — no IST (Interrupt
Stack Table) entry — so a double fault caused by kernel stack overflow
runs its handler on the same already-overflowed stack, which can fault
again and triple-fault the machine instead of producing a clean panic.

To fix it:

1. Allocate a dedicated stack (`vmm_alloc_guarded` again) for IST1.
2. Set `TSS.IST1` to its top (`kernel/arch/x86_64/cpu/gdt.c` owns the
   TSS structure).
3. Set the IST field (currently always 0) to 1 for vector 8's IDT entry
   only (`kernel/arch/x86_64/cpu/idt.c`'s `set_gate`).
4. The tricky part: an IST-induced stack switch means the CPU pushes
   `RSP`/`SS` in addition to `RIP`/`CS`/`RFLAGS`, same as a ring3→ring0
   transition. `isr_common_stub` (`isr_stubs.S`) already handles this
   correctly *without any changes* — `iretq`'s pop count is decided at
   execution time from the `CS` value, not something the stub branches
   on (verified for the ring-3 case in Phase 5's testing; the same
   reasoning applies here). The actual risk is elsewhere: make sure
   nothing assumes `struct interrupt_frame` (`isr.h`) has fields it
   doesn't (it currently has none for `RSP`/`SS`, on purpose, since
   nothing needed them before).

### Deeper ACPI (FADT / AML)

`kernel/acpi` reads fixed-layout tables (RSDP, RSDT/XSDT, MADT) only.
The FADT and AML (ACPI Machine Language, inside the DSDT/SSDT) are a
fundamentally different, much larger undertaking — a bytecode
interpreter, not a struct parser. If you need it (power management,
device enumeration beyond the MADT's fixed entries): `acpi_find_table("FACP")`
already works for locating the FADT itself with the existing
`acpi_find_table` API; the DSDT pointer is a field inside it. Writing an
AML interpreter from there is its own project — look at ACPICA (the
reference implementation) or a minimal hobby interpreter for scope
before starting.

### More keyboard coverage

`kernel/drivers/keyboard/keyboard.c`'s scancode tables cover the
alphanumeric block, punctuation, space, enter, tab, and backspace only.
To add more:

- **F-keys, arrows, numpad, right Ctrl/Alt**: these use the `0xE0`
  extended-scancode prefix in Scan Code Set 1. `keyboard_isr` currently
  has no state for "the previous byte was `0xE0`" — add a `static bool
  extended_prefix` alongside `shift_held`, set it when `inb` returns
  `0xE0`, and consult it (then clear it) on the next byte.
- **Caps Lock**: track it the same way `shift_held` is tracked (make
  code `0x3A` toggles a `static bool caps_lock_on` on press, ignore the
  release), then combine it with `shift_held` when indexing into
  `shifted_table`/`unshifted_table` (Caps Lock XORs with Shift for
  letters specifically, not for digits/punctuation — worth getting
  right rather than treating them identically).
- **8042 controller initialization**: `keyboard_init` assumes the
  controller and port are already enabled, true for every QEMU machine
  type by firmware default. Real, potentially-uninitialized hardware
  needs the actual sequence: disable both PS/2 ports, flush the output
  buffer, self-test (`0xAA`, expect `0x55`), enable the keyboard port,
  reset the keyboard itself (`0xFF`, expect `0xFA` then `0xAA`). OSDev
  Wiki's "8042 PS/2 Controller" page has the full sequence.

### Growing the heap

`kmalloc` returns `NULL` once the 16MiB reserved region
(`kernel/mm/heap.c`'s `HEAP_SIZE`) is exhausted — there's no growth
path. Two directions:

- **Simplest**: just raise `HEAP_SIZE`. It's a lazily-backed region
  (`vmm_register_lazy_region`), so a bigger reservation costs virtual
  address space only, not physical memory, until pages are actually
  touched.
- **Real growth**: register a second lazy region adjacent to the first
  when the allocator can't satisfy a request, and extend `kmalloc`'s
  free-list walk to span both. This is more work than it sounds like
  the first time — the free list's `struct block_header` links
  (`kernel/mm/heap.c`) would need to bridge two disjoint memory ranges
  cleanly.

### Reclaiming bootloader memory

The PMM (`kernel/mm/pmm.c`) permanently excludes bootloader-reclaimable
memory, since Limine's own page tables live there and this kernel keeps
extending those tables rather than building an independent address
space. Reclaiming it safely needs, in order: (1) finish reading
everything you still need from bootloader-reclaimable regions — the
memmap itself, the RSDP — since once reclaimed those pages can be
overwritten; (2) build an independent set of page tables covering the
kernel image, HHDM, and anything else still needed (`vmm_map` can build
these once you have a fresh PML4 to point it at instead of the one CR3
currently holds); (3) switch `CR3`; (4) only then hand the old
bootloader-reclaimable frames to `pmm_free_frame`.

### Symbolicated panic stack traces

`panic.c`'s stack trace prints raw addresses. To resolve them to
function names, you'd need a symbol table available at runtime — the
straightforward approach is generating one at build time (e.g. `nm
build/quin-kernel.elf` into a sorted array of `{address, name}`, linked
into the kernel as a data section) and a binary search in
`print_stack_trace` (`kernel/arch/x86_64/cpu/panic.c`) to find the
nearest symbol at or below each return address. Until then,
`addr2line -e build/quin-kernel.elf <address>` or gdb against the same
ELF (it has full debug info — `-g` is always on) does the same job
outside the kernel.

## Known limitations

The full, kept-current list — including *why* each one is a limitation
and not an oversight — is `docs/ROADMAP.md`. Condensed:

| Limitation | Where |
|---|---|
| No IST for `#DF` — kernel stack overflow can triple-fault instead of panicking cleanly | Phase 1 |
| IOAPIC falls back to a hardcoded base only if the MADT is unavailable | Phase 1/3 |
| Panic stack traces aren't symbolicated | Phase 1 |
| Bootloader-reclaimable memory is never reclaimed | Phase 2 |
| `vmm_alloc_guarded` leaks frames on partial failure | Phase 2 |
| Kernel heap has a fixed 16MiB ceiling | Phase 2 |
| No FADT/AML parsing | Phase 3 |
| PS/2 driver has no 8042 init sequence, partial scancode coverage | Phase 3 |
| No thread exit/reaping; no blocking/sleep primitive | Phase 4 |
| Fixed per-thread stack size | Phase 4 |
| One shared kernel stack for every ring-3 excursion | Phase 5 |
| No syscall table, argument convention, or permission model | Phase 5 |
| SMP, a real filesystem | Explicitly out of scope |

## Testing

Two layers:

- **`tests/unit/`** — host-side, no QEMU, no cross toolchain. Currently
  covers `kernel/lib/bitmap.c` (the only module that happened to already
  have zero freestanding-specific dependencies). `make -C tests/unit`.
  Extending this means finding another pure-logic core, giving it zero
  kernel-specific dependencies (the way `bitmap.c` has none), and
  testing *that* directly — not trying to link the whole kernel against
  a host compiler.
- **`tests/integration/smoke_test.sh`** — boots the real ISO headlessly
  in QEMU, asserts the boot banner appears on serial and the
  `isa-debug-exit` exit code is correct. Run via `scripts/run.sh test`.
  Every phase-specific behavior documented above (guard pages,
  preemption, the ring-3 round trip, the two real bugs this template
  shipped with and then fixed) was verified this same way during
  development — temporarily, with a deliberately-provoked failure added
  to `kmain`, checked, and removed — documented in `docs/ARCHITECTURE.md`
  rather than kept as a permanent automated test. Worth knowing before
  assuming either test layer alone would catch the next regression.

## Troubleshooting

- **Build fails with a missing tool**: `./scripts/setup-toolchain.sh`.
- **QEMU can't find OVMF**: `scripts/_common.sh`'s `quin_find_ovmf_code`/
  `quin_find_ovmf_vars` check a few known install paths; open an issue
  with your OS and how you installed OVMF if yours isn't one of them.
- **Nothing prints and QEMU just sits there**: `limine.conf` sets
  `timeout: 0` so it should auto-boot instantly — a stale `build/` from
  an interrupted build can confuse this. `./scripts/clean.sh && ./scripts/build.sh`.
- **macOS: "clang: error: unknown target triple"**: Homebrew's `llvm`
  formula is keg-only; the top-level `Makefile` auto-detects it via
  `brew --prefix llvm`, but if that fails, add
  `$(brew --prefix llvm)/bin` to your `PATH` directly.
- Anything else: open an issue with the bug report template — it asks
  for exactly the version/log info needed to reproduce.

## Contributing

See `CONTRIBUTING.md` for the full checklist (zero warnings,
`clang-format` clean, tests passing, `docs/ROADMAP.md` updated).
Conventional Commits, and a note on scope: this is a *template* — new
features should teach a generally applicable concept, not add
project-specific functionality that belongs in a downstream fork.

## Documentation index

| Doc | What's in it |
|---|---|
| [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) | Build/run/debug workflow, directory tour |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | The full, phase-by-phase design writeup this README summarizes |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | What's implemented, scaffolded, or out of scope — kept current |
| [`docs/CODING_STYLE.md`](docs/CODING_STYLE.md) | Naming, comment conventions, error-handling rules |
| [`docs/FAQ.md`](docs/FAQ.md) | Why Limine, why Clang, why QEMU-only, why no filesystem |

## License

MIT — see [`LICENSE`](LICENSE). Limine itself is 0BSD; see
`third_party/limine/LICENSE`.
