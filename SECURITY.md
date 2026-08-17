# Security Policy

Quin Kernel Template is an educational starting point for people writing
their own hobby kernel. It is **not** a hardened, production-grade kernel:
there is no threat model beyond "runs correctly in QEMU while you learn how
an x86_64 kernel is put together." Memory safety in freestanding C, physical
device emulation quirks, and the general immaturity of a from-scratch kernel
mean you should never boot this (or anything derived from it) on hardware
that matters, or expose it to untrusted input.

That said, incorrect behavior in the template itself — a bug in the PMM that
double-allocates a physical frame, an IDT entry pointing at the wrong
handler, a page table permission that should be `NX` but isn't — is worth
reporting, since people are learning correct patterns from this code.

## Reporting a vulnerability or correctness bug

Please open a GitHub issue using the bug report template. If the issue
involves something you'd rather not disclose publicly first (unlikely for a
template repo, but possible), email the maintainers listed in
`.github/CODEOWNERS` instead and we'll coordinate disclosure.

Include:

- The affected file(s) and, if applicable, the Intel SDM section or OSDev
  Wiki page describing correct behavior.
- Steps to reproduce (build + QEMU invocation).
- Why the current behavior is unsafe or incorrect, not just different from
  what you'd choose.

## Supported versions

Only the latest commit on `main` is supported. This is a template, not a
versioned library — there are no backported security fixes to older tags.
