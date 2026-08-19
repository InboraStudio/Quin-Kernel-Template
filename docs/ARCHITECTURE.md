# Architecture

This document tracks the kernel's actual design as it's built, phase by
phase. Sections below are written when the phase that introduces them
lands; see `docs/ROADMAP.md` for what's implemented versus scaffolded.

## Boot sequence

1. Firmware (OVMF) loads Limine as a UEFI application from the ISO's
   `/EFI/BOOT/BOOTX64.EFI`.
2. Limine reads `/boot/limine/limine.conf`, loads `/boot/quin-kernel` (an
   ELF, protocol `limine`), sets up long mode, a GDT, paging, and a stack
   exactly as specified in `third_party/limine/PROTOCOL.md` ("Machine
   State at Entry"), then jumps to the ELF entry point.
3. `kernel_entry` (`kernel/arch/x86_64/boot/entry.c`) is that entry point.
   It calls `limine_requests_check()` to confirm the bootloader honored
   the requested base revision, then calls `kmain()`.
4. `kmain` (`kernel/main.c`) is the arch-independent entry. It brings up
   subsystems in dependency order and never returns.

No hand-written assembly stub sits before `kernel_entry`: Limine already
hands off in long mode with paging enabled, so the ELF entry point can be
a plain C function. See `kernel/arch/x86_64/boot/entry.c`.

## Why arch/x86_64/boot is the only place that knows about Limine

`kernel/arch/x86_64/boot/limine_requests.c` is the only translation unit
in the kernel that includes `<limine.h>`. It exposes plain structs (see
`kernel/include/boot_info.h`) to the rest of the kernel — drivers, `mm`,
`acpi` — none of which know or care that Limine is the bootloader. That
boundary is the seam for porting to a different boot protocol, or to
another architecture's `kernel/arch/<arch>/boot`, without touching
anything outside that one directory.

## Toolchain provenance

`third_party/limine` is a git submodule of
[`limine-bootloader/limine-protocol`](https://github.com/limine-bootloader/limine-protocol),
pinned to a commit (that repo has no tags, so a commit hash is the most
precise thing to pin — see the submodule's log). It provides
`include/limine.h`, the protocol header, and nothing else; the kernel
never builds Limine itself from source.

The actual bootloader binaries (`BOOTX64.EFI`, `limine-uefi-cd.bin`) are
prebuilt UEFI executables, not something a `git submodule` naturally
carries for this version line — Limine distributes them as GitHub Release
assets rather than in a git-trackable binary branch (see
`docs/FAQ.md`, "Why Limine instead of GRUB / Multiboot2?" for the version
history). `Makefile`'s `$(LIMINE_BINARY_DIR)/limine-uefi-cd.bin` rule
downloads a pinned release version (`LIMINE_VERSION` in the Makefile) into
`.cache/`, which is gitignored — the same treatment `scripts/setup-toolchain.sh`
gives OVMF and QEMU themselves: fetched by version, not vendored as binary
blobs in the repository.

The header (pinned via submodule) and the bootloader binary (pinned via
release version string) are updated together deliberately: bump
`LIMINE_VERSION` in the `Makefile` and re-point the submodule to a commit
from around the same time, then verify `scripts/run.sh test` still passes.

## Memory layout (x86_64)

Higher-half kernel at `-2GiB`, matching `-mcmodel=kernel`: the compiler
assumes every kernel symbol lives in the top (or bottom) 2GiB of address
space and encodes call/data references accordingly. See `docs/FAQ.md` for
why this split specifically.

| Range (virtual)                                | Contents                          | Permissions |
|-------------------------------------------------|------------------------------------|-------------|
| `0x0000000000000000` – `0x00007fffffffffff`     | Userspace (Phase 5 scaffolding only; nothing maps here yet) | user, per-mapping |
| `0xffff800000000000` – ...                      | HHDM (Higher Half Direct Map), base returned by Limine's HHDM feature, offset varies per boot | supervisor, RW, NX |
| `0xffffffff80000000` (`KERNEL_VMA`)              | `.text` — kernel code              | supervisor, R+X |
| next page boundary                               | `.rodata` — read-only data         | supervisor, R only |
| next page boundary                               | `.data`, `.bss`, Limine request structs | supervisor, R+W, NX |
| `0xffffffff90000000` (`HEAP_VBASE`)              | Kernel heap, 16MiB reserved, lazily backed (`kernel/mm/heap.c`) | supervisor, RW, NX |
| `0xffffffffb0000000` (`VMM_DYNAMIC_VBASE`)       | `vmm_alloc_guarded` allocations, each bounded by unmapped guard pages | supervisor, RW, NX |
| `0xffffffffc0000000` (`VMM_MMIO_VBASE`)          | Fixed-purpose MMIO (LAPIC, IOAPIC — `vmm_map_mmio`) | supervisor, RW, cache-disabled |

The exact section layout is defined in `kernel/arch/x86_64/linker.ld`;
`__kernel_start`/`__kernel_end` there bound the whole loaded image. Every
`PT_LOAD` segment is either read+execute or read+write, never both — see
the `PHDRS` block — matching the W^X discipline `kernel/arch/x86_64/mm/vmm.c`
enforces for everything it maps from Phase 2 onward.

The HHDM row's exact base isn't fixed: base revision 3 (what this kernel
requests — see `kernel/arch/x86_64/boot/limine_requests.c`) only maps
usable, bootloader-reclaimable, executable-and-modules, and framebuffer
memory-map regions into it, and the base address itself is chosen by the
bootloader per boot. `kernel/mm/pmm.c` and `kernel/arch/x86_64/mm/vmm.c`
read it from the HHDM feature response rather than assuming a fixed
offset.

## CPU state (Phase 1)

### GDT

`kernel/arch/x86_64/cpu/gdt.c`. Segment base/limit are meaningless for
64-bit code/data segments (the CPU ignores them), but the selector
*order* is load-bearing: `SYSCALL`/`SYSRET` (Phase 5) derive CS/SS from a
single base offset in the `STAR` MSR, which only produces the right
selectors if user data sits immediately before user code.

| Selector | Segment | DPL |
|---|---|---|
| `0x00` | Null | — |
| `0x08` | Kernel code | 0 |
| `0x10` | Kernel data | 0 |
| `0x18` | User data | 3 |
| `0x20` | User code | 3 |
| `0x28` | TSS (16 bytes: `0x28`–`0x37`) | 0 |

The TSS's only currently-meaningful field is `rsp0` (the stack the CPU
loads on a ring3→ring0 transition) — unused until Phase 5, but `ltr` has
to load a valid TSS regardless of whether anything transitions privilege
levels yet.

### IDT and interrupt dispatch

`kernel/arch/x86_64/cpu/idt.c` + `isr_stubs.S` + `isr.c`. All 256 gates
are interrupt gates (not trap gates): the CPU clears `IF` on entry, so a
handler can't be interrupted by another before it finishes — there's no
nested-interrupt support yet.

- **Vectors 0–31**: CPU exceptions (Intel SDM Vol. 3A, Table 6-1). Ten of
  them push a hardware error code (8, 10–14, 17, 21, 29, 30); the
  assembly stubs push a dummy 0 for the rest so every exception hits
  `isr_dispatch` with the same frame shape. Unregistered exceptions
  panic — see `panic_exception`.
- **Vectors 32–47**: legacy IRQ lines 0–15, routed through the IOAPIC
  (never the 8259 PIC — see `docs/FAQ.md`). Unregistered IRQs are EOI'd
  and dropped.
- **Vector 0xff**: the LAPIC's spurious-interrupt vector. Its stub
  (`isr_stub_spurious`) is a bare `iretq` — no register save, no EOI
  (SDM Vol. 3A, 11.9 says not to EOI a spurious interrupt).

Every interrupt in this kernel is a same-privilege (ring0→ring0),
no-stack-switch entry — no ring 3 yet, and no IDT gate uses the IST
mechanism — so the CPU always pushes exactly `RIP`/`CS`/`RFLAGS`, never
`RSP`/`SS`. That's what makes one fixed-size `struct interrupt_frame`
(`isr.h`) and one `iretq` path in `isr_common_stub` correct. See
`docs/ROADMAP.md` for why double-fault doesn't get its own IST stack yet
— mixing IST and this fixed-frame assumption without handling both frame
shapes would be a correctness bug, not a simplification.

### LAPIC / IOAPIC

`kernel/arch/x86_64/cpu/lapic.c`, `ioapic.c`. Neither's MMIO is reachable
through HHDM: their physical addresses (`0xfee00000`, `0xfec00000`) don't
appear in the firmware memory map at all (verified against this kernel's
own QEMU/OVMF memmap — they're not DRAM, so there's no entry to map).
Both are mapped with `vmm_map_mmio` (Phase 2's VMM), which is why
`kmain` brings up `pmm_init`/`vmm_init` before `lapic_init`/`ioapic_init`
— an ordering swap from how Phase 1 originally landed this, back when a
minimal standalone page-table editor (since removed) mapped these two
pages before any general-purpose VMM existed.

`ioapic_init` takes its base address as a parameter rather than
hardcoding it; `kmain` supplies whatever `kernel/acpi/madt.c` finds in
the MADT (Phase 3), falling back to the conventional `0xfec00000`
default (correct for QEMU's q35/i440fx and virtually every real
chipset) only if the MADT is unavailable or has no IOAPIC entry.

### Panic

`kernel/arch/x86_64/cpu/panic.c`. Two entry points: `panic_exception`
(full register dump from a trapped `struct interrupt_frame`, plus CR2 on
a page fault) and `panic(fmt, ...)` (a kernel-detected fatal condition
with no register frame). Both print a stack trace by walking the RBP
chain (`-fno-omit-frame-pointer` in the Makefile keeps it walkable) and
halt. No symbol resolution yet — addresses are raw; cross-reference
against `build/quin-kernel.elf` with `addr2line` or gdb.

## Virtual memory (Phase 2)

### Physical memory manager

`kernel/mm/pmm.c`. A bitmap, one bit per 4KiB frame, built once from
`boot_get_memmap()`: every bit starts set (in use), and only frames
inside a `BOOT_MEMMAP_USABLE` region get cleared (free). Anything the
bootloader didn't call out as usable — reserved, ACPI, MMIO gaps, bad
memory — is therefore never handed out, with no separate exclusion list
to maintain.

The bitmap itself is sized off the highest address among only the
RAM-backed memmap types (usable, bootloader-reclaimable, the kernel's
own executable-and-modules region). QEMU's memmap also reports a large
`Reserved` entry for the 64-bit PCIe MMIO window — tens of GiB above any
real RAM even on a small guest — and an earlier version of this code
included that in the sizing calculation, producing a bitmap three orders
of magnitude larger than it needed to be. Worth knowing if you ever see
`pmm_total_frame_count()` return something wildly larger than the
machine's actual RAM: check what memmap type is driving `highest_addr`.

Physical page 0 is permanently reserved (never handed out), so a valid
frame's physical address is never confusable with a null pointer.
Bootloader-reclaimable memory is deliberately *not* reclaimed — Limine's
own page tables and other structures live there, and this kernel keeps
extending those same page tables (see below) rather than building a
fresh address space, so reclaiming them out from under itself would be a
use-after-free. A fork that wants that memory back needs to first copy
out everything it still needs from bootloader-reclaimable regions (the
memmap itself, the RSDP physical pointer — see Phase 3), build an
independent set of page tables, switch `CR3` to them, and only then free
the old ones.

### Virtual memory manager

`kernel/arch/x86_64/mm/vmm.c`. Continues extending the page tables
Limine already built (walked via `CR3` + HHDM) instead of switching to a
fresh address space — those tables already correctly map the kernel
image, HHDM, and the framebuffer, and rebuilding that from scratch would
just be re-deriving information Limine already computed correctly. New
page-table pages come from `pmm_alloc_frame`. Intermediate (non-leaf)
entries are always Present+Writable regardless of what the caller asked
for; only the leaf PTE encodes the caller's actual `VMM_*` flags — the
CPU ANDs permissions across all four levels, so a restrictive
intermediate entry would silently override a more permissive leaf.

Three fixed virtual regions (see the memory layout table above):
`vmm_map_mmio` for fixed-purpose device MMIO, `vmm_alloc_guarded` for
guard-page-bounded eager allocations, and `kernel/mm/heap.c`'s own
16MiB lazy region. All three bump-allocate from their own base address
and never reclaim virtual address space — acceptable for a kernel that
never removes a driver or resizes its heap down, not something to build
on for a use case that does.

### Guard pages

`vmm_alloc_guarded(page_count, flags)` maps `page_count` pages with one
unmapped page immediately before and after the range, so an off-by-one
read or write faults immediately instead of silently corrupting a
neighboring allocation. Verified by deliberately writing one byte past
a 2-page guarded allocation and confirming `panic_exception` fires with
`CR2` pointing exactly at the guard page, before removing the trigger —
same discipline as the Phase 1 panic-path check. Nothing in Phase 2
itself needs a guarded allocation (the heap uses a plain lazy region,
not this), but Phase 4's per-thread kernel stacks are the intended
consumer — stack overflow detection is the classic guard-page use case.

### Page fault handling: real faults vs. lazy mappings

`vmm_handle_page_fault`, registered against IDT vector 14 by
`vmm_init`. Consults error-code bit 0 first (SDM Vol. 3A, 4.7): a set
bit means the page *was* present and this is a permission violation
(writing read-only memory, executing NX memory) — always a real bug,
never something to paper over, so it's treated as an unhandled
exception immediately. A clear bit means not-present, which is only
interesting if the faulting address falls inside a region some
subsystem registered with `vmm_register_lazy_region`: if so, a fresh
frame is allocated and mapped with that region's flags and the faulting
instruction transparently retries; if not, it's a real fault (wild
pointer, stack overflow into unmapped guard space, etc.) and falls
through to `panic_exception`.

`kernel/mm/heap.c` is the only current lazy-region consumer: its 16MiB
virtual range is registered but never eagerly mapped, so the very first
write in `heap_init` (initializing the first free-list block's header)
is what faults in the heap's first physical page, through this exact
path.

### Kernel heap

`kernel/mm/heap.c`. A doubly-linked, first-fit, splitting-and-coalescing
allocator over the lazy heap region above — the classic "block header
before each allocation" design, not a slab or size-class allocator.
O(n) in the number of live blocks, which is the right trade for a
template kernel's allocation traffic against how much simpler it is to
read than a production design.

## Platform services (Phase 3)

### ACPI

`kernel/acpi/acpi.c`. Validates the RSDP, walks to the XSDT (falling
back to the RSDT on the ACPI 1.0 systems this template's QEMU/OVMF
target never actually presents, but the fallback is cheap so it's
there), and exposes `acpi_find_table` by 4-character signature. Every
table's checksum (a sum-of-bytes-equals-zero over the whole table,
including tables found via `acpi_find_table`) is verified before use;
a mismatch panics rather than trusting corrupt firmware data.

This is why `kernel/arch/x86_64/boot/limine_requests.c` requests base
revision 4, not 3 (where Phase 1/2 started): revision 4 is the first
revision that *guarantees* the RSDP and every table it leads to are
actually mapped into HHDM (via ACPI-reclaimable, ACPI-NVS, or the
Reserved-Mapped memmap type) — under revision 3, ACPI tables aren't
reliably reachable through HHDM at all. Revision 4 is a strict superset
of revision 3's guarantees, so the bump didn't require touching
anything Phase 1/2 already built.

`kernel/acpi/madt.c` parses the MADT ("APIC" table) for two things: the
count of enabled Local APIC entries (informational — logged at boot,
not yet used for anything, since SMP is out of scope) and the first
IOAPIC entry's base address. `kmain` passes that address to
`ioapic_init`, falling back to the conventional `0xfec00000` default if
the MADT is missing or has no IOAPIC entry — the same default
`ioapic_init` used to hardcode unconditionally back in Phase 1.

No FADT/DSDT/AML parsing — that's a fundamentally different (and much
larger) undertaking than fixed-layout table parsing; see `docs/ROADMAP.md`.

### LAPIC timer

`kernel/arch/x86_64/cpu/lapic_timer.c` calibrates the LAPIC timer's
actual tick frequency by counting its ticks across a known-length delay
from the legacy PIT — specifically PIT channel 2, gated through port
`0x61`, whose output is directly pollable without needing IRQ0 wired up
at all (unlike channel 0, which only reaches the IOAPIC). Once
calibrated, the timer is reprogrammed for periodic interrupts at
`TIMER_FREQUENCY_HZ` (1000 Hz — `kernel/drivers/timer/timer.h`).

`kernel/drivers/timer/timer.c` is the arch-independent side: it owns the
tick counter and the interrupt handler that increments it, and calls
into `lapic_timer_calibrate_and_start` only to actually program the
hardware. That split mirrors `kernel/include/boot_info.h`'s
arch-independent/arch-specific boundary — a hypothetical aarch64 port
would implement `kernel/arch/aarch64/cpu/generic_timer.c` and leave
`kernel/drivers/timer/timer.c` untouched.

### Leveled logger

`kernel/lib/log.c`. `log_debug`/`log_info`/`log_warn` all funnel through
`log_write`, which writes a `[LEVEL] ` prefix (color-coded on the
framebuffer; serial has no color) followed by the formatted message to
both serial and the framebuffer console — the same dual-sink pattern
`panic.c` uses, just with a level-dependent color instead of always red.
`log_set_min_level` filters below a threshold; defaults to `LOG_LEVEL_DEBUG`
(show everything), appropriate for a template kernel where visibility
into what's happening matters more than quiet logs.

Deliberately not the same code path as `panic()`: a panic needs a
register dump and a guaranteed-halts-after semantics that routine
logging shouldn't carry, so the two stay separate despite the visual
similarity in their `[TAG]` output format.

### PS/2 keyboard

`kernel/drivers/keyboard/keyboard.c`. Reads Scan Code Set 1 (the PS/2
controller's power-on default) from port `0x60` on IRQ1, tracks Shift
state, and translates through a fixed lookup table covering the
alphanumeric block, punctuation, space, enter, tab, and backspace —
not F-keys, the numpad, arrow keys, or the `0xE0`-prefixed extended
scancodes. Translated characters land in a small circular buffer;
`keyboard_read_char` is non-blocking and returns `'\0'` when it's
empty. No 8042 controller initialization sequence — every checked QEMU
machine type has it already enabled by firmware default, and a
from-scratch controller init is a well-scoped addition for a fork
targeting real hardware, not something this template needs to carry.

Verified with QEMU's monitor `sendkey` command (including `shift-1`
producing `!`, confirming the modifier tracking) before removing the
temporary echo-to-serial loop used to check it.

## Scheduling (Phase 4)

### Spinlocks

`kernel/sched/spinlock.c`. A real atomic test-and-test-and-set lock
(`__atomic_exchange_n`/`__atomic_store_n`, with a plain-read inner spin
loop and `pause` to avoid hammering the cache line) — not a `cli`/`sti`
stand-in, even though this kernel only ever runs one CPU today. The
`_irqsave`/`_irqrestore` variants additionally save and restore `RFLAGS`
around a `cli`, so a lock touched from both normal code and an
interrupt handler can't deadlock against its own interrupted owner; the
plain variants are only safe for locks nothing in interrupt context
ever acquires.

### Kernel threads and context switching

`kernel/sched/thread.c` (arch-independent: allocation, the fake initial
stack frame) and `kernel/arch/x86_64/cpu/context_switch.S` (the actual
register swap). A context switch is treated as an unusual function call:
`context_switch(&old->stack_pointer, new->stack_pointer)` pushes the
SysV callee-saved registers onto the outgoing thread's stack, stashes
the resulting `RSP`, loads the incoming thread's stashed `RSP`, pops its
registers, and `ret`s — landing back wherever that thread was when it
was last switched away from.

A thread that has never run yet has no such "last switched away from"
point, so `thread_create` fabricates one: it writes six register slots
and a return address onto a fresh guard-paged stack (`vmm_alloc_guarded`
— Phase 2's guard-page primitive finds its first real consumer here),
with the entry function and its argument placed in the exact slots
`context_switch`'s `pop` sequence loads into `r12`/`r13`, and the return
address pointing at `thread_trampoline`. Trampoline moves `r13` into
`rdi` (the SysV first-argument register) and calls `*r12` — completing
what amounts to a hand-rolled `entry(arg)` call that a plain `ret`
couldn't set up on its own.

**Why `thread_trampoline` calls `sti`, and why that's exactly what
caused (and, once understood, fixed) a real deadlock during Phase 4
development:** interrupt gates clear `IF` on entry, and nothing in this
kernel explicitly restores it *except* the eventual `iretq` at the end
of whichever interrupt frame is on the current stack. A thread resuming
through its own previously-suspended interrupt frame gets `IF` restored
correctly by that `iretq`, later. A thread running for the very first
time has no such frame — without an explicit `sti` in `thread_trampoline`,
it would simply never receive another interrupt, including the timer
tick that's supposed to preempt it.

That fix alone surfaced a second, worse bug: `sti` in `thread_trampoline`
re-enables interrupts *before* the timer interrupt that triggered the
switch has been acknowledged, because `isr_dispatch` used to send EOI
*after* the handler returns — and a handler that context-switches away
doesn't return in the normal sense until, potentially, a long time
later. With EOI still outstanding, the LAPIC withholds every further
tick of that vector, so nothing ever preempts the new thread, nothing
ever switches back to the thread whose stack the deferred EOI call is
buried in, and the machine hangs — permanently, not just slowly. This
is exactly what happened when Phase 4's preemption was first tested
with a thread that deliberately never yields. The fix was moving EOI in
`isr_dispatch` (`kernel/arch/x86_64/cpu/isr.c`) to fire *before* the
handler runs rather than after, which is both correct and, per Intel's
own guidance, the generally preferred ordering anyway (it minimizes
interrupt latency for whatever's next in line). See the comment at that
call site for the full account.

### Round-robin scheduler

`kernel/sched/sched.c`. Threads sit in a circular singly-linked ready
list; `current` points at whichever one is running. `sched_tick`
(called from the timer ISR every tick — `kernel/drivers/timer/timer.c`)
counts ticks since the last switch and preempts to `current->next` once
`SCHED_TIME_SLICE_TICKS` (10, i.e. 10ms at the timer's 1000Hz) have
elapsed; `sched_yield` does the same immediately, for voluntary
preemption points. The ready-list read-modify-write itself is protected
by an `_irqsave` spinlock — necessary even on one core, since
`sched_tick` running the same code from interrupt context is exactly
the hazard those variants exist for.

`sched_init` wraps whatever kmain was doing as thread 0 (`bootstrap_thread`)
so there's always a valid `current` to save into on the very first
switch, without needing `thread_create`'s fake-stack machinery for code
that already has a perfectly good real stack.

Verified two ways: three threads voluntarily round-robining through a
few `sched_yield` calls each (confirming basic switching and argument
passing work), and — separately, to specifically catch the EOI bug
above — a thread that busy-loops for 500 ticks without ever yielding,
alongside a second thread that only makes progress if real preemption
(not cooperative yielding) is actually happening. Both checks' demo
code was removed after confirming the behavior; only the three-thread
round-robin demo ships in `kmain` as a permanent, visible demonstration
of the scheduler skeleton actually working.

## Syscall ABI (Phase 5)

This is where the template ends and your kernel begins. Everything here
is deliberately minimal scaffolding, not a finished syscall ABI — see
`docs/ROADMAP.md`.

### SYSCALL/SYSRET

`kernel/arch/x86_64/cpu/syscall.c` + `syscall_entry.S`. `SYSCALL` and
`SYSRET` derive the selectors they load from a single `STAR` MSR field
each, using fixed offsets from a base value (SDM Vol. 2B) — which is
exactly why `kernel/arch/x86_64/cpu/gdt.c` orders the GDT the way it
does (see "GDT" above): kernel code/data adjacent for `SYSCALL`'s
formula, user data immediately before user code for `SYSRET`'s.

Unlike an interrupt gate, `SYSCALL` doesn't consult the TSS or change
`RSP` at all — landing in `syscall_entry` means `%rsp` still points at
whatever user stack was live when `syscall` executed, so the entry
stub's first job is getting off of it onto a known-good kernel stack
(a global, set once by `syscall_init`) before touching memory for
anything else. `IA32_FMASK` clears `IF` and `TF` on entry, closing the
window between that instruction and the stack swap where an interrupt
landing on the still-user stack would be dangerous.

The dispatcher (`syscall_dispatch`) reads a call number out of `%rax`
and returns `-ENOSYS` unconditionally — there is no syscall table, no
argument-marshaling convention beyond what the CPU itself defines
(arguments would conventionally arrive in `%rdi`/`%rsi`/`%rdx`/`%r10`/
`%r8`/`%r9`, following the same convention Linux uses and for the same
reason: `%rcx` is unavailable, since `SYSCALL` itself clobbers it), and
no permission model. Designing that is the actual work of building a
kernel on this template, not something the template can decide for you.

### Jumping to ring 3

`jump_to_ring3` (`syscall_entry.S`) builds a five-item `iretq` frame by
hand (`SS`, `RSP`, `RFLAGS`, `CS`, `RIP`, pushed in that order) and
`iretq`s — the standard technique for an *initial* transition into a
lower privilege level, as opposed to `SYSRET`, which only makes sense
returning from a `SYSCALL` that's already in flight (it expects its
return context in `%rcx`/`%r11`, which nothing has set up yet on a
first jump).

This relies on a detail that's easy to get backwards: `iretq` decides
*at execution time*, from the CS value it's about to load, whether to
also pop `RSP`/`SS` — it's not something the interrupt/exception stub
needs to branch on in software. That means `isr_common_stub`
(`kernel/arch/x86_64/cpu/isr_stubs.S`), written back in Phase 1 for an
exclusively-ring-0 kernel, already handles a ring-3-originated exception
correctly without any changes: the CPU pushes the extra two items on
its own, and `iretq` pops exactly what was pushed. The one piece that
*did* need attention was `TSS.RSP0` (Phase 1's GDT/TSS work left it
unset, since nothing needed it yet) — a ring3→ring0 transition through
any interrupt gate loads `RSP` from it automatically (SDM Vol. 3A, 8.5),
so `syscall_init` allocates a dedicated stack and calls
`gdt_set_kernel_stack` before anything can reach ring 3. It's shared
with the `SYSCALL` path's own kernel stack; safe as long as at most one
ring-3 excursion is ever in flight, true for this template's one demo
thread and not something to rely on with more than one — a fork adding
real userspace processes needs a kernel stack per thread here, not one
global.

The demo thread in `kmain` maps one user-accessible code page and one
stack page — directly writable from kernel code, since this template
has no per-process address spaces yet, just one shared set of page
tables everything (kernel and this one "user" mapping alike) lives in —
writes a 4-byte program (`syscall; jmp $`), and jumps to it. The
`syscall_dispatch` log line is the proof the full round trip actually
happened. Verified further during development, temporarily, by swapping
the trailing `jmp $` for `int3`: rather than reaching the `#BP` handler,
it produced a clean `#GP` (error code identifying IDT vector 3) with
`CS=0x0023` in the panic dump. That's correct, not a bug — `int3` and
`into`, despite being hardware exceptions, are *software-triggered* (a
dedicated one-byte opcode) and so are subject to the normal CPL-vs-gate-DPL
check like any `int n`, unlike a genuinely hardware-generated exception
(`#PF`, `#GP` itself, `#UD`, ...), which always gets through regardless
of CPL. Since every IDT gate here has DPL 0 (`kernel/arch/x86_64/cpu/idt.c`),
ring-3 code can't invoke `int3` directly — which is exactly what
happened, and the resulting `#GP` still confirmed the real thing this
was testing: a ring-3-originated exception's five-item frame unwinds
correctly.

A real bug *did* surface building this demo, in code that had shipped
two phases earlier: `kernel/arch/x86_64/mm/vmm.c`'s `ensure_next_level`
created new intermediate page-table entries as Present+Writable only,
never Present+Writable+**User**. Since x86 ANDs permissions across all
four paging levels, every mapping this kernel had ever made was
*already* effectively supervisor-only regardless of what a leaf PTE
said — invisible until Phase 5 tried to execute from a page whose leaf
genuinely did say `User=1`, and got a protection fault anyway. Fixed by
always setting `User` on intermediate entries too, same reasoning as
the pre-existing Writable choice: a leaf's own flags are what should
actually govern access, not an accidentally-more-restrictive ancestor.

## Testing (Phase 6)

Two layers, deliberately not one:

- **`tests/unit/`** — host-side, no QEMU, no cross toolchain, just the
  host's own `cc`. Only code with zero freestanding-specific
  dependencies can live here, which in practice means it has to be
  extracted first: `kernel/mm/pmm.c`'s bit-twiddling used to be inline
  static functions inside that file, coupled to `boot_get_memmap` and
  physical-address bookkeeping that has no meaning outside a booted
  kernel. Pulling the actual bitmap operations out into
  `kernel/lib/bitmap.c` — which depends on nothing but `<stdint.h>`/
  `<stdbool.h>`, identical on the host and the freestanding target —
  is what makes `tests/unit/test_bitmap.c` possible at all; `pmm.c` is
  now a thin (and not independently host-testable) integration layer
  on top of it. That's the pattern for adding more host-side coverage
  later: find the pure-logic core, give it zero kernel-specific
  dependencies, test *that*, directly.
- **`scripts/run.sh test`** — the QEMU integration smoke test: boots
  the real ISO headlessly, asserts the banner appears on serial, and
  checks the `isa-debug-exit` exit code. This is what every phase in
  this document was actually verified against, often via a temporary,
  deliberately-provoked failure (a guard-page overrun, a busy-looping
  thread, `int3` from ring 3, ...) added, checked, and removed —
  documented here and in `docs/ROADMAP.md` rather than left in `kmain`
  permanently. Both real bugs this template shipped with (the PMM
  bitmap sizing in Phase 2, the missing intermediate-entry User bit in
  Phase 5) were caught exactly this way, not by either test layer
  running unattended — worth keeping in mind before trusting either
  layer alone to catch the next one.
