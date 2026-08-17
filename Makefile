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
	-Wall -Wextra -Werror -Wshadow -Wconversion

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

SRCS := $(shell find kernel -name '*.c' | LC_ALL=C sort)
OBJS := $(patsubst kernel/%.c,build/kernel-obj/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all
all: $(KERNEL)

$(KERNEL): $(OBJS) kernel/arch/x86_64/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

build/kernel-obj/%.o: kernel/%.c submodules
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MJ $@.json -c $< -o $@

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
