## What changed?

<!-- Describe the change and why. Link related issues, e.g. "Fixes #12". -->

## Checklist

- [ ] Builds and tests pass: `cmake -S . -B build -DCLOGPP_WARNINGS_AS_ERRORS=ON && cmake --build build && ctest --test-dir build`
- [ ] Added or updated tests in `tests/` (using `CHECK`, not `assert`)
- [ ] Ran `clang-format -i` on touched files
- [ ] Updated `CHANGELOG.md` and, for public API changes, `docs/api.md`
- [ ] I license my contribution under the MIT license
