# Prioritized Audit Targets

Last updated: 2026-04-26

This document lists Pergyra runtime safety contracts in priority order
for AI Validator audit. Priority reflects:

1. **Tier in `docs/118` §4** — Tier 3 (runtime fallback) is highest
   priority because static rules do not back it.
2. **Adversarial exposure** — security-relevant contracts (token,
   authority) before non-security contracts (panic class, OOB).
3. **Marketing claim weight** — contracts the language *publicly
   advertises* (e.g., "secure slot tokens cannot be forged") before
   internal invariants.
4. **Production user impact** — contracts that, if violated, lead to
   silent wrong results before contracts that lead to loud panics.

## P0 — Must Audit Before Beta Freeze

These contracts are load-bearing for marketing claims and cannot ship
to a 1-year freeze without adversarial coverage.

| ID | Contract | Source | Doc |
|---|---|---|---|
| P0-1 | Secure slot token unforgeability | `src/runtime/slot_security.c`, `src/runtime/security_types.c` | [contracts/secure_slot_token_unforgeability.md](contracts/secure_slot_token_unforgeability.md) |
| P0-2 | Slot generation stale-handle rejection | `src/runtime/slot_manager.c` | (TBD) |
| P0-3 | Authority transfer single-owner | `src/runtime/pgy_authority_runtime.c` (or equivalent) | (TBD) |
| P0-4 | TTL cleanup vs pin-state interaction | `src/runtime/slot_manager.c` | (TBD) |
| P0-5 | Release-while-pinned rejection | `src/runtime/slot_manager.c` | (TBD) |

Existing regression: `make test-security` 132/132 + `make runtime-panic-abi-test-smoke`. Audit covers
*unenumerated* edge cases beyond the 132.

## P1 — Strongly Recommended Before Beta

These contracts shape language guarantees but failures produce loud
panics, so silent-wrong-result risk is lower.

| ID | Contract | Source | Doc |
|---|---|---|---|
| P1-1 | Channel cross-World rule (cannot bypass) | `src/runtime/channel_runtime.c` (or equivalent) | (TBD) |
| P1-2 | Token transport reject (spawn / channel send / cancel) | `src/semantic/`, runtime ABI | (TBD) |
| P1-3 | Runtime panic class consistency (C vs LLVM parity) | `src/runtime/pgy_runtime_panic_contract.c` | (TBD) |
| P1-4 | OOM / divide-by-zero / OOB consistency | runtime panic | (TBD) |
| P1-5 | AIR drift detection completeness for declared kinds | `src/compiler/air.c`, `air_boundary.c` | (TBD) |

## P2 — Audit During 1-Year Freeze

These can wait for the post-beta freeze period.

| ID | Contract | Source | Doc |
|---|---|---|---|
| P2-1 | Pin block boundary rule (when Option C ships) | `src/parser/parser.c`, `src/semantic/` | (TBD) |
| P2-2 | WriteView<T> exclusive access (when Option C ships) | `src/semantic/` | (TBD) |
| P2-3 | Backend-compare parity completeness | `tests/cases/backend_compare/` | (TBD) |
| P2-4 | CFG body dataflow soundness | `src/compiler/hir_cfg.c` | (TBD) |
| P2-5 | Effect mask propagation soundness | `src/semantic/` | (TBD) |

## P3 — Post-1.0

| ID | Contract |
|---|---|
| P3-1 | Mechanized proof of P0-1, P0-2, P0-3 (Coq/Lean) |
| P3-2 | Concurrency interleaving coverage (Loom-style) |
| P3-3 | Side-channel analysis (timing, cache) |

## Contract Doc Status

Each entry above must eventually have a contract doc in `contracts/`.
Currently filled:

- ✅ P0-1 — `contracts/secure_slot_token_unforgeability.md`
- 🟡 P0-2 through P0-5 — TBD before first audit run
- 🟡 P1, P2, P3 — TBD progressively

Adding a new audit target: copy `templates/contract_template.md` (TBD,
will be created on first need), fill the invariant statement, source
files, regression coverage, and adversarial input shape.

## Last-Audited Tracker

Append-only. One line per audit run, latest at top.

| Date | Target | Tool | Result | Audit log |
|---|---|---|---|---|
| (none yet) | | | | |
