#!/usr/bin/env bash
# Sourced by the other scripts/*.sh -- not meant to be run directly.

quin_repo_root() {
    cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
}

# OVMF ships under different paths depending on how it was installed:
# Ubuntu/Debian's `ovmf` apt package uses the 4M split code/vars layout;
# Homebrew's `qemu` formula bundles its own edk2 build; a few other
# distros still use the older non-suffixed naming. Tries each in turn
# rather than hard-coding one, since scripts/setup-toolchain.sh's apt and
# brew paths install to different places.
quin_find_ovmf_code() {
    local candidates=(
        /usr/share/OVMF/OVMF_CODE_4M.fd
        /usr/share/OVMF/OVMF_CODE.fd
        /usr/share/edk2/ovmf/OVMF_CODE.fd
    )
    if command -v brew >/dev/null 2>&1; then
        local prefix
        prefix="$(brew --prefix qemu 2>/dev/null || true)"
        [[ -n "$prefix" ]] && candidates+=("$prefix/share/qemu/edk2-x86_64-code.fd")
    fi
    local path
    for path in "${candidates[@]}"; do
        [[ -f "$path" ]] && printf '%s\n' "$path" && return 0
    done
    return 1
}

quin_find_ovmf_vars() {
    local candidates=(
        /usr/share/OVMF/OVMF_VARS_4M.fd
        /usr/share/OVMF/OVMF_VARS.fd
        /usr/share/edk2/ovmf/OVMF_VARS.fd
    )
    if command -v brew >/dev/null 2>&1; then
        local prefix
        prefix="$(brew --prefix qemu 2>/dev/null || true)"
        [[ -n "$prefix" ]] && candidates+=("$prefix/share/qemu/edk2-x86_64-vars.fd")
    fi
    local path
    for path in "${candidates[@]}"; do
        [[ -f "$path" ]] && printf '%s\n' "$path" && return 0
    done
    return 1
}
