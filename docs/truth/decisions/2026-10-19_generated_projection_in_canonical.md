# 2026-10-19: a generated projection may change with its registry

Decided by: the user, on the Claude lane's question about `MapSize`.

## Rule

The canonical program stays unchanged in what people wrote. A file that a
`render_* --check` gate generates from a native registry, such as
`src/self_hosted/semantic/builtin_argument_retention_projection_owner.pgy`,
is regenerated when its registry changes, even though the canonical
driver imports it. The self-host checker then holds the same rule as the
native one.

## Why now

`MapSize` was missing from `builtin_argument_retention_registry.def`, so a
callee that only read the size of a by-value struct's map counted as
letting the map escape, and its caller's argument moved. Adding the row
needs the projection regenerated. The projection was also stale already:
`350515e50` added three in-place Array mutator rows to the registry
without regenerating it, which `builtin_argument_retention_registry_smoke`
refuses.
