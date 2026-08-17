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

The exact section layout is defined in `kernel/arch/x86_64/linker.ld`;
`__kernel_start`/`__kernel_end` there bound the whole loaded image. Every
`PT_LOAD` segment is either read+execute or read+write, never both — see
the `PHDRS` block — matching the W^X discipline the kernel's own page
tables will enforce once `kernel/arch/x86_64/mm` (Phase 2) maps things
itself instead of relying on Limine's initial mappings.

The HHDM row's exact base isn't fixed: base revision 3 (what this kernel
requests — see `kernel/arch/x86_64/boot/limine_requests.c`) only maps
usable, bootloader-reclaimable, executable-and-modules, and framebuffer
memory-map regions into it, and the base address itself is chosen by the
bootloader per boot. Code must read it from the HHDM feature response
rather than assume a fixed offset (this lands in Phase 2, when `kernel/mm`
starts using it for physical-to-virtual translation).

## CPU state (Phase 1)

Not yet implemented — see `docs/ROADMAP.md`.

## Virtual memory (Phase 2)

Not yet implemented — see `docs/ROADMAP.md`.

## Platform services (Phase 3)

Not yet implemented — see `docs/ROADMAP.md`.

## Scheduling (Phase 4)

Not yet implemented — see `docs/ROADMAP.md`.

## Syscall ABI (Phase 5)

Not yet implemented — see `docs/ROADMAP.md`.
