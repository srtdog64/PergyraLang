# HIR Routine Identity Contract

Status: `partial-gate-backed`

## Owner Chain

Routine linkage has one directional identity chain:

```text
semantic Symbol.decl_syntax_id
  -> AST call semantic target fact
  -> HIR direct-call declaration identity
  -> HIR RoutineId edge
```

Semantic analysis resolves a callable and records the resolved declaration's
nonzero `SyntaxNodeId` on the call. HIR lowering captures that target identity
beside the call spelling, builds a temporary `SyntaxNodeId -> RoutineId` index,
and stores only `RoutineId` values in the materialized callgraph edge set.

`RoutineId` is a nonzero, program-local, HIR-owned handle. Its current
canonical encoding is the one-based routine inventory position. The HIR
validator rejects noncanonical IDs, duplicate source identities, incomplete
direct-call identity arrays, and dangling callee IDs.

Names remain observability and diagnostic facts. They are not permitted to
select a callee. The public name query returns no result when a name/kind pair
is ambiguous instead of selecting the first routine.

## Boundary Cases

Calls without an internal HIR routine, such as builtin or external runtime
boundaries, do not create an internal callgraph edge. Their execution identity
must be owned by the relevant runtime-call ABI row; a name match must never
turn them into an internal routine edge.

If the target identity names an internal HIR declaration but no `RoutineId`
exists, callgraph construction fails closed. It may not reinterpret that loss
as an external boundary.

Hosted methods and top-level functions may have the same spelling. A call
resolved to the top-level declaration must reach the top-level `RoutineId`,
regardless of inventory order.

## Evidence

`make hir-routine-identity-test-smoke`:

- locks the semantic target producer and HIR identity fields;
- rejects name-index and direct-call-name consumption in the callgraph owner;
- proves the detector catches a synthetic regression to textual edge input;
- lowers a same-name hosted/top-level fixture and requires only the semantic
  target to become reachable.

The HIR unit suite additionally checks the exact callee `RoutineId`, validates
the graph, and requires the ambiguous public name query to fail closed.

## Remaining Obligation

This closes plain function-call linkage into the HIR callgraph. It does not
claim complete routine identity across receiver dispatch, generic
specializations, ability witnesses, extern/runtime-call ABI rows, MIR hosted
method joins, serialization, or compilation revisions. Those consumers must
extend the handle chain; they may not recover a target from a name, prefix,
owner/name pair, AST pointer, or source coordinate.
