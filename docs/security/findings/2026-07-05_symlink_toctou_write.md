# Finding: sandbox write path had a check-to-open TOCTOU symlink window

- **Finding ID**: 2026-07-05-001
- **Date filed**: 2026-07-05
- **Audit run**: external red-team pass (Gemini/Antigravity), verified against
  current tree by Claude
- **Contract**: I/O sandbox boundary (`PGY_IO_ROOT` containment), threat model
  tier A3 (scheduler / filesystem-race attacker), `../02_red_team_threat_model.md`
- **Severity**: Low (defense-in-depth; the primary check was already present)
- **Status**: Fixed

## Summary

`pgy_runtime_resolve_file_path(path, /*for_write=*/true)` validates a write
target before the open: it realpath-prefix-matches the parent directory against
`PGY_IO_ROOT` and `lstat`-rejects a symlinked final component
(`pgy_runtime_lib_file_path_core.h`). Before the fix, validation and the
subsequent write-capable `fopen` were separate operations. An attacker who can
write into the sandbox tree could replace the already-validated final component
with a symlink pointing outside `PGY_IO_ROOT` between check and open.

This is not a memory-safety / UB defect. It is an I/O policy escape at a
runtime boundary residue. Per the trinity contract (`docs/155`), the residue
must fail closed, and the pre-fix open path did not close the race.

## Reproducer

POSIX only for the symlink fixture:

```sh
export PGY_IO_ROOT="$sandbox"
# attacker, racing the runtime:
#   ln -sf /etc/passwd "$sandbox/results.txt"   # after resolve, before open
# program: WriteFile("results.txt", "...")       # followed the symlink pre-fix
```

Regression fixture: `tests/security/symlink_write_nofollow_smoke.sh` plants
symlinks at the resolved targets for both `WriteFile` and handle-based
`FileOpen(..., "w")` and asserts both writes fail.

## Expected vs Actual

- **Expected**: a write whose final target is or becomes a symlink out of the
  sandbox must fail closed at open time.
- **Actual (pre-fix)**: the `lstat` check and `fopen` were non-atomic; a
  symlink swapped in after the check could be followed by the open.

## Root Cause

Two-step check-then-use: `pgy_runtime_resolve_file_path` validated the path,
then write-capable open sites used `fopen`. The C inline `FileOpen` path and the
LLVM runtime export path needed to consume a single secure-open owner.

## Fix

- **Runtime owner**: `src/runtime/pgy_runtime_secure_open.h` provides
  `pgy_runtime_secure_fopen(resolved, mode)`.
- **POSIX write opens**: write-capable modes use `open(..., O_NOFOLLOW)` plus
  `fdopen`. The kernel refuses a symlinked final component at open time.
- **Windows write opens**: write-capable modes use `CreateFileA` with
  `FILE_FLAG_OPEN_REPARSE_POINT`; the opened handle is rejected if
  `GetFileInformationByHandle` reports `FILE_ATTRIBUTE_REPARSE_POINT`, and only
  then is the handle converted to a CRT `FILE*`.
- **Read opens**: plain `fopen` remains acceptable because the resolve path
  realpath-resolves the full candidate and prefix-rejects a symlink out of the
  sandbox before any open.
- **Layering**: the path pre-check remains load-bearing. Secure open is
  defense-in-depth on top of that check, not a replacement.
- **Backend parity**: C inline runtime and LLVM runtime exports consume the same
  secure-open owner.
- **Self-host codegen follow-up**: generated standalone C file helpers route
  `ReadFile`, `WriteFile`, and handle-based `FileOpen` through generated
  `pgy_secure_fopen`; POSIX generated artifacts are probed by the self-host
  codegen parity gate.

## Backend Parity Status

- C backend: fixed (inline runtime open path; POSIX `O_NOFOLLOW`, Windows
  `CreateFileA` reparse-point handle rejection)
- LLVM backend: fixed (runtime export open path consumes the same runtime owner)
- Self-host C-emission rung: fixed for generated standalone C file helpers
  (POSIX nofollow helper, C/LLVM-built codegen parity plus generated-artifact
  symlink probes)
- Backend-compare regression added: fixed (POSIX-gated smoke)

## Windows Residual Update

- **Closed on 2026-07-05**: the runtime secure-open owner no longer relies on
  Windows `fopen` for write-capable sandbox opens. It uses `CreateFileA` with
  `FILE_FLAG_OPEN_REPARSE_POINT` and rejects an opened reparse-point handle
  before handing the descriptor to the CRT. `security-portability-contract`
  locks those Windows API terms so the residual cannot silently regress.

## Verification

- `make sandbox-symlink-nofollow-test-smoke` on POSIX exercises the symlink
  write denial path.
- `make security-portability-contract-test-smoke` locks the secure-open owner
  terms, including POSIX `O_NOFOLLOW` and Windows `CreateFileA` +
  `FILE_FLAG_OPEN_REPARSE_POINT`.
- `make test-security` covers the runtime security suite.

## Disclosure

Pergyra is pre-1.0 with no production users. Filed openly.

## References

- Threat model: `../02_red_team_threat_model.md` (tier A3)
- Trinity (residue must fail closed): `../../155_declare_gate_failclose.md`
- Related finding: `2026-07-05_fingerprint_not_attestation.md` (same red-team pass)
