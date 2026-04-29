# Type Resolution DAG Audit

**Updated:** 2026-04-29
**Scope:** beta-stable type references, generic defaults, where bounds, ability
consumers, module contracts, zone/world authority consumers, and alias-cycle
diagnostics.

## Current Position

The type-resolution DAG is active for the frozen semantic suite. The previous
private compatibility resolver body has been removed; only zero-call audit
counters remain so regressions can be detected. The stats labels use
`retired-compatibility-*` vocabulary to avoid implying an active fallback
resolver still exists.

Current local DAG gate output:

| Metric | Value |
| --- | ---: |
| graph-backed stage skips | 3140 |
| retired compatibility resolver calls | 0 |
| retired compatibility resolver body fallbacks / cache misses | 0 |
| metadata entries | 3363 |
| metadata owned | 257 |
| metadata hits | 6755 |
| materializer fallbacks | 0 |
| stage compatibility alias fallback | 0 |
| stage compatibility non-alias fallback | 0 |
| alias materialized | 6 |
| alias diagnostic unresolved inventory | 78 |
| alias diagnostic resolver calls | 0 |

The 78 alias diagnostic unresolved entries are not recursive resolver calls.
They are alias-cycle diagnostic coverage and are separately gated by
`alias_diagnostic_resolver_calls=0`.

## What Is Closed

- Stable constructed refs materialize through graph/topo metadata before any
  retired compatibility audit counter can be incremented.
- Semantic owners are smoke-gated to use
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)` instead of
  hand-rolled direct fallback calls.
- Signature-stage quiet resolution is also routed through that metadata-first
  type-ref helper after metadata preflight misses. The stage owner is no longer
  on the direct diagnostic-materializer allowlist.
- Stable constructed-type diagnostic argument resolution uses the same helper.
  The only remaining direct `semantic_type_resolution_lookup_or_materialize(ctx,
  ...)` call is the central metadata type-ref helper's fallback branch.
- The direct materializer smoke allowlist is narrowed to that central metadata
  owner only. The obsolete `type_checker_resolve.c` owner is gone; retired
  zero-call audit counters live in `type_checker_resolution_retired.c`.
- Stable constructed shell vocabulary is centralized in the metadata diagnostics
  owner. Constructed metadata materialization and fallback accounting now consume
  the same `stable shell` / `slot-like shell` helpers instead of maintaining
  parallel name lists.
- Central metadata materialization no longer falls through to
  `resolve_type_node(type_node, ctx)`.
- Metadata unresolved audit family accounting is fixed at zero:
  named, generic-named, compound, other, builtin shell, generic class, alias,
  non-class symbol, and missing-symbol fallback must all stay zero.
- Recursive alias resolver debt is removed. Alias materialization and cycle
  diagnostics are owned by metadata/stage DAG owners.
- The public `type_checker.h` surface does not expose a recursive type-node
  resolver.
- `type-resolution-resolver-inventory-test-smoke` rejects any reintroduced
  recursive evaluator or direct semantic owner call.

## Remaining Debt

The private compatibility evaluator body has been removed. Only the exported
audit counters remain, so `PGY_TYPE_RES_STATS=1` can continue proving the
retired surface stays at 0 calls / 0 body fallbacks.

The next cleanup target is therefore precise:

1. Keep semantic-suite compatibility calls at 0.
2. Prove every driver/backend path reaches stable type refs through DAG
   metadata or an owner-local metadata-first API.
3. Keep broader semantic and backend gates green while the deleted body stays
   absent.
4. Keep `stage-compat-*` counters at zero. They are compatibility audit
   counters for the retired resolver surface, not an active recursive resolver
   path.

Do not reintroduce a hard-crash compatibility path. Beta policy is explicit
diagnostic plus zero-use gate, not process abort.

## Evidence

Required gates:

```sh
make type-resolution-dag-test-smoke
make type-resolution-resolver-inventory-test-smoke
make test-semantic
```

The first two gates are the minimum DAG closure proof. `test-semantic` is the
broader regression sweep and should be run before claiming a source-of-truth
removal.
