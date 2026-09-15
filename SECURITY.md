# Security Policy

## Supported versions

Only the latest release receives fixes.

| Version | Supported |
|---------|-----------|
| 0.3.x   | yes       |
| < 0.3   | no        |

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub: open the repository
**Security** tab and choose *Report a vulnerability* (a private security
advisory). If that is unavailable, open an issue labelled `security` with
minimal detail and ask for a private channel. Please do not disclose the issue
publicly until a fix has been coordinated.

You can expect an acknowledgement within a week. Fixes ship as a patch release
with a CHANGELOG entry crediting the reporter unless you prefer otherwise.

## Scope notes

cLog++ writes what you give it. The JSON formatter escapes strings per RFC
8259, so untrusted input in `kv()` values cannot break the line structure.
`kv_raw()` is emitted verbatim by design and must only receive JSON you have
validated. File sinks open the paths they are given without further checks;
treat log paths as configuration, not user input.
