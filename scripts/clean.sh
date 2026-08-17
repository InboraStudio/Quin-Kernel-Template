#!/usr/bin/env bash
#
# Removes build output. `--all` also drops the cached Limine bootloader
# binary download (.cache/), forcing a re-fetch on the next build.

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"
cd "$(quin_repo_root)"

if [[ "${1:-}" == "--all" ]]; then
    make distclean
else
    make clean
fi
