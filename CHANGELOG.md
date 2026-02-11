# cLog++ Changelog

## [v0.1.0] - 2026-02-10
### Added
- First public release: async by default, robust background log draining, JSON output.
- File/console sink, chainable API for log creation.
- Test suite for async flush, sync mode, file sink, and crash-avoidance.
- Modern, minimal, single-header main implementation.

### Fixed
- Thread shutdown is now completely lossless. The background logging thread always fully drains before the logger is destroyed.

---
All changes will be documented in this file.
