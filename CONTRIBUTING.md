# Contributing to cLog++

Thank you for your interest in contributing. Bug reports, documentation,
features and code are all welcome.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCLOGPP_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure --parallel 4
```

Useful options:

| Option                                                 | Purpose                                        |
| ------------------------------------------------------ | ---------------------------------------------- |
| `-DCLOGPP_BUILD_BENCHMARKS=ON`                         | also build `benchmark_logger`                  |
| `-DCLOGPP_SANITIZE=address,undefined`                  | ASan + UBSan (GCC/Clang, use a Debug build)    |
| `-DCLOGPP_SANITIZE=thread`                             | TSan; best with `-DCMAKE_CXX_COMPILER=clang++` |
| `-DCLOGPP_BUILD_TESTS=OFF -DCLOGPP_BUILD_EXAMPLES=OFF` | library only                                   |

To compile a single test without CMake:

```bash
g++ -std=c++17 -pthread -Iinclude -Itests tests/levels.cpp -o levels && ./levels
```

CI runs the suite on GCC, Clang and MSVC, under the sanitizers, checks that the
installed package is consumable with `find_package`, compiles the single-header
build, and checks formatting. A change is ready when all of that is green.

## Making a change

1. Fork the repository and create a branch for the change.
2. Keep the library header-only and dependency-free (standard library only).
3. Add or update a test in `tests/` for every behavioural change. Tests use the
   `CHECK` macros from `tests/test_util.hpp`, not `assert()`, so they also run
   in Release builds.
4. Run `clang-format -i` on the files you touched (the repository ships a
   `.clang-format`). CI uses the clang-format from `ubuntu-latest`; versions 18
   and later agree on this codebase.
5. Update `CHANGELOG.md` under the upcoming version, and `docs/api.md` if the
   public API changed.
6. Open a pull request with a clear title and a summary of what changed and
   why. Link related issues (for example "Fixes #12").

## Code style

- Modern C++17. Prefer clarity and minimal machinery; no macros in the public
  API.
- Public types and functions carry a short comment saying what they do and any
  threading or lifetime rule the caller must know.
- Keep the hot path allocation-light and free of thread-local state with
  non-trivial destructors (see `docs/design.md` for why).
- Match the surrounding code; the formatter handles layout.

## Reporting issues

Search existing issues first. For bugs, include a minimal code sample, the
compile and run command, your platform and compiler version, the cLog++
version or commit, and what you observed versus what you expected.

## License

By contributing you agree that your work is licensed under the MIT license.
