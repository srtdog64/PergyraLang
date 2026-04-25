# String And Unicode Beta Policy

Status: beta-freeze-source-of-truth.

This document freezes the beta string/unicode surface. Pergyra beta supports
UTF-8 string payloads, but it does not claim a full Unicode text model.

Executable gate: `make unicode-policy-test-smoke`.

## Stable Surface

- Source files are treated as UTF-8 byte streams.
- String literals may contain UTF-8 payload bytes.
- The C and LLVM backends must preserve those UTF-8 bytes in generated output.
- `Contains`, `Concat`, `Replace`, `Trim`, `Upper`, `Lower`, and file IO operate
  on the current runtime string representation.
- `StringLength` is byte-length for beta, not Unicode scalar-value length and not
  grapheme-cluster length.
- String equality and substring search are byte-exact and normalization-blind.

## Explicit Reject / Out Of Beta

- Unicode identifiers are not beta-stable.
- Unicode normalization is not performed by the runtime.
- Locale-sensitive comparison, case folding, and collation are out-of-beta.
- Grapheme-cluster iteration and display-width calculation are out-of-beta.
- Mixed-encoding source files are out-of-beta.

## Rationale

The beta contract must be narrow enough to keep C/LLVM parity honest. UTF-8
payload preservation is useful and testable today; a full Unicode text model
requires new vocabulary for normalization, locale, scalar values, grapheme
clusters, and diagnostics. Those semantics must not be implied by accepting
UTF-8 bytes inside a string literal.
