#!/usr/bin/env bash
#
# Installs everything scripts/build.sh and scripts/run.sh need: Clang/LLD
# (the whole point of not using a GCC cross-compiler -- see docs/FAQ.md),
# xorriso (ISO assembly), QEMU, OVMF (UEFI firmware for QEMU), gdb, and make.
#
# Linux: apt. macOS: brew. Native Windows: not supported -- run this
# inside WSL2 instead, same as the rest of the template.

set -euo pipefail

info() { printf '[setup] %s\n' "$1"; }
fail() {
    printf '[setup] error: %s\n' "$*" >&2
    exit 1
}

if [[ "$(uname -s)" == "Linux" ]] && grep -qi microsoft /proc/version 2>/dev/null; then
    info "WSL2 detected, proceeding as Linux."
fi

case "$(uname -s)" in
Linux)
    if ! command -v apt-get >/dev/null 2>&1; then
        fail "this script only knows apt-based distros. Install manually: clang lld xorriso qemu-system-x86_64 ovmf gdb make."
    fi

    info "installing packages via apt (you may be prompted for your password)..."
    sudo apt-get update
    sudo apt-get install -y \
        clang \
        lld \
        llvm \
        clang-format \
        clang-tidy \
        xorriso \
        qemu-system-x86 \
        ovmf \
        gdb \
        make \
        curl

    info "done. OVMF firmware is under /usr/share/OVMF/ or /usr/share/ovmf/ depending on distro version;"
    info "scripts/run.sh auto-detects it."
    ;;

Darwin)
    if ! command -v brew >/dev/null 2>&1; then
        fail "Homebrew not found. Install it from https://brew.sh, then re-run this script."
    fi

    info "installing packages via Homebrew..."
    brew install llvm lld xorriso qemu gdb make curl

    info "done. Homebrew's llvm keg is keg-only (not linked onto PATH, so it doesn't"
    info "shadow Xcode's own clang) -- the top-level Makefile finds clang/ld.lld there"
    info "automatically via 'brew --prefix llvm'. clang-format and clang-tidy ship in"
    info "the same keg: run '\$(brew --prefix llvm)/bin/clang-format' directly, or add"
    info "that bin/ to your PATH. OVMF ships inside the qemu formula's share dir."
    ;;

MINGW* | MSYS* | CYGWIN*)
    fail "native Windows kernel cross-development isn't realistic (no clean path to a" \
        "freestanding toolchain, QEMU, and OVMF that behaves the same as Linux/macOS)." \
        "Install WSL2 (https://learn.microsoft.com/windows/wsl/install), open an Ubuntu" \
        "shell inside it, clone the repo there, and run this script again."
    ;;

*)
    fail "unrecognized platform '$(uname -s)'. This template supports Linux, macOS, and WSL2."
    ;;
esac
