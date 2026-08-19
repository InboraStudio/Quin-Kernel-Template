#!/usr/bin/env bash
#
# Boots build/quin-kernel.iso in QEMU.
#
#   scripts/run.sh         interactive: graphical window, serial mirrored
#                           to this terminal, stays running until you quit
#                           QEMU (Ctrl-A X in the serial console, or close
#                           the window).
#   scripts/run.sh test    headless smoke test: no display, isa-debug-exit
#                           attached, asserts the boot banner appears on
#                           serial and QEMU exits with the expected code.
#                           This is what CI runs (.github/workflows/build.yml).

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"
cd "$(quin_repo_root)"

MODE="${1:-interactive}"

./scripts/build.sh

OVMF_CODE="$(quin_find_ovmf_code)" || {
    printf '[run] could not find OVMF firmware. Run ./scripts/setup-toolchain.sh.\n' >&2
    exit 1
}
OVMF_VARS_SRC="$(quin_find_ovmf_vars)" || {
    printf '[run] could not find OVMF firmware. Run ./scripts/setup-toolchain.sh.\n' >&2
    exit 1
}

# OVMF writes NVRAM variables (boot order, etc.) into the vars file, so
# each run gets its own writable copy instead of mutating the one
# scripts/setup-toolchain.sh installed system-wide.
mkdir -p build
OVMF_VARS="build/ovmf-vars.fd"
cp "$OVMF_VARS_SRC" "$OVMF_VARS"

COMMON_ARGS=(
    -M q35
    -m 256M
    -drive "if=pflash,format=raw,unit=0,file=$OVMF_CODE,readonly=on"
    -drive "if=pflash,format=raw,unit=1,file=$OVMF_VARS"
    -cdrom build/quin-kernel.iso
    -no-reboot
)

if [[ "$MODE" == "test" ]]; then
    exec ./tests/integration/smoke_test.sh "$OVMF_CODE" "$OVMF_VARS" build/quin-kernel.iso
else
    exec qemu-system-x86_64 \
        "${COMMON_ARGS[@]}" \
        -serial stdio
fi
