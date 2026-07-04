# Finding: secure slot check/use atomicity audit

- **Finding ID**: 2026-07-05-003
- **Date filed**: 2026-07-05
- **Audit run**: external red-team pass, verified against current tree
- **Contract**: `SlotManager` secure runtime API token/use atomicity
- **Severity**: Medium for the residual found in pin/context lifecycle
- **Status**: Fixed

## Summary

The report claimed `SlotWriteSecure` and `SlotReadSecure` validated a token,
released the manager lock, and only then read or wrote slot data. That exact
claim is stale in the current tree: both functions already resolve the
`SlotEntry`, validate the token with `slot_token_valid_for_entry_locked`, and
seal/open the payload while holding the manager mutex.

The audit did expose the same class of bug at adjacent secure boundaries:

- `PergyraSlotPin` validated the token through public `SlotValidateToken`
  before taking the manager lock, then reacquired the entry later.
- `SlotClaimSecure` generated tokens from `manager->securityContext` after a
  raw claim without keeping the context lifecycle under the same lock.
- `SlotManagerDisableSecurity` detached/destroyed `securityContext` outside the
  full owner rule, while logging/stats code could also read the context without
  a common locked path.

Those are now closed by making the manager mutex the source of truth for the
secure entry plus security-context pair.

## Fix

- `slot_token_valid_for_entry_locked` is now an internal locked owner API,
  consumed by secure read/write/release/refresh and by `PergyraSlotPin`.
- `PergyraSlotPin` no longer calls public `SlotValidateToken` before taking the
  entry lock. Token-slot-generation validation happens after the entry is
  resolved and before any secure payload open.
- `SlotClaimSecure` claims the raw slot, then re-enters the manager lock and
  generates/stores the secure token only if the locked security context is still
  enabled. If security was disabled in between, the raw slot is released and the
  operation fails closed.
- `SlotManagerEnableSecurity`, `SlotManagerDisableSecurity`,
  `SlotManagerSetDefaultSecurityLevel`, `SlotManagerIsSecurityEnabled`, security
  event logging, anomaly detection, and stats printing now read or modify
  `securityContext` through the manager mutex.
- `SlotManagerLogSecurityEvent` is the public locking wrapper; already-locked
  secure paths consume `slot_manager_log_security_event_locked` to avoid
  deadlock while keeping `SecurityAuditLog` under the same context lifetime
  owner.
- Manager-level security events now emit JSONL to `stderr`, matching
  `slot-security-audit` and other security failure events. This avoids
  stdout/stderr interleaving that can make otherwise-structured logs appear
  malformed to a combined-stream collector.

## Regression Gates

- `make security-portability-contract-test-smoke` now forbids
  `SlotValidateToken(manager, handle, token)` in `slot_manager_pin.c` and
  requires the locked token-validation owner there.
- The same gate requires the context detach/destroy pattern in
  `slot_manager_secure_ops.c` and the locked logging owner in
  `slot_manager_security_stats.c`.
- The gate also requires manager security events to use the stderr JSONL path,
  not stdout.
- Existing `make test-security` keeps the functional secure slot and pin/lease
  behavior covered.

## Related Claims From The Same Report

- Sandbox symlink write TOCTOU: already fixed by
  `pgy_runtime_secure_fopen(... O_NOFOLLOW ...)`; see
  `2026-07-05_symlink_toctou_write.md`.
- Hardware fingerprint FNV/hostname claim: stale. Current code hashes
  fingerprint fallback material with SHA-256 and documents that software
  fallback is a binding hint, not attestation; see
  `2026-07-05_fingerprint_not_attestation.md`.
- Free-text security logs: stale for the cited functions. The current warning
  paths emit JSON lines with escaped string fields. The remaining quality rule
  is that future security logs should use the shared JSON string emitter or a
  locked logging owner, not raw prose prefixes.
