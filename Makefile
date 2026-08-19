# Quin Kernel Template top-level build.
#
# Compiles the kernel with Clang targeting a bare x86_64 ELF (no cross
# GCC toolchain -- see docs/FAQ.md for why) and links it with LLD against
# kernel/arch/x86_64/linker.ld. `make iso` then assembles a UEFI-bootable
# ISO using the prebuilt Limine bootloader binaries (fetched on first use
# from a pinned release, see the .cache/limine-binary rule below) and
# xorriso. See scripts/build.sh for the human-facing wrapper.

KERNEL := build/quin-kernel.elf
ISO := build/quin-kernel.iso

CC := clang
LD := ld.lld

# On macOS, Homebrew's llvm formula is keg-only -- deliberately not
# linked onto PATH, so it doesn't shadow Xcode's own clang -- which
# means the plain `clang`/`ld.lld` above would silently resolve to
# Apple's clang (no x86_64-none-elf freestanding target support) instead
# of the one scripts/setup-toolchain.sh just installed. Point CC/LD at
# Homebrew's copy explicitly when it's present; Linux's apt-installed
# clang/lld are already directly on PATH, so this is a no-op there.
ifeq ($(shell uname -s),Darwin)
    BREW_LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null)
    ifneq ($(BREW_LLVM_PREFIX),)
        CC := $(BREW_LLVM_PREFIX)/bin/clang
        LD := $(BREW_LLVM_PREFIX)/bin/ld.lld
    endif
endif

TARGET := x86_64-none-elf

LIMINE_VERSION := v12.5.2
LIMINE_BINARY_DIR := .cache/limine-binary
LIMINE_BINARY_URL := https://github.com/Limine-Bootloader/Limine/releases/download/$(LIMINE_VERSION)/limine-binary.tar.gz

# The exact, non-negotiable kernel compile flags this template is built
# around (see the project brief / docs/ARCHITECTURE.md): no red zone or
# stack protector (nothing to unwind to or guard against in ring 0 yet),
# no SSE/MMX/x87 (no FPU state save/restore across context switches --
# see docs/ROADMAP.md), position-dependent kernel-model code since we
# link at a fixed higher-half address rather than supporting a slide.
CFLAGS := \
	-target $(TARGET) \
	-march=x86-64 \
	-std=c17 \
	-O2 -g \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-pic \
	-fno-pie \
	-mno-red-zone \
	-mcmodel=kernel \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-fno-omit-frame-pointer \
	-Wall -Wextra -Werror -Wshadow -Wconversion

# The panic handler's stack trace (kernel/arch/x86_64/cpu/panic.c) walks
# the RBP chain, which only exists if frame pointers aren't optimized
# away -- not part of the flag list above, but required for that Phase 1
# feature to work at all.
ASFLAGS := \
	-target $(TARGET) \
	-march=x86-64 \
	-Wall -Werror

# -nostdinc keeps the host's glibc headers (/usr/include) completely out
# of reach -- without it, a plain #include "string.h" from a subdirectory
# silently resolves to the host's string.h instead of kernel/lib/string.h,
# and you get link errors or, worse, mismatched signatures with no
# warning. The clang resource-dir include is added back explicitly: it
# holds only the target-independent freestanding headers (stdint.h,
# stddef.h, stdbool.h, stdarg.h, float.h, limits.h) that the C standard
# guarantees are available even hosted-OS-free.
CLANG_RESOURCE_DIR := $(shell $(CC) -target $(TARGET) -print-resource-dir)

CPPFLAGS := \
	-nostdinc \
	-isystem $(CLANG_RESOURCE_DIR)/include \
	-Ikernel \
	-Ikernel/include \
	-Ithird_party/limine/include \
	-MMD -MP

LDFLAGS := \
	-T kernel/arch/x86_64/linker.ld \
	-z max-page-size=0x1000 \
	--no-dynamic-linker \
	-nostdlib

CSRCS := $(shell find kernel -name '*.c' | LC_ALL=C sort)
ASSRCS := $(shell find kernel -name '*.S' | LC_ALL=C sort)
OBJS := $(patsubst kernel/%.c,build/kernel-obj/%.o,$(CSRCS)) \
        $(patsubst kernel/%.S,build/kernel-obj/%.o,$(ASSRCS))
DEPS := $(patsubst kernel/%.c,build/kernel-obj/%.d,$(CSRCS))

.PHONY: all
all: $(KERNEL)

$(KERNEL): $(OBJS) kernel/arch/x86_64/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

build/kernel-obj/%.o: kernel/%.c submodules
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MJ $@.json -c $< -o $@

build/kernel-obj/%.o: kernel/%.S submodules
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c $< -o $@

-include $(DEPS)

# One -MJ fragment per translation unit, concatenated into a single
# compilation database. clang-tidy (lint.yml) and .vscode/c_cpp_properties.json
# both read this instead of guessing kernel/'s freestanding flags themselves.
compile_commands.json: $(OBJS)
	@echo "[" > $@
	@find build/kernel-obj -name '*.o.json' | LC_ALL=C sort | xargs cat | sed '$$s/,$$//' >> $@
	@echo "]" >> $@

.PHONY: iso
iso: $(ISO)

$(ISO): $(KERNEL) $(LIMINE_BINARY_DIR)/limine-uefi-cd.bin limine.conf
	rm -rf build/iso_root
	mkdir -p build/iso_root/boot/limine
	mkdir -p build/iso_root/EFI/BOOT
	cp $(KERNEL) build/iso_root/boot/quin-kernel
	cp limine.conf build/iso_root/boot/limine/limine.conf
	cp $(LIMINE_BINARY_DIR)/limine-uefi-cd.bin build/iso_root/boot/limine/
	cp $(LIMINE_BINARY_DIR)/BOOTX64.EFI build/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		build/iso_root -o $(ISO) >/dev/null
	rm -rf build/iso_root

# Limine ships prebuilt UEFI bootloader binaries as GitHub release assets
# rather than in a git-trackable form for this version line (see
# docs/ARCHITECTURE.md, "Toolchain provenance", for why third_party/limine
# is a submodule of limine-protocol -- the header -- while this is a
# pinned download instead). Cached under .cache/, not build/, so
# `make clean` doesn't force a re-download every time.
$(LIMINE_BINARY_DIR)/limine-uefi-cd.bin:
	@mkdir -p .cache
	curl -Lo .cache/limine-binary.tar.gz $(LIMINE_BINARY_URL)
	tar -xzf .cache/limine-binary.tar.gz -C .cache
	rm -f .cache/limine-binary.tar.gz

.PHONY: submodules
submodules:
	@test -f third_party/limine/include/limine.h || git submodule update --init --recursive

.PHONY: clean
clean:
	rm -rf build

.PHONY: distclean
distclean: clean
	rm -rf .cache
