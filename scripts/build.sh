#!/usr/bin/env bash
#
# Builds the kernel and assembles build/quin-kernel.iso. Thin wrapper
# around `make iso` that also checks the toolchain is actually present
# first, so a missing dependency fails with a clear message instead of a
# confusing compiler error three lines in.

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"
cd "$(quin_repo_root)"

missing=()
for tool in clang ld.lld xorriso curl make; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if [[ ${#missing[@]} -gt 0 ]]; then
    printf '[build] missing tools: %s\n' "${missing[*]}" >&2
    printf '[build] run ./scripts/setup-toolchain.sh first.\n' >&2
    exit 1
fi

if [[ ! -f third_party/limine/include/limine.h ]]; then
    printf '[build] fetching git submodules...\n'
    git submodule update --init --recursive
fi

make iso

printf '[build] done: build/quin-kernel.iso\n'
