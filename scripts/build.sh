#!/usr/bin/env bash
#
# Builds the kernel and assembles build/quin-kernel.iso. Thin wrapper
# around `make iso` that also checks the toolchain is actually present
# first, so a missing dependency fails with a clear message instead of a
# confusing compiler error three lines in.
#
#   scripts/build.sh              build/quin-kernel.iso (the default)
#   scripts/build.sh fmt-check    clang-format --dry-run --Werror over
#                                 kernel/ and tests/ -- what lint.yml runs
#   scripts/build.sh fmt          the same files, clang-format -i (fixes in place)

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"
cd "$(quin_repo_root)"

MODE="${1:-build}"

if [[ "$MODE" == "fmt-check" || "$MODE" == "fmt" ]]; then
    command -v clang-format >/dev/null 2>&1 || {
        printf '[build] clang-format not found. Run ./scripts/setup-toolchain.sh first\n' >&2
        printf '[build] (on macOS: %s/bin/clang-format, or add it to PATH).\n' \
            "$(brew --prefix llvm 2>/dev/null || echo '$(brew --prefix llvm)')" >&2
        exit 1
    }

    mapfile -t files < <(find kernel tests -name '*.c' -o -name '*.h')
    if [[ "$MODE" == "fmt-check" ]]; then
        exec clang-format --dry-run --Werror "${files[@]}"
    else
        clang-format -i "${files[@]}"
        printf '[build] formatted %d files\n' "${#files[@]}"
        exit 0
    fi
fi

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
