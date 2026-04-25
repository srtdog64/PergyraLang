# 01. Intent / World / Zone Proof Obligations

Last updated: 2026-04-26

Status: `IN PROGRESS / BLOCKER`

Keywords: `intent`, `world`, `zone`, `subject`, `authority`, `handoff`.

## Stable Surface

- `intent` declarations and intent steps.
- `world` and `zone` declarations.
- `subject` and `subject slot` authority participants.
- `authorized by` step guards.
- Handoff and embedded world/zone propagation baseline.
- Observability queries: `last`, `history`, `active`, `recent`.

Out of beta:

- General distributed authority model.
- Unbounded world/zone graph scheduling.
- Full fairness theorem for fiber/coroutine scheduling.
- Arbitrary authority delegation algebra.

## Judgments

```text
ModuleEnv; Gamma; Delta |- intent I ok
ModuleEnv; Gamma; Delta |- world W ok
ModuleEnv; Gamma; Delta |- zone Z ok
ResourceState |- authorized_by(a, op) => ResourceState'
ZoneState; ResourceState; History |- intent_step => ZoneState'; ResourceState'; History'; outcome
```

## Theorem: Authority Soundness

If an operation guarded by `authorized by a` succeeds, then `a` resolves to an authority-bearing participant or slot that is valid in the current world/zone/resource state.

Assumptions:

- Participant aliases are resolved before runtime lowering.
- Same-type subject slots are ambiguous unless the alias directly identifies a concrete slot.
- Non-authority slots cannot satisfy authority-bearing contracts.

Current evidence:

- Direct subject-slot alias resolution handles same-type zone participants without stale ambiguity.
- Authority diagnostics distinguish concrete slot resolution from genuinely ambiguous same-type participants.
- Example smoke covers direct authority aliases in a multi-subject same-type zone.
- Runtime authority rejection exposes queryable `last_ok`, `last_zone`, `last_participant`, `last_code`, and `last_reason` state for missing-zone, missing-participant, and authority-token-mismatch failures.
- `pgy_runtime_authority_contract.h` is the shared source of truth for authority failure codes, reasons, and stderr formats across inline C runtime and LLVM runtime library exports.
- `runtime-authority-contract-test-smoke`, `authority_failure_abi`, and `authority_failure_surface` keep the C/LLVM surface aligned, including the token-mismatch query state.

Remaining proof obligation:

- Extend the same queryable rejection model into domain-boundary denial reasons.

## Theorem: Intent Step Progress

For a well-typed intent step, execution either advances the world/zone state, produces a recoverable failure, reports a contract violation, or hard-fails only on an internal invariant break.

Assumptions:

- `within`, `using`, `authorized by`, `causes`, and derived/inherited clauses have a canonical normalized contract.
- Missing or invalid step contracts are semantic errors, not backend surprises.

Current evidence:

- Intent diagnostics include fix-oriented hints for known clause misuse.
- C and LLVM both collect MIR intent authority metadata for runtime validation.
- Intent authority failure state now reuses the runtime authority contract surface instead of duplicating ad-hoc strings.

Remaining proof obligation:

- Contract clause density still needs canonical proof pairs for explicit vs compressed forms.

## Theorem: World/Zone Frontier Termination

A bounded world/zone frontier recompute terminates within the configured pass limit, either reaching a stable ready state or producing a failure class.

Assumptions:

- Frontier edges are finite after semantic lowering.
- Recompute is monotone with respect to dirty/ready epoch advancement inside one bounded pass set.

Current evidence:

- Dirty/ready plus epoch/cause provenance exists for covered world-derived and embedded-zone slices.
- Bounded recompute paths have C/LLVM parity smoke coverage.
- `runtime-frontier-contract-test-smoke` gates the C and LLVM emitter contracts
  for world derived-state bounded recompute, zone lifecycle bounded frontier
  loop, projection-chain bounded recompute, pass-limit overflow hard-fail, ABI
  smoke registration, and backend-compare registration.

Remaining proof obligation:

- General transitive frontier scheduler across the full world/zone graph is still the main runtime propagation blocker. The remaining work is broader authority/failure handoff and world-zone propagation generalization, not the absence of bounded frontier loops in the covered slices.
