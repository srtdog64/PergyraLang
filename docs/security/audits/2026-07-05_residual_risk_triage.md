# Security Residual Risk Triage - 2026-07-05

This note checks the beta-closure residual-risk review against the current
tree. It separates stale findings from real residual debt so security status
does not drift from code reality.

## Verdicts

| Claim | Current verdict | Evidence owner |
|---|---|---|
| Windows symlink TOCTOU residual: closed | The runtime secure-open owner no longer writes through Windows `fopen`. Write-capable sandbox opens use `CreateFileA` with `FILE_FLAG_OPEN_REPARSE_POINT`, reject an opened reparse-point handle, then convert the handle to a CRT `FILE*`. | `src/runtime/pgy_runtime_secure_open.h`, `security-portability-contract-test-smoke` |
| Secure slot read/write TOCTOU: stale | `SlotWriteSecure` and `SlotReadSecure` resolve the entry, validate the token, and seal/open payloads under `manager_mutex`. The adjacent pin/context lifecycle issue is the fixed finding. | `src/runtime/slot_manager_secure_ops.c`, `docs/security/findings/2026-07-05_slot_secure_toctou_audit.md` |
| Hardware fingerprint hash/FNV claim: stale | The current fingerprint path uses `SecureHashSHA256`. The surviving limit is inherent: fallback identity is a software binding hint, not hardware attestation. | `src/runtime/slot_security_fingerprint.c`, `docs/security/findings/2026-07-05_fingerprint_not_attestation.md` |
| Cited free-text slot logs: stale | The cited slot warning paths emit JSON lines and escape string fields through `pgy_runtime_fprint_json_string`. Future regressions are forbidden for those owners. | `src/runtime/slot_security.c`, `src/runtime/slot_manager_core_ops.c`, `security-portability-contract-test-smoke` |
| `Array<String>` ownership: beta policy closed | Generic `Array<String>` remains pointer-storage for beta. Stable result-producing APIs such as `StringSplit` and `MapKeys` must duplicate payloads before returning a result-owned array. Global deep-copy push/set/drop remains a beta+ ABI proposal, not a silent security patch. | `docs/128_pointer_risk_register.md`, `runtime-abi-lifetime-test-smoke` |
| Scratch-to-cache lifetime drift: partially open | Many historical static scratch seams are removed or ratcheted, but there is not yet a complete whole-program static analyzer proving every scratch pointer cannot be cached in a longer-lived lane. This remains real residual debt. | `docs/128_pointer_risk_register.md`, `perf-contract-test-smoke`, `runtime-abi-lifetime-test-smoke` |
| System-tier raw pointer escape: intentionally out of beta | `unsafe {}` is a lexical boundary, not permission for raw pointers, MMIO, or inline assembly. Raw escape is rejected until a scoped capability contract has diagnostics, AIR evidence, ABI lowering, and runtime-none semantics. | `docs/132_unsafe_capability_scope.md`, `raw-escape-contract-test-smoke` |

## Residual Work That Remains Real

1. Build a narrower scratch-to-cache lifetime ratchet that rejects storing
   scratch-lane pointers in persistent registries, diagnostics, or ABI metadata.
   Current tests cover known seams; they are not a general static proof.
2. Keep `Array<String>` Option B as an explicit beta+ ABI migration if the
   language chooses container-owned strings globally. Do not change generic
   mutation semantics silently under a security label.
3. Keep raw/system-tier escape out of beta claims. If it moves from HOLD to
   implementation, add scoped capability evidence first, then C/LLVM/runtime
   lowering.

## Gate

`security-portability-contract-test-smoke` now locks this triage note and the
cited structured-log owner terms so the stale claims cannot reappear as current
status.
