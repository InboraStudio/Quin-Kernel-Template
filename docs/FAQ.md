# FAQ

## Why Limine instead of GRUB / Multiboot2?

Multiboot2 + GRUB works, but it means owning a BIOS or a chainloaded UEFI
path, and Multiboot2's memory map and module-passing conventions show their
age. Limine speaks native UEFI, hands you a higher-half mapping and an HHDM
(higher-half direct map) for physical memory out of the box, and its request
protocol (a table of tagged structs the kernel exposes via linker sections)
is simpler to extend than Multiboot2 tags. It's also actively maintained and
widely used across the current OSDev community, so the questions you'll hit
are ones other people have already asked in the same place you'll be asking
them.

The tradeoff: Limine is one specific bootloader's protocol, not a
cross-bootloader standard. That's fine here — Quin picks one boot path and
does it well rather than supporting several halfway.

## Why Clang + LLD instead of a GCC cross-compiler?

A GCC cross-compiler for `x86_64-elf` has to be built from source — there's
no `apt`/`brew` package for it on most distros, and building binutils + GCC
from source takes real time and a working host toolchain. Clang ships as a
single compiler that targets *any* architecture out of the box via
`-target x86_64-none-elf` — no cross-build step, no version-matching
binutils separately. That's what makes `scripts/setup-toolchain.sh` a
five-minute package-manager install on Linux, macOS, and WSL2 instead of a
multi-hour build.

The tradeoff: a handful of GCC-specific extensions and inline-asm idioms
you'll find in older OSDev tutorials don't apply here. Everything in this
template is written against Clang; if you're following a GCC-based tutorial
alongside this template, expect small syntax differences in inline asm
constraints and attribute spelling.

## Why QEMU only? What about real hardware?

Real hardware means real firmware quirks, real ACPI table inconsistencies,
and no way to run it in CI. QEMU + OVMF gives a consistent, scriptable,
reproducible target: the same boot behavior on every contributor's machine
and in GitHub Actions, and the `isa-debug-exit` device lets CI detect a
successful boot without a human watching a screen. Every driver in this
template is written against what QEMU emulates.

Nothing stops you from later chasing real-hardware compatibility in your own
fork — but that's a different, harder project (see `docs/ROADMAP.md` for why
it's explicitly out of scope here), and you'll want a solid QEMU-verified
baseline before you start debugging on physical firmware.

## Why is there no filesystem yet?

A filesystem needs a block device driver, a VFS layer, and a decision about
on-disk format — that's a substantial subsystem on its own, and every choice
you'd make (ext2? a custom format? read-only vs. read-write?) is exactly the
kind of decision this template deliberately leaves to you. Quin hands you
what the kernel needs to boot and run (itself, and whatever modules you list)
via Limine's module-loading mechanism, which doesn't require a filesystem
driver at all. See `docs/ROADMAP.md` for the explicit scope boundary.

## Why no legacy 8259 PIC support, not even to mask it?

The PIC and IOAPIC/LAPIC are two different interrupt-routing models, and
mixing them (programming both, or masking the PIC "just in case") is a
classic source of spurious-interrupt bugs early in a kernel's life. Since
Limine boots UEFI-only and every target is QEMU with an IOAPIC present, there's
no scenario in this template where the PIC path is reachable. Phase 1 routes
IRQs through the IOAPIC/LAPIC from the start rather than doing the PIC path
"first" and migrating later.

## Why Make instead of CMake/Meson/xmake?

This template is meant to be readable by someone who has never touched
kernel code, and Make is the one build tool every OSDev Wiki tutorial,
forum post, and reference kernel (xv6, and most of the hobby-OS ecosystem)
already assumes. A generated-build-system layer on top would be one more
thing to learn before you can see what the compiler is actually being
invoked with. The Makefiles here are meant to be read top to bottom, not
treated as a black box.

## Why higher-half at `-2GiB` instead of some other split?

`0xffffffff80000000` (the top 2GiB of the 64-bit address space) is the
conventional x86_64 higher-half kernel base — it's small enough to address
with `-mcmodel=kernel` (which assumes symbols fit in the top or bottom 2GiB)
and it's the same convention Linux and most hobby 64-bit kernels use, so
existing documentation and tooling assumptions carry over. See
`docs/ARCHITECTURE.md` for the full virtual memory layout.
