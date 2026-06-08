# Contributing to cLog++

Thank you for your interest in contributing!

## How to Contribute
1. **Fork the repository** and clone it locally.
2. **Create a new branch** for each feature or fix.
3. **Write clear code** and **add tests** for any new behavior.
4. **Build and run the tests** before submitting:
   ```bash
   cmake -S . -B build
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```
   To compile a single test directly: `g++ -std=c++17 -pthread -Iinclude tests/levels.cpp -o levels && ./levels`
5. **Open a pull request** (PR) with a clear, descriptive title and summary.
6. **Participate in code review. Please be responsive, constructive, and open!**

## Reporting Issues
- Search for existing issues first.
- Open an Issue with a clear description, steps to reproduce, your platform, and expected vs. actual behavior.
- Bug reports, documentation, and feature requests all welcome!

## Code Style
- Modern C++17. Prefer clarity, consistency, and minimal macros.
- Document any public APIs in comments.
- Chainable builder APIs and clear naming are preferred.

## License
By contributing you agree your work will be licensed under the MIT license.
