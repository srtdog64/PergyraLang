# Prioritized Audit Targets

Last updated: 2026-06-13

This document lists Pergyra runtime safety contracts in priority order for AI
Validator audit. Priority reflects:

1. **Tier in `docs/118` section 4**: Tier 3 runtime fallback is highest priority
   because static rules do not back it.
2. **Adversarial exposure**: security-relevant contracts such as token and
   authority invariants come before non-security contracts such as panic class
   parity or OOB handling.
3. **Marketing claim weight**: contracts the language publicly advertises, such
   as "secure slot tokens cannot be forged", come before internal-only
   invariants.
4. **Production user impact**: contracts that can produce silent wrong results
   come before contracts that produce loud panics.

## P0 - Must Audit Before Beta Freeze

These contracts are load-bearing for marketing claims and cannot ship to a
1-year freeze without adversarial coverage.

| ID | Contract | Source | Doc / Gate |
|---|---|---|---|
| P0-1 | Secure slot token unforgeability | `src/runtime/slot_security.c`, `src/runtime/slot_security_crypto.c`, `src/runtime/slot_security_sealed_payload.c`, `src/runtime/slot_manager.c` | [contracts/secure_slot_token_unforgeability.md](contracts/secure_slot_token_unforgeability.md), [02_red_team_threat_model.md](02_red_team_threat_model.md) |
| P0-2 | Slot generation stale-handle rejection | `src/runtime/slot_manager.c` | (TBD) |
| P0-3 | Authority transfer single-owner | `src/runtime/pgy_authority_runtime.c` (or equivalent) | (TBD) |
| P0-4 | TTL cleanup vs pin-state interaction | `src/runtime/slot_manager.c` | (TBD) |
| P0-5 | Release-while-pinned rejection | `src/runtime/slot_manager.c` | (TBD) |
| P0-6 | Slot id exhaustion availability / tombstone flooding | `src/runtime/slot_manager_core_ops.c`, `src/runtime/slot_manager.h`, `src/tests/security/test_security_runtime.cases.h`, `docs/74_slot_pinning_caching.md` | `security-portability-contract-test-smoke`, `make test-security` |

Existing regression: `make test-security` 182/182 + `make runtime-panic-abi-test-smoke`.
Audit covers unenumerated edge cases beyond those executable regressions.

## P1 - Strongly Recommended Before Beta

These contracts shape language guarantees but failures produce loud panics, so
silent-wrong-result risk is lower.

| ID | Contract | Source | Doc / Gate |
|---|---|---|---|
| P1-1 | Channel cross-World rule (cannot bypass) | `src/runtime/channel_runtime.c` (or equivalent) | (TBD) |
| P1-2 | Token transport reject (spawn / channel send / cancel) | `src/semantic/`, runtime ABI | (TBD) |
| P1-3 | Runtime panic class consistency (C vs LLVM parity) | `src/runtime/pgy_runtime_panic_contract.c` | (TBD) |
| P1-4 | OOM / divide-by-zero / OOB consistency | runtime panic | (TBD) |
| P1-5 | AIR drift detection completeness for declared kinds | `src/compiler/air.c`, `air_boundary.c` | (TBD) |
| P1-6 | Runtime panic DoS boundary policy: hard-fail vs recoverable service boundary | `src/runtime/pgy_runtime_panic_contract.c`, `src/runtime/pgy_runtime_slot_status.h`, `src/runtime/pgy_runtime_io_status.h`, `src/runtime/pgy_runtime_channel_status.h`, `src/runtime/pgy_runtime_plain_slot_inline.h`, `src/runtime/pgy_runtime_slot_macros.h`, `src/runtime/pgy_runtime_io_qubit_inline.h`, `src/runtime/pgy_runtime_channel_inline.h`, `src/runtime/pgy_runtime_channel_string_inline.h`, `src/runtime/pgy_runtime_lib_intent_slot_core_exports.h`, `src/runtime/pgy_runtime_lib_device_slot_exports.h`, `src/runtime/pgy_runtime_lib_secure_slot_exports.h`, `src/runtime/pgy_runtime_lib_io_string_exports.h`, `src/runtime/pgy_runtime_lib_channel_int_exports.h`, `src/runtime/pgy_runtime_lib_channel_string_exports.h`, `docs/105_runtime_panic_contract.md` | `runtime-panic-abi-test-smoke`, `security-portability-contract-test-smoke` |
| P1-7 | Zone-bound handle escape decision and diagnostics | `src/semantic/`, `src/compiler/air.c`, `src/compiler/mir_*` | (TBD) |

## P2 - Audit During 1-Year Freeze

These can wait for the post-beta freeze period.

| ID | Contract | Source | Doc / Gate |
|---|---|---|---|
| P2-1 | Pin block boundary rule (when Option C ships) | `src/parser/parser.c`, `src/semantic/` | (TBD) |
| P2-2 | WriteView<T> exclusive access (when Option C ships) | `src/semantic/` | (TBD) |
| P2-3 | Backend-compare parity completeness | `tests/cases/backend_compare/` | (TBD) |
| P2-4 | CFG body dataflow soundness | `src/compiler/hir_cfg.c` | (TBD) |
| P2-5 | Effect mask propagation soundness | `src/semantic/` | (TBD) |

## P3 - Post-1.0

| ID | Contract |
|---|---|
| P3-1 | Mechanized proof of P0-1, P0-2, P0-3 (Coq/Lean) |
| P3-2 | Concurrency interleaving coverage (Loom-style) |
| P3-3 | Side-channel analysis (timing, cache) |

## Contract Doc Status

Each entry above must eventually have a contract doc in `contracts/`.
Currently filled:

- P0-1: `contracts/secure_slot_token_unforgeability.md`
- P0-2 through P0-5: TBD before first audit run
- P1, P2, P3: TBD progressively

Status correction:

- P0-2 through P0-5 remain TBD before the first audit run.
- P0-6 has smoke-gated evidence for the current 32-bit ABI policy: released
  slot ids are recycled only after advancing `generation`, while fresh
  zero-id/max-id claims and generation-exhausted recycle attempts still report
  `SLOT_ERROR_ID_EXHAUSTED` / `PGY_RUNTIME_SLOT_STATUS_ID_EXHAUSTED`.
- P0-6 keeps availability exhaustion distinct from allocator failure at
  service/FFI boundaries. It is not a 64-bit handle claim.
- P1-6 has smoke-gated evidence for typed `try_*` Slot / DeviceSlot /
  SecureSlot status paths and typed `PgyRuntimeSlotResult_*` read wrappers at
  host/FFI/service boundaries. The Slot / DeviceSlot / SecureSlot panic ABI
  remains the sharp default for direct runtime calls.
- P1-6 also has first-stage typed I/O boundary evidence:
  `PgyRuntimeIoFailure`, `pgy_try_file_open_result`,
  `pgy_try_file_exists_result`, `pgy_try_file_read_result`,
  `pgy_try_file_write_result`, `pgy_try_read_file_result`,
  `pgy_try_write_file_result`, and `pgy_try_input_result` preserve failure
  status/stage/operation while the legacy `FileOpen`, `FileExists`,
  `FileRead`, `FileWrite`, `ReadFile`, `WriteFile`, and `Input` compatibility
  ABI keeps returning `-1`, `false`, empty string, or no-op.
- P1-6 also has first-stage typed channel receive boundary evidence:
  `PgyRuntimeChannelFailure`, `pgy_channel_recv_result_*`,
  `pgy_channel_try_recv_result_*`, and
  `pgy_channel_recv_timeout_result_*` preserve closed/empty/timeout failure
  states while legacy `pgy_channel_recv_val_*` keeps returning a zero/NULL
  sentinel.
- `SubmitDeviceRead(...)` is a remote completion boundary: its worker consumes
  the typed DeviceSlot `PgyRuntimeSlotResult_*` read wrapper, so a released/null
  device slot completes as `RemoteFuture<T> -> Result.Err` instead of aborting
  the process from the worker thread.

Red-team drift guard:

- Do not claim a 128-bit Slot handle while the beta ABI remains 32-bit `slotId`
  plus 32-bit `generation`.
- Do not claim whole-language Coq/Lean mechanized safety until executable proof
  artifacts cover the theorem being advertised.
- Do not describe every runtime panic as recoverable. Recovery is a boundary
  contract; hard-fail classes remain hard-fail unless a named safe wrapper owns
  the conversion to `Result`.
- Primitive Slot, `DeviceSlot`, and `SecureSlot` panic functions remain the
  default sharp runtime ABI, but `PgyRuntimeSlotStatus`,
  `PgyRuntimeSlotFailure`, typed `try_*` read/write/release functions, and
  typed `PgyRuntimeSlotResult_*` read wrappers provide the recoverable boundary
  seam for host, FFI, and service wrappers that need to convert slot failure
  into data instead of aborting the process.
- File I/O compatibility functions still return `-1`, empty strings, or no-op
  for stable legacy callers, but the `pgy_try_*_result` I/O functions are the
  owner seam for recoverable host/service boundary reporting.
- Channel receive compatibility functions still return bool or zero/NULL
  sentinels for stable legacy callers, but the `pgy_channel_*_result_*`
  functions are the owner seam for recoverable channel-boundary reporting.
- `SlotManager` operations also expose `SlotRuntimeStatusFromError(...)`,
  `SlotErrorBoundaryRecoverable(...)`, and `SlotFailure.recoverable` so the
  manager-level `SlotError` path participates in the same recoverable boundary
  contract instead of remaining a separate string-only failure vocabulary.

Adding a new audit target: copy `templates/contract_template.md` (TBD, will be
created on first need), fill the invariant statement, source files, regression
coverage, and adversarial input shape.

## Last-Audited Tracker

Append-only. One line per audit run, latest at top.

| Date | Target | Tool | Result | Audit log |
|---|---|---|---|---|
| (none yet) | | | | |
