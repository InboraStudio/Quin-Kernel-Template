#!/usr/bin/env bash
#
# QEMU integration smoke test: boots a Quin ISO headlessly with
# isa-debug-exit attached, and asserts the boot banner appears on
# serial and QEMU exits with the expected code. This is the only
# integration test this template ships with -- everything phase-specific
# (guard pages, preemption, the ring-3 round trip, ...) was verified the
# same way during development, temporarily, and is documented rather
# than automated; see docs/ARCHITECTURE.md, "Testing".
#
# Called by scripts/run.sh test (and so, transitively, CI) rather than
# invoked directly -- it assumes the ISO is already built and takes
# OVMF's paths as arguments instead of duplicating scripts/_common.sh's
# detection logic.
#
# Usage: tests/integration/smoke_test.sh <ovmf_code> <ovmf_vars> <iso_path>

set -euo pipefail

OVMF_CODE="$1"
OVMF_VARS="$2"
ISO="$3"

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

set +e
timeout 30s qemu-system-x86_64 \
    -M q35 \
    -m 256M \
    -drive "if=pflash,format=raw,unit=0,file=$OVMF_CODE,readonly=on" \
    -drive "if=pflash,format=raw,unit=1,file=$OVMF_VARS" \
    -cdrom "$ISO" \
    -no-reboot \
    -display none \
    -serial "file:$LOG" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04
STATUS=$?
set -e

printf '[smoke-test] --- serial output ---\n'
cat "$LOG"
printf '[smoke-test] --- end serial output ---\n'

if [[ $STATUS -ne 33 ]]; then
    printf '[smoke-test] FAIL: expected QEMU exit code 33 (isa-debug-exit success), got %s\n' "$STATUS" >&2
    exit 1
fi

if ! grep -q "Quin Kernel Template" "$LOG"; then
    printf '[smoke-test] FAIL: boot banner not found in serial output\n' >&2
    exit 1
fi

printf '[smoke-test] PASS: kernel booted and reported success via isa-debug-exit\n'
