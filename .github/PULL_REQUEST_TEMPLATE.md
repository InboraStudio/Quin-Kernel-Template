## Summary

<!-- What does this change do, and why? -->

## Checklist

- [ ] Builds with zero warnings (`-Werror` passes)
- [ ] `clang-format --check` passes (`scripts/build.sh fmt-check` or CI)
- [ ] Host-side unit tests pass (`make -C tests/unit`)
- [ ] QEMU boot smoke test passes (`scripts/run.sh test`)
- [ ] `docs/ROADMAP.md` updated if this changes what's implemented/scaffolded
- [ ] New non-obvious constants/bit layouts cite their source (OSDev Wiki, Limine spec, Intel SDM volume/section)

## Testing

<!-- How did you verify this? Paste relevant serial output if useful. -->
