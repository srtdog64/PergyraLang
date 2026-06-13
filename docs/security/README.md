# Pergyra Security Audit Folder

Last updated: 2026-06-13

This folder is the home for **adversarial counterexample-search audits** of
Pergyra runtime safety contracts. It uses AI as a *Validator* (static
analyzer / fuzzer), not as a code Generator.

## What This Folder Is

- A structured place to record **invariant statements** that Pergyra runtime
  Tier 3 checks claim to enforce (generation, token, authority, channel
  rules, panic contract, etc.).
- A workflow for running **AI Validator harnesses** that try to construct
  counterexamples to those invariants.
- A log of **audit runs** (when, what target, what was found, what was
  exhausted).
- A log of **findings** (confirmed bugs, false positives, contract
  clarifications) with regression test references.

## What This Folder Is NOT

- Not implementation documentation (see `docs/03_security_mode_design.md`,
  `docs/15_compiler_security_modifications.md`). The file
  `docs/16_security_implementation_report.md` is a historical/imported
  snapshot, not the current beta source of truth.
- Not formal proof obligations (see `docs/semantics/`).
- Not regression test fixtures (see `tests/cases/`).
- Not a place to ask AI to *write* security code. AI Validator only
  *checks* existing code.

## Current Security Source Of Truth

The beta-facing security story has two active implementation surfaces:

- Generated `SecureSlot<T>` ABI: `src/runtime/pgy_abi_spec.h`,
  `src/runtime/pgy_runtime_slot_macros.h`, and
  `src/runtime/pgy_runtime_lib_secure_slot_exports.h` define
  `PgySecureSlot_*` plus `PgyToken_* { id, can_write, can_read }`. This is the
  stable language/backend ABI surface.
- `SlotManager` secure runtime API: `src/runtime/slot_security.h`,
  `src/runtime/slot_security.c`, `src/runtime/slot_security_crypto.c`,
  `src/runtime/slot_security_sealed_payload.c`, and
  `src/runtime/slot_manager_secure_ops.c` define `SecurityLevel`,
  `SecureToken`, `TokenCapability`, `HardwareFingerprint`, sealed payload
  policy, and crypto provider bindings. Windows uses CNG/BCrypt; non-Windows
  uses OpenSSL EVP/HMAC/RAND. This layer is runtime-manager evidence, not
  automatically the generated language ABI.

`SECURITY_LEVEL_*` syntax is parsed for compatibility with the design
direction. Until a named lowering owner connects that syntax to the
`SlotManager` policy layer, do not describe BASIC/HARDWARE/ENCRYPTED as active
generated `SecureSlot<T>` storage modes.

## Why This Workflow

The Swival rust-stdlib audit (2025-2026) demonstrated that AI in
**Validator mode** finds bugs that human reviewers and existing
regression tests miss. AI hallucination is a liability when generating
code, but in adversarial counterexample search the same enumeration
behavior becomes infinite-patience fuzzing. Real Heap Overflow and
correctness bugs were found in Rust stdlib `unsafe` blocks
(`CString::clone_into`, `slice::join`) using this pattern.

For Pergyra specifically:

- Tier 3 runtime checks (generation, token, authority — see
  `docs/118_slot_model_rigor_audit.md` §4.4) are the safety net that
  Tier 1/Tier 2 static rules fall back on. Their soundness is
  incompletely proven; only enumerated edge cases are regression-tested
  (currently `make test-security` 175/175). Adversarial audit covers
  unenumerated edge cases.
- Dual C/LLVM parity gates provide an automatic oracle: backends that
  produce different observable output are confirmed bugs.
- This partially closes the theory-depth ceiling identified in
  `docs/118` §10 by substituting adversarial counterexample search for
  mechanized proof.

## Index

- [`00_audit_methodology.md`](00_audit_methodology.md) — Generator vs
  Validator paradigm, workflow, tool requirements.
- [`01_audit_targets.md`](01_audit_targets.md) — Prioritized list of
  contracts to audit. Reflects Tier 3 / Tier 1-pending exposure.
- [`contracts/`](contracts/) — Invariant statements per contract,
  governed source files, known regression coverage, adversarial inputs.
- [`audits/`](audits/) — Audit run logs (date-stamped), one per run.
- [`findings/`](findings/) — Confirmed bugs and false positives, each
  with regression-test reference.
- [`templates/`](templates/) — Audit log and finding templates.

- [`02_red_team_threat_model.md`](02_red_team_threat_model.md) - Attacker
  tiers, kill-chain families, defense matrix, and beta exit bar.

## Cross-References

- `docs/security/02_red_team_threat_model.md` - red-team attack model and
  claim rules for current security surfaces.

- `docs/118_slot_model_rigor_audit.md` — Tier 3 invariants are the
  primary audit targets here.
- `docs/100_beta_readiness_checklist.md` §0 / §0d — formal semantics
  + secure invariant closure.
- `docs/semantics/04_ownership_abi.md` — secure token unforgeability
  theorem statement.
- `docs/semantics/08_slot_capability_calculus.md` — slot calculus
  proof sketch.
- `feedback_marketing_language_drift.md` (memory) — phrases to avoid
  while audit findings are open.
