#!/usr/bin/env bash
#
# Starts QEMU paused (-S) with a GDB stub on :1234 (-s).
#
#   scripts/debug.sh              starts QEMU, then attaches a terminal
#                                  gdb session with symbols loaded and a
#                                  breakpoint on kernel_entry.
#   scripts/debug.sh --qemu-only  starts QEMU in the background and
#                                  returns immediately, without attaching
#                                  gdb. This is what .vscode/tasks.json's
#                                  preLaunchTask uses -- .vscode/launch.json
#                                  then attaches VS Code's own debugger to
#                                  the same :1234 stub instead of a
#                                  terminal one.

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"
cd "$(quin_repo_root)"

QEMU_ONLY=0
[[ "${1:-}" == "--qemu-only" ]] && QEMU_ONLY=1

if [[ ! -f build/quin-kernel.iso ]]; then
    printf '[debug] build/quin-kernel.iso not found, building it first.\n'
    ./scripts/build.sh
fi

OVMF_CODE="$(quin_find_ovmf_code)" || {
    printf '[debug] could not find OVMF firmware. Run ./scripts/setup-toolchain.sh.\n' >&2
    exit 1
}
OVMF_VARS_SRC="$(quin_find_ovmf_vars)" || {
    printf '[debug] could not find OVMF firmware. Run ./scripts/setup-toolchain.sh.\n' >&2
    exit 1
}

mkdir -p build
OVMF_VARS="build/ovmf-vars.fd"
cp "$OVMF_VARS_SRC" "$OVMF_VARS"

SERIAL_LOG="build/debug-serial.log"

qemu-system-x86_64 \
    -M q35 \
    -m 256M \
    -drive "if=pflash,format=raw,unit=0,file=$OVMF_CODE,readonly=on" \
    -drive "if=pflash,format=raw,unit=1,file=$OVMF_VARS" \
    -cdrom build/quin-kernel.iso \
    -no-reboot \
    -serial "file:$SERIAL_LOG" \
    -s -S &
QEMU_PID=$!

printf '[debug] QEMU started (pid %s), paused, gdb stub on :1234.\n' "$QEMU_PID"
printf '[debug] serial output is being logged to %s\n' "$SERIAL_LOG"

if [[ $QEMU_ONLY -eq 1 ]]; then
    disown "$QEMU_PID"
    exit 0
fi

trap 'kill "$QEMU_PID" 2>/dev/null || true' EXIT

command -v gdb >/dev/null 2>&1 || {
    printf '[debug] gdb not found. Run ./scripts/setup-toolchain.sh.\n' >&2
    exit 1
}

gdb build/quin-kernel.elf \
    -ex "target remote :1234" \
    -ex "break kernel_entry" \
    -ex "continue"
