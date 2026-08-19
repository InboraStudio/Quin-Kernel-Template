# Contributing to Quin Kernel Template

Quin is a *template*: people fork it and build their own kernel on top. The
bar for changes here is higher than for a normal application repo, because
every line is something a beginner will read as an example of how it's done.

## Commit convention

This repo follows [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add IOAPIC redirection table setup
fix: correct PML4 index mask in vmm_map
docs: explain higher-half offset in ARCHITECTURE.md
chore: bump Limine submodule to v8.5.1
```

Common types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`, `ci`.

## Branch naming

`<type>/<short-description>`, e.g. `feat/lapic-timer-calibration`,
`fix/gdt-tss-limit-off-by-one`.

## Before opening a PR

1. `scripts/build.sh` — must complete with zero warnings (`-Werror` is on;
   treat any new warning as a bug in your change, not something to suppress).
2. `scripts/build.sh fmt-check` (or `scripts/build.sh fmt` to fix in place)
   — `clang-format` against `.clang-format`.
3. `make -C tests/unit` — host-side unit tests must pass.
4. `scripts/run.sh test` — QEMU headless smoke test must pass.
5. Update `docs/ROADMAP.md` if your change moves something from
   "scaffolded" to "implemented", or adds new scaffolding.

The PR template checklist mirrors this list.

## Code review expectations

- No unexplained boilerplate, no dead code, no placeholder function without
  a linked GitHub issue.
- Comments explain *why*, not *what* — see `docs/CODING_STYLE.md`.
- Non-obvious bit layouts, offsets, or hardware behavior cite a source
  (OSDev Wiki page, Limine protocol spec section, Intel SDM volume/section).
- If a design decision is genuinely ambiguous, say so in the PR description
  rather than picking silently — reviewers would rather discuss it than
  reverse-engineer your reasoning later.

## Scope

Quin targets x86_64 + QEMU only. Real-hardware drivers, other architectures,
and alternate boot protocols are out of scope for this template — see
`docs/ROADMAP.md` for what's explicitly excluded and why.
