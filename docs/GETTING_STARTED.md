# Getting Started

## Prerequisites

Linux (apt-based) or macOS with Homebrew, natively. On Windows, use WSL2
— open a WSL2 shell and follow the Linux instructions there; native
Windows isn't supported (see `docs/FAQ.md`).

```bash
git clone --recursive https://github.com/InboraStudio/Quin-Kernel-Template.git
cd quin-kernel-template
./scripts/setup-toolchain.sh
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

## Build, run, debug

```bash
./scripts/build.sh          # compiles the kernel, assembles build/quin-kernel.iso
./scripts/run.sh             # boots it in QEMU with a graphical window
./scripts/run.sh test        # headless boot + isa-debug-exit smoke test (what CI runs)
./scripts/debug.sh           # boots QEMU paused, attaches a terminal gdb session
./scripts/clean.sh           # removes build/
./scripts/clean.sh --all     # also drops the cached Limine binary download
make -C tests/unit           # host-side unit tests -- no QEMU, no cross toolchain, just cc
./scripts/build.sh fmt-check # clang-format --dry-run (what lint.yml runs)
./scripts/build.sh fmt       # clang-format -i -- fixes formatting in place
```

`scripts/run.sh` (no `test` argument) opens a QEMU window and mirrors
serial output to your terminal. Quit with the QEMU window's close button,
or `Ctrl-A X` if your terminal has focus.

You can also drive the build directly through `make` — the scripts are
thin wrappers that add toolchain checks and OVMF path detection:

```bash
make            # build/quin-kernel.elf
make iso        # build/quin-kernel.iso
make clean
make distclean  # also removes .cache/ (the downloaded Limine binaries)
```

## VS Code

Open the repo folder; `.vscode/tasks.json` provides build/run/clean tasks
(`Ctrl+Shift+B` for build), and `.vscode/launch.json` attaches VS Code's
debugger to a QEMU instance started paused with a GDB stub — press F5. If
IntelliSense looks wrong (unresolved includes, wrong defines), run the
"quin: compile_commands.json" task once; `.vscode/c_cpp_properties.json`
reads that file for the exact flags each translation unit was compiled
with.

No local VS Code setup? `.devcontainer/` has a Dockerfile with the whole
toolchain baked in — open the folder in a devcontainer (VS Code's "Reopen
in Container", or any devcontainer-compatible tool) and everything above
just works.

## Directory tour

| Path | What's there |
|---|---|
| `kernel/arch/x86_64/boot/` | Limine request structs, the ELF entry point, the linker script |
| `kernel/arch/x86_64/cpu/` | GDT/TSS/IDT, port I/O helpers, low-level CPU bring-up |
| `kernel/arch/x86_64/mm/` | Arch-specific paging (4-level page tables, higher-half setup) |
| `kernel/mm/` | Arch-independent memory management: physical allocator, VMM, heap |
| `kernel/drivers/` | Serial, framebuffer, keyboard, timer |
| `kernel/acpi/` | ACPI table parsing |
| `kernel/sched/` | Scheduler, threads, spinlocks |
| `kernel/lib/` | Freestanding subset of libc actually used: string ops, printf, logger |
| `kernel/include/` | Cross-cutting headers shared across subsystems |
| `tests/unit/` | Host-side unit tests (no QEMU) for architecture-independent logic |
| `tests/integration/` | The QEMU boot smoke test `scripts/run.sh test` runs |
| `docs/` | This file, `ARCHITECTURE.md`, `ROADMAP.md`, `CODING_STYLE.md`, `FAQ.md` |

Module headers live next to their `.c` file (e.g. `kernel/drivers/serial/serial.h`
alongside `serial.c`); `kernel/include/` is only for headers genuinely
shared across subsystem boundaries (`kernel.h`, `boot_info.h`).

No separate `kernel/syscall/`: the `SYSCALL`/`SYSRET` entry stub and the
ring-3 jump path live in `kernel/arch/x86_64/cpu/syscall.c` +
`syscall_entry.S` instead. Unlike the timer or keyboard, there's no
meaningful arch-independent "syscall" concept to split out yet — the
entry mechanism itself is a different CPU instruction on every
architecture (`SYSCALL` here, `SVC` on aarch64, `ECALL` on RISC-V), and
there's no syscall table or dispatch policy on top of it yet for an
arch-independent layer to own. See `docs/ARCHITECTURE.md`, "Syscall
ABI (Phase 5)".

## If something goes wrong

- **Build fails with a missing tool**: `./scripts/setup-toolchain.sh`,
  then re-run.
- **QEMU can't find OVMF**: `scripts/_common.sh`'s `quin_find_ovmf_code`/
  `quin_find_ovmf_vars` check a few known install paths; if yours isn't
  one of them, open an issue with your OS and how you installed OVMF.
- **Nothing prints and QEMU just sits there**: check you're not stuck at
  the Limine boot menu — `limine.conf` sets `timeout: 0` so it should
  auto-boot instantly, but a corrupted ISO (stale `build/` from an
  interrupted build) can confuse this. Try `./scripts/clean.sh && ./scripts/build.sh`.
- Anything else: open an issue with the bug report template
  (`.github/ISSUE_TEMPLATE/bug_report.md`) — it asks for exactly the
  version/log info needed to reproduce.
