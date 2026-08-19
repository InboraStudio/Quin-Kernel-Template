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

## Quick start

```bash
git clone --recursive https://github.com/InboraStudio/Quin-Kernel-Template.git
cd quin-kernel-template
./scripts/setup-toolchain.sh
./scripts/build.sh && ./scripts/run.sh
```

Linux and macOS are supported natively; on Windows, run this inside WSL2 —
`setup-toolchain.sh` will tell you so if you run it on native Windows.

Already cloned without `--recursive`? Run `git submodule update --init` to
fetch the pinned Limine submodule before building.

## What's actually working

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the authoritative, kept-current
list — implemented vs. scaffolded vs. explicitly out of scope. Summary:

- [x] UEFI boot via Limine, higher-half kernel, serial + framebuffer console
- [x] GDT/TSS, full IDT (32 exception vectors), IOAPIC/LAPIC interrupt routing
- [x] Panic handler with register dump and stack trace
- [x] Bitmap physical memory manager, 4-level paging VMM, kernel heap
- [x] Page fault handler with lazy-mapping support
- [x] ACPI parsing (RSDP → XSDT → MADT), calibrated LAPIC timer
- [x] Leveled logger (serial + framebuffer), PS/2 keyboard driver
- [x] Kernel threads, round-robin preemptive scheduler, spinlocks
- [x] `syscall`/`sysret` entry stub and ring-3 jump path (intentionally minimal — this is where the template ends and your kernel begins)
- [ ] SMP, a real syscall ABI, a filesystem — explicitly out of scope, see the roadmap

## Why these choices

`docs/FAQ.md` has the full reasoning; short version: Clang/LLD instead of a
GCC cross-compiler so setup is a package-manager install on every host OS,
Limine instead of GRUB/Multiboot2 for a modern UEFI-native boot protocol,
QEMU-only so every contributor and CI run sees identical, reproducible
hardware, and plain Make because it's the one build tool every OSDev
reference already assumes.

## Documentation

| Doc | What's in it |
|---|---|
| [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) | Build/run/debug workflow, directory tour |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Memory layout, boot sequence, subsystem design |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | What's implemented, scaffolded, or out of scope |
| [`docs/CODING_STYLE.md`](docs/CODING_STYLE.md) | Naming, comment conventions, error-handling rules |
| [`docs/FAQ.md`](docs/FAQ.md) | Why Limine, why Clang, why QEMU-only, why no filesystem |

## License

MIT — see [`LICENSE`](LICENSE). Limine itself is 0BSD; see
`third_party/limine/LICENSE`.
