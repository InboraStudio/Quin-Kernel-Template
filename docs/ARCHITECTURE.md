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

The IOAPIC's base address is currently hardcoded to the conventional
`0xfec00000` default every common chipset (including QEMU's q35/i440fx)
uses. A system with a non-default placement needs the ACPI MADT, which
is Phase 3 — `ioapic_init` documents this as a known simplification, not
an oversight.

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

Not yet implemented — see `docs/ROADMAP.md`.

## Scheduling (Phase 4)

Not yet implemented — see `docs/ROADMAP.md`.

## Syscall ABI (Phase 5)

Not yet implemented — see `docs/ROADMAP.md`.
