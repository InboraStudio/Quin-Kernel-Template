# Coding Style

Formatting is mechanical — `.clang-format` and `clang-format --check` in CI
own it, don't hand-wrangle whitespace. This document covers the things a
formatter can't enforce: naming, comments, and how code should read.

## Naming

- Functions and variables: `snake_case`. `pmm_alloc_frame`, `page_count`.
- Types (`struct`, `enum`, `typedef`): `snake_case_t` for typedefs used as
  values (`vaddr_t`, `pte_t`), bare `snake_case` tag names for structs passed
  by pointer (`struct thread`, `struct idt_entry`).
- Macros and compile-time constants: `SCREAMING_SNAKE_CASE`.
- File-local (`static`) functions and globals: no special prefix — `static`
  is the marker. Do not prefix with `_` or `priv_`.
- Subsystem prefixes match the directory: `kernel/mm/pmm.c` exports
  `pmm_*`, `kernel/sched/sched.c` exports `sched_*`. This is what lets you
  grep a symbol and know which file owns it without an IDE.

## Comments

Comments explain *why*, not *what*. The code already says what it does; a
comment that repeats it is noise a reader has to filter out. Write a comment
when:

- The reasoning isn't visible in the code itself (a workaround for a
  specific errata, a non-obvious ordering requirement, a trade-off you
  considered and rejected).
- A bit layout, magic offset, or hardware behavior needs a citation —
  OSDev Wiki page, Limine protocol spec section, or Intel SDM volume and
  section number. Reference it inline: `// SDM Vol. 3A, 4.5: PML4 entries`.
- Something will look wrong or unsafe out of context and isn't.

Don't write a comment when it would just restate the line under it. Comment
density should track actual complexity: a one-line getter gets nothing, a
subtle allocator or interrupt stub earns a real explanation. Not every
function needs the same shape of comment above it — that uniformity is
itself a sign the comment was padding, not information.

No section-banner comments (`// ===`, `// ---`, boxed headers). Blank lines
and file structure carry the same information without the noise. No hype
language, exclamation marks, or emoji in comments, logs, or panic messages —
model log tone on `dmesg`: terse and factual (`[INFO] apic: calibrated at
998244353 Hz`), not narrative.

## Error handling

Kernel code has two failure modes: a bug that should never happen (assert or
panic) and an expected, recoverable condition (return an error and let the
caller decide). Don't blur the two — an allocator running out of memory is
expected and returns `NULL`/an error code; an allocator handed a corrupt
free-list pointer has a bug and should panic loudly rather than limp on.

Don't add defensive checks for states the type system or calling convention
already rules out. If a function is only ever called with a validated
pointer, don't re-validate it — that's noise that also hides the real
precondition.

## Freestanding constraints

No libc. `kernel/lib` provides the freestanding subset actually used
(`memcpy`, `memset`, a `printf`-family for the logger, string helpers) —
don't reach for a hosted-environment idiom that assumes an allocator, exceptions,
or the standard library exists.

## What "done" looks like

No dead code, no commented-out blocks, no unused functions. A `TODO`
without a linked GitHub issue is a promise nobody's tracking — either file
the issue or don't leave the comment. `docs/ROADMAP.md` is the source of
truth for what's implemented vs. scaffolded; don't let a comment or the
README imply a subsystem is finished when the roadmap says otherwise.
