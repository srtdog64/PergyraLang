# 02. Relation / Effect / Projection Proof Obligations

Last updated: 2026-04-25

Status: `IN PROGRESS / BLOCKER`

Keywords: `relation`, `effect`, `projection`, `refresh`, `publish`, `bind`.

## Stable Surface

- `relation` and `effect` declarations used by core contracts.
- `projection` source-to-target mapping.
- `refresh`, `publish`, and `bind` clauses.
- Projection diagnostics with target/source/projection kind/field path/fix.
- Dirty/ready plus epoch/cause propagation provenance.

Out of beta:

- Arbitrary effect algebra beyond the current authority-resource-effect partial order.
- Unbounded projection-chain recompute.
- General bidirectional lens laws.
- Deep multi-instance observability queries beyond the baseline.

## Judgments

```text
ModuleEnv; Gamma; Delta |- relation R ok
ModuleEnv; Gamma; Delta |- effect E ok
ModuleEnv; Gamma; Delta |- projection P ok
ResourceState |- refresh(P) => ResourceState'
ResourceState |- publish(E) => ResourceState'
ResourceState |- bind(R, E) => ResourceState'
```

## Theorem: Projection Freshness

A projection read observes either a ready value derived from the latest relevant source epoch, or triggers a bounded recompute that reaches a ready value or a contract failure.

Assumptions:

- Every projection edge has a known source field path and target field path.
- Duplicate field maps, missing source fields, ambiguous paths, and wrong projection kind are semantic errors.
- Runtime dirty state carries epoch and cause.

Current evidence:

- Projection, world-derived, embedded zone, branch-join, and handoff slices use dirty/ready plus epoch/cause provenance.
- C/LLVM parity smokes cover representative bounded recompute paths.

Remaining proof obligation:

- Generalize from covered slices to a bounded transitive frontier scheduler over the full world/zone/projection graph.

## Theorem: Effect Conflict Soundness

If two effects are accepted as joinable in a resource context, their authority-resource-effect order does not allow an unreported conflict.

Assumptions:

- Effect joins/meets/conflicts are computed against a canonical partial order.
- Authority denial and effect conflict are distinct failure classes.

Current evidence:

- Effect join/meet/conflict baseline exists.
- Diagnostics have `Reason:` and `Fix:` vocabulary for projection-related failures.

Remaining proof obligation:

- Promote authority-resource-effect ordering from implementation helper behavior to an explicit semantic contract.

## Theorem: Projection Diagnostic Completeness

Every rejected stable projection contract reports the projection source, target, kind, field path where applicable, reason, and fix.

Current evidence:

- Projection provenance diagnostics are stronger than the alpha baseline.
- Missing field and wrong-kind paths are covered in semantic regression.
- `projection-diagnostic-contract-test-smoke` gates the four beta-required diagnostic cases: missing source field, ambiguous source path, wrong projection kind, and duplicate field map.

Remaining proof obligation:

- Keep expanding this contract to branch/join/handoff propagation failures, not only declaration-time projection contract failures.
