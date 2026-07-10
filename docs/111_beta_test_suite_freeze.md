# Beta Test Suite Freeze

Status: beta-freeze-source-of-truth.

This document freezes the mandatory test gates for beta. Test count alone is
not a beta signal; the required signal is that each stable surface has a named,
repeatable gate.

Executable gate: `make beta-test-suite-freeze-test-smoke`.

## Mandatory Pre-Beta Gates

The beta release candidate must keep these gates green:

- `make test-all`
- `make test-semantic`
- `make test-transpile`
- `make test-abi`
- `make llvm-test-smoke`
- `make llvm-test-abi-same-process`
- `make backend-compare-inventory-test-smoke`
- `make backend-compare-llvm-coverage-test-smoke`
- `make llvm-test-backend-compare`
- `make llvm-campaign-projection-test-smoke`
- `make llvm-dnd-campaign-test-smoke`
- `make cfg-body-dataflow-test-smoke`
- `make type-resolution-dag-test-smoke`
- `make runtime-frontier-contract-test-smoke`
- `make runtime-panic-contract-test-smoke`
- `make runtime-panic-abi-test-smoke`
- `make runtime-panic-codegen-test-smoke`
- `make slot-contract-test-smoke`
- `make projection-diagnostic-contract-test-smoke`
- `make abi-ownership-shape-test-smoke`
- `make diagnostics-json-test-smoke`
- `make parser-lexer-diagnostic-test-smoke`
- `make diagnostic-registry-test-smoke`
- `make formal-semantics-test-smoke`
- `make proof-carrying-pipeline-test-smoke`
- `make proof-carrying-adequacy-test-smoke`
- `make air-drift-test-smoke`
- `make air-backend-nonimpact-full-test-smoke`
- `make air-strict-backend-compare-test-smoke`
- `make codegen-determinism-test-smoke`
- `make runtime-none-contract-test-smoke`
- `make raw-escape-contract-test-smoke`
- `make mir-declaration-inventory-test-smoke`
- `make semantic-inc-size-test-smoke`
- `make backend-inc-size-test-smoke`
- `make test-inc-size-test-smoke`
- `make transpile-strict-source-test-smoke`
- `make stdlib-test-smoke`
- `make package-module-resolver-test-smoke`
- `make unicode-policy-test-smoke`
- `make observability-schema-test-smoke`
- `make async-model-positioning-test-smoke`
- `make memory-concurrency-model-test-smoke`
- `make worker-boundary-ub-test-smoke`
- `make documentation-quality-test-smoke`
- `make self-host-preparation-test-smoke`
- `make tooling-conformance-test-smoke`
- `make perf-contract-test-smoke`
- `make evidence-guard-amortization-test-smoke`
- `make backend-fail-closed-test-smoke`
- `make language-contract-golden-test-smoke`
- `make boundary-migration-test-smoke`
- `make stable-identity-test-smoke`
- `make semantic-declaration-identity-test-smoke`
- `make hir-routine-identity-test-smoke`
- `make verified-projection-plan-test-smoke`
- `make beta-readiness-checklist-test-smoke`
- `make beta-test-suite-freeze-test-smoke`

## Platform Gates

- Linux beta gate: `make ci-linux`.
- Windows beta gate: `make ci-windows`, with LLVM gated only by executable
  `llvm-config --libs core` evidence.
- macOS preflight gate: `make ci-macos`, C-only until a dedicated LLVM contract
  is green.

## Explicitly Not Claimed Yet

- Fuzz testing is out-of-beta until a seed corpus and crash minimization policy
  exist. A bounded deterministic Pergyra-origin generator
  exists as an opt-in regression gate:
  `make self-host-fuzz-backend-generator-parity-test-smoke` proves the
  generator emits byte-identical C/LLVM corpora, and
  `make fuzz-backend-parity-test-smoke` additionally checks the generated cases
  against C/LLVM observable-output parity. A bounded multi-seed variant exists
  as `make fuzz-backend-parity-matrix-test-smoke`, but it is not a substitute
  for crash minimization or a persisted fuzz corpus.
- Property-based testing is out-of-beta until semantic properties are tied to
  the proof pack in `docs/semantics/`.
- Coverage percentage is not yet a beta acceptance metric. The beta gate is
  named stable-surface coverage, not raw line coverage.
- Long-running stress/performance sweeps are out-of-beta except for the
  bounded `make perf-contract-test-smoke` baseline and the ratio-based
  `make evidence-guard-amortization-test-smoke` evidence-view cache gate.

## Regression Policy

Adding a beta-stable surface requires adding or updating a named gate in this
file before it can be described as stable in `docs/107_beta_stable_subset.md`.
Removing a gate is allowed only when the corresponding surface is removed,
merged into a stronger gate, or explicitly moved out-of-beta.
