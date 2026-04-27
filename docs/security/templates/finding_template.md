# Finding Template

Copy this file when filing a new finding. Naming convention:
`findings/YYYY-MM-DD_<short-id>.md` (e.g.,
`findings/2026-05-01_secure_slot_replay.md`).

---

# Finding: <one-line summary>

- **Finding ID**: YYYY-MM-DD-NNN
- **Date filed**: YYYY-MM-DD
- **Audit run**: link to `audits/...` log
- **Contract**: link to `contracts/...`
- **Severity**: Critical | High | Medium | Low | Informational
- **Status**: Open | Fixed | False positive | Wontfix (with reason)

## Summary

One paragraph: what invariant was violated, in what runtime state, by
what operation sequence.

## Reproducer

Minimal Pergyra source or C harness that exhibits the issue. Should be
small enough to land as a regression test.

```pergyra
// or C harness
```

## Expected vs Actual

- **Expected (per invariant in `contracts/...`)**:
- **Actual**:

## Severity Classification

Pergyra severity bands:

- **Critical** — Silent wrong data returned to user code (e.g., wrong
  bytes from forged-token read). Data integrity broken.
- **High** — Wrong panic class fired (e.g., `released-slot` instead of
  `authority-token-mismatch`). Recoverability misclassified.
- **Medium** — Loud panic when contract said no panic should occur, or
  rejection when contract said acceptance.
- **Low** — Diagnostic message wrong but behavior correct.
- **Informational** — Contract is unclear; not a code bug, a doc bug.

## Root Cause

Code path that produced the violation. File:line references.

## Fix

- **Code change**: PR / commit reference.
- **Regression test added**: `tests/cases/security/<file>.pgy` or
  C harness, referenced by Finding ID.
- **Backend-compare**: C and LLVM both fixed and parity verified.
- **Doc update**: contracts/... or other docs updated if invariant
  was clarified.

## Backend Parity Status

- C backend: fixed ✓ / not affected ✓ / pending
- LLVM backend: fixed ✓ / not affected ✓ / pending
- Backend-compare regression added: ✓ / not applicable

## Disclosure

Pergyra is pre-1.0 with no production users (as of finding date).
Findings are filed openly. After 1.0:

- **Critical / High**: 90-day private disclosure to known users
  before public announcement.
- **Medium / Low**: open from filing.
- **Informational**: open from filing.

## References

- Contract doc: `contracts/...`
- Audit log: `audits/...`
- Related findings: (if any)
- Upstream issue (if reported externally): (if any)
