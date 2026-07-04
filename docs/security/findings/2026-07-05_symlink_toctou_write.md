# Finding: sandbox write path had a check→open TOCTOU symlink window

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
target BEFORE the open: it realpath-prefix-matches the parent directory against
`PGY_IO_ROOT` and `lstat`-rejects a symlinked final component
(`pgy_runtime_lib_file_path_core.h:308`). But the validation and the subsequent
`fopen(resolved, "wb")` are two separate syscalls. An attacker who can write
into the sandbox tree can, between the check and the open, replace the
already-validated final component with a symlink pointing OUTSIDE `PGY_IO_ROOT`
(e.g. at `/etc/passwd`). The check passed on a regular file; the open then
followed the freshly-planted symlink and wrote out of the sandbox. Classic
time-of-check-to-time-of-use (TOCTOU) race.

Note the scope honestly: this is NOT a memory-safety / UB defect (the C is
fully defined throughout). It is a *policy* escape — the sandbox boundary,
not the language's structural safety. It lives in exactly the I/O-boundary
residue the project already delegates to runtime enforcement, per the trinity
(docs/155): the residue must fail closed, and here it did not close the race.

## Reproducer

POSIX only (symlink creation). Conceptually:

```sh
export PGY_IO_ROOT="$sandbox"
# attacker, racing the runtime:
#   ln -sf /etc/passwd "$sandbox/results.txt"   # after resolve, before open
# program: WriteFile("results.txt", "...")       # followed the symlink pre-fix
```

Regression fixture: `tests/security/symlink_write_nofollow_smoke.sh` plants
symlinks AT the resolved targets for both `WriteFile` and handle-based
`FileOpen(..., "w")` (the stronger, non-racy form of the same escape: if a
symlink already sits at the target, the write must still refuse) and asserts
both writes fail.

## Expected vs Actual

- **Expected**: a write whose final target is (or becomes) a symlink out of
  the sandbox must fail closed, atomically.
- **Actual (pre-fix)**: the `lstat` check and the `fopen` were non-atomic; a
  symlink swapped in after the check was followed by the open.

## Root Cause

Two-syscall check-then-use: `pgy_runtime_resolve_file_path` (`lstat`) followed
by write-capable `fopen` calls in the whole-file write path and the
handle-based `FileOpen` path. The LLVM runtime export was already routed
through `pgy_runtime_secure_fopen`; the C inline runtime still had a direct
`fopen(resolved, mode)` in `pgy_runtime_io_qubit_inline.h`.

## Fix

- **Code change**: new `src/runtime/pgy_runtime_secure_open.h` provides
  `pgy_runtime_secure_fopen(resolved, mode)`. On POSIX, write-capable modes are
  opened with `open(resolved, O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0666)` then
  `fdopen` — the kernel refuses (ELOOP) if the final component is a symlink AT
  open time, atomically, with no window. Read modes keep plain `fopen` (their
  resolve realpath-resolves the FULL candidate, so a symlink out of the sandbox
  already fails resolution). Every sandbox write-open site now routes through
  this helper, including the C inline `FileOpen` path and the LLVM runtime
  export path. The `lstat` pre-check stays as a first, cheap gate — this is
  layered on top, not a replacement.
- **Regression test added**: `tests/security/symlink_write_nofollow_smoke.sh`
  (POSIX-gated; SKIPs where symlink creation is unavailable, e.g. Windows),
  wired to `make sandbox-symlink-nofollow-test-smoke` and the ci-linux step
  list (Linux is its real CI home).
- **Backend-compare**: both runtime surfaces consume the hardened open owner:
  the C inline runtime and the LLVM runtime export.
- **Self-host codegen follow-up**: the standalone C emitted by the Pergyra
  self-host codegen rung now routes `ReadFile`, `WriteFile`, and handle-based
  `FileOpen` through a generated `pgy_secure_fopen` helper. On POSIX that
  helper uses `open(..., O_NOFOLLOW)` plus `fdopen`, and the helper name is
  owned by `HostIORuntimeOwner` instead of being a backend-local spelling.
- **Verified on real Linux**: a standalone harness of `pgy_runtime_secure_fopen`
  compiled with the project's exact Linux `PLATFORM_CFLAGS`
  (`-std=c11 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
  -Werror=implicit-function-declaration`, Ubuntu gcc 13.3) refused a
  symlinked write target (NULL / ELOOP), left the outside file intact, and
  wrote a regular target normally. `O_NOFOLLOW` is a POSIX.1-2008 symbol so
  `_POSIX_C_SOURCE=200809L` exposes it — the existing `realpath`/`lstat` in
  the resolve path rely on the same PLATFORM_CFLAGS, so there is no latent
  feature-macro break.

## Backend Parity Status

- C backend: fixed ✓ (inline runtime open path)
- LLVM backend: fixed ✓ (runtime export open path)
- Self-host C-emission rung: fixed for generated standalone C file helpers
  (POSIX nofollow helper, C/LLVM-built codegen parity)
- Backend-compare regression added: ✓ (POSIX-gated smoke)

## Residual (not closed here)

- **Windows**: the open still uses `fopen`. The resolve path already rejects
  any reparse-point component (`pgy_runtime_path_has_reparse_component`), but a
  fully atomic open-time guard would need `CreateFileA` +
  `FILE_FLAG_OPEN_REPARSE_POINT`. Tracked as an A4/A5-adjacent residual, not
  closed in this finding. Registered here so it is not silently assumed closed.

## Disclosure

Pergyra is pre-1.0 with no production users. Filed openly.

## References

- Threat model: `../02_red_team_threat_model.md` (tier A3)
- Trinity (residue must fail closed): `../../155_declare_gate_failclose.md`
- Related finding: `2026-07-05_fingerprint_not_attestation.md` (same red-team pass)
