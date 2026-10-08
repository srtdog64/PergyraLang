# Proof-Carrying IR Certificate Core

This note explains
[`ProofCarryingIR.v`](ProofCarryingIR.v). It is a Stage 2 checker-core model for
the Stage 1 certificate envelope in
[`17_proof_carrying_pipeline.md`](../17_proof_carrying_pipeline.md).

## Scope

The model proves the checker contract, not the whole compiler:

```text
valid certificate + valid owner payloads => downstream fact consumption
missing required certificate fact        => fail closed
```

It models the certificate layers:

- AIR
- DAG/type
- MIR
- ABI/layout
- backend consumption

and the bounded AIR/MIR presence facts admitted by
`scripts/proof_certificate_admission.py`, the shared Stage 1 smoke validator.
`MayConsumeBackendFacts` remains permission within this model, not a call into
the C/LLVM backend.

## Theorems

- `valid_certificate_allows_backend_consumption`
- `missing_air_authority_fails_closed`
- `missing_mir_expr0_fails_closed`
- `compat_success_policy_fails_closed`
- `negative_deletion_gate_required`
- `valid_certificate_requires_required_layers`
- `valid_certificate_requires_air_and_mir_facts`
- `checker_reflects_certificate_validity`
- `bit_checker_reflects_certificate_validity`

Deletion rejection is a theorem about missing required facts, not a boolean
field on an untrusted certificate asserting that its own negative tests passed.

## Live Adequacy Boundary

`tests/proof_carrying_adequacy_smoke.sh` no longer treats keyword presence as
implementation adequacy. It freshly compiles the model and extraction module
in an isolated directory, kernel-checks their empty assumption budget, extracts
`check_bits` to OCaml, compiles it, then compares its decisions with the finite
predicate actually consumed by the shared envelope validator.

The comparison enumerates all 262144 valuations of the 18 boolean decisions:
five layers, eight AIR facts, four MIR facts and backend policy. Length/type
controls and envelope projection/deletion/policy/owner-consumption controls
check the adapter boundary. The envelope validator re-reads bound payloads at
admission; a required fact list alone does not supply the payload fact.

`make proof-carrying-adequacy-test-smoke` first runs the Stage 1 gate on live
compiler-produced AIR/MIR, then this finite checker gate. The Rocq 9 CI job can
run the finite gate independently of compiler bootstrap. A missing prover is
fatal by default; `PGY_ALLOW_MISSING_COQ=1` reports an explicit unchecked skip,
never an adequacy verdict. A prover without OCaml or a failed extraction is an
error, not a switch back to text checks.

## Negative Scope

This is not whole-compiler verification. It does not prove that the C or LLVM
backend is correct. The reflection theorem proves the Gallina boolean predicate
matches the logical contract; finite differential execution supplies observed
equivalence with the Python predicate, not a mechanized Python refinement proof.
Standard extraction, OCaml compilation and the byte-protocol bridge are trusted
execution boundaries. SHA-256/JSON admission is tested outside the finite model.
AIR/MIR presence checks are not a per-routine ownership proof, and DAG/ABI/backend
layers remain manifest-only. Source-to-model lowering, producer correctness,
signed issuance and production backend consumption are not established here.
