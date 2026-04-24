# 03. Generics / Modules / DAG Proof Obligations

Last updated: 2026-04-25

Status: `IN PROGRESS / BLOCKER`

Keywords and surfaces: `where`, `ability`, generic parameters, default type arguments, module imports/exports, type-resolution DAG.

## Stable Surface

- Exact generic type arguments.
- Ability-bound generic contracts.
- Multi-bound `where T: A + B`.
- Default type argument actual resolution.
- Hidden/default-export module visibility.
- Stable constructed type refs: `List<T>`, `Set<T>`, `HashMap<String|Int, T>`, `Option<T>`, `Result<T,E>`.

Out of beta:

- Higher-kinded types.
- Full FP functor/applicative/monad typeclass hierarchy.
- Arbitrary type-family generalization.
- Arbitrary `HashMap<K,V>` key universes.

## Judgments

```text
ModuleEnv; Gamma; Delta |- T satisfies Ability
ModuleEnv; Gamma; Delta |- where T: A + B ok
ModuleEnv; Delta |- import/export ok
Delta |- provider before consumer
Delta |- no cycle
```

## Theorem: Generic Contract Soundness

If a generic instantiation is accepted, every required exact type, ability bound, multi-bound, and default argument contract is satisfied by the actual type arguments.

Assumptions:

- Default type arguments are resolved before consumer contract checking.
- Ability lookup respects module visibility.
- Hidden or non-exported abilities cannot satisfy imported contracts.

Current evidence:

- Generic default/constraint/where-bound staged resolution exists.
- Ability, authority, party, action, intent, and cross-module imported consumers have semantic coverage.
- `resolve_generic_type_arg(...)` is metadata-first before recursive fallback.

Remaining proof obligation:

- Make nominal/module/generic/default references graph-owned facts rather than fallback resolver facts.

## Theorem: DAG Soundness

If `Delta` accepts a program, every typed consumer has a resolved provider before semantic use. If `Delta` rejects a program, cycle/provenance diagnostics name the contract source, reason, and fix.

Assumptions:

- Type providers and consumers are registered before staged semantic checking.
- Graph-backed facts are immutable during a stage replay.
- Legacy fallback seams are explicit and inventory-gated.

Current evidence:

- Graph inventory, topo worklist, cycle provenance, metadata entries, metadata hits, and resolver inventory gates exist.
- Primitive and stable constructed type facts are materialized into graph metadata.
- Resolver inventory smoke prevents new direct `resolve_type_node(...)` calls outside approved seams.

Remaining proof obligation:

- Promote `Delta` to compiler-wide source of truth for all stable type dependencies.

## Theorem: Module Visibility Non-Interference

Hidden or non-exported declarations cannot be observed through imports, generic contracts, authority contracts, or backend declaration inventory.

Current evidence:

- Module taxonomy and manifest gates exist.
- Hidden/default-export visibility has semantic coverage.

Remaining proof obligation:

- Tie module visibility facts to DAG and MIR declaration inventory so semantic and backend paths cannot drift.
