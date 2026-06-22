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

and the first required AIR/MIR facts used by
`tests/proof_carrying_pipeline_smoke.sh`.

## Theorems

- `valid_certificate_allows_backend_consumption`
- `missing_air_authority_fails_closed`
- `missing_mir_expr0_fails_closed`
- `compat_success_policy_fails_closed`
- `negative_deletion_gate_required`
- `valid_certificate_requires_required_layers`
- `valid_certificate_requires_air_and_mir_facts`

## Live Adequacy Boundary

The Coq model is only useful when it is bound to live implementation facts. The
adequacy smoke requires:

- the Coq file names `FactOrFailClosed`, `AirRIRAuthority`, `MirExpr0`, and the
  fail-closed theorems;
- the Stage 1 smoke creates a `pgy.proof-carrying-ir.v1` envelope;
- the Stage 1 smoke deletes a required certificate fact and rejects it;
- the pass manifest lists `proof_certificate_pipeline`;
- the formal-smoke Coq loop type-checks `ProofCarryingIR.v` when `coqc` exists.

## Negative Scope

This is not whole-compiler verification. It does not prove that the C or LLVM
backend is correct. It proves the modeled checker rule: if the certificate is
valid, a downstream pass may consume the named facts; if a required fact is
missing, compatibility success is invalid and the pass must fail closed.
