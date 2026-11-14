# Type Resolution DAG Audit

**Updated:** 2026-05-26
**Scope:** beta-stable type references, generic defaults, where bounds, ability
consumers, module contracts, zone/world authority consumers, and alias-cycle
diagnostics.

## Current Position

The type-resolution DAG is active for the frozen semantic suite. The previous
private compatibility resolver body has been removed. The retired owner now
remains only as a quarantine sentinel; zero-only resolver and stage-materializer
telemetry was removed so the stats stream exposes active DAG facts instead of
dead compatibility counters.

Current local DAG gate output:

| Metric | Value |
| --- | ---: |
| graph-backed stage skips | 2087 |
| generic parameter nodes | 102 |
| DAG generic contract evidence | 165 |
| DAG ability consumer evidence | 76 |
| metadata entries | 3760 |
| metadata owned | 261 |
| metadata hits | 9380 |
| metadata dead ends | 0 |
| alias materialized | 6 |
| alias diagnostic unresolved inventory | 78 |

The 78 alias diagnostic unresolved entries are not recursive resolver calls.
They are alias-cycle diagnostic coverage and are separately gated by
`alias_diagnostic_cycle_unresolved=78`.

## What Is Closed

- Stable constructed refs materialize through graph/topo metadata; retired
  compatibility counters no longer exist as a semantic metric.
- Semantic owners are smoke-gated to consume metadata facts plus narrow
  diagnostic helpers instead of hand-rolled direct fallback calls.
- Signature-stage quiet resolution and stable constructed-type diagnostics now
  stay on metadata-only owner paths; the intermediate materializing type-ref
  helper has been removed from `src/semantic`.
- The direct materializer smoke allowlist is closed at zero. The obsolete
  `type_checker_resolve.c` owner is gone; `type_checker_resolution_retired.c`
  remains only as a quarantine sentinel.
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

1. Keep the retired resolver body absent and smoke-gated.
2. Prove every driver/backend path reaches stable type refs through DAG
   metadata or an owner-local metadata-first API.
3. Keep broader semantic and backend gates green while the deleted body stays
   absent.
4. Do not reintroduce zero-only `stage-compat-*` counters as proof of closure.
   Active proof must come from graph/metadata/evidence inventory.
5. Keep the retired resolved-annotation API closed at zero. Direct metadata
   readers are allowed only inside metadata owners; new semantic owners must
   move through metadata-first type-ref helpers instead of reopening annotation
   lookup.

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

`type-resolution-resolver-inventory-test-smoke` also gates
`semantic_type_resolution_lookup_resolved_annotation(...)` at zero. Its
reappearance is treated as debt resurrection; annotation-sensitive readers must
use metadata-first APIs or stay inside metadata owners.
