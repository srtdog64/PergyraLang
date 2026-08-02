# Architecture Review Check, 2026-08-02

Source review: the attached `PergyraLang Architecture Review — August 2,
2026`, which observed `bac9b3f1` and named `a891851b` as its latest substantive
implementation. Checked on 2026-08-03 against role-operator implementation
commit `aa61503ad1762d9b3f14933cff71e247eb2d5a90`, role-receiver implementation
commit `9f4ec7442b6278b2494a42ac0e7a4952527c8736`, and the documentation update
recorded in `docs/current_work_handoff.md`.

This document adjudicates the review. It is not a semantic owner, a green-head
claim, or an independent work queue.

## Objective card

- Objective: retain the review's architecture constraints without reopening
  already closed aggregate rungs or diverting the active self-host executable
  path into a general redesign.
- Priority: producer-owned semantic target, one target-neutral plan, explicit
  selected-target ABI, fail-closed backend publication, then integration and
  bounded performance evidence.
- Fact owners: current semantic/MIR registries, callable receiver carriage plus
  its exact concrete role-target carriage, direct-MIR aggregate/role plan
  owners, and the executable gates named below.
- Last consumers: the selected C or LLVM emitter and, for the next rung, the
  self-host codegen role receiver binding boundary.
- Forbidden fallback: fixture/topology/name dispatch, layout treated as a full
  call ABI, backend semantic reconstruction, planner retry after classification,
  or stale binaries reported as current-source evidence.
- Gates: focused direct-MIR aggregate gates already recorded in the SoT
  registry, `tests/self_hosted/parity/one_mir_role_operator_projection.sh`, and
  `tests/self_hosted/parity/codegen_role_receiver_admission_owner.sh` for the
  separate current-source self-codegen receiver seam.

## Superseded findings

### Pair return/local aggregate carriage is closed

The review correctly identified aggregate return/local flow as the next blocker
at its observed head. That checkpoint is no longer active. The repository now
records installed C/LLVM substitutions for Pair return/local, Option<Pair>,
explicit and inferred generic nominal flows, mixed scalar inference, and
constructed member flows. The shared
`DirectMirAggregateValueFlowFact`/target-projection boundary owns the promoted
target-neutral decisions. The active handoff must not revive `BuildPair` merely
because the older review described it in more detail.

### The reviewed integration and performance numbers are historical

The review's 96.2-second/3,655,177-byte driver and earlier 2.705 GiB seed run do
not describe the current binary. The current role slice installed a
4,226,751-byte driver after a 106.7-second official build. No memory run was
performed for that slice, so no older peak is relabelled as current. Full CI,
Coq/Rocq, pressure, bootstrap fixpoint, and current-source gen2==gen3 remain
explicitly unrun.

The SoT authority edge gate was run and is RED on five pre-existing duplicate
Coq fact assignments in the registry. This role slice updates one existing
receiver-carriage row but adds no duplicate fact authority. The duplicates must
be consolidated or receive distinct proof facts before a future green-head
claim; silently accepting repeated proof authority would contradict the review's
single-owner requirement.

## Findings retained as active constraints

### Storage layout and call ABI remain distinct

The role slice applies the review's most important ABI distinction. The
target-neutral plan carries semantic `Int` parameter/result facts. A separate
selected-target view maps them to C `long long` or LLVM `i64`, alignment 8,
direct receiver-pointer passing, and direct scalar argument/return. This is a
bounded closed-module contract, not a Win64/SysV/AArch64 interoperability claim.
Cross-target native ABI classification remains open and must not be inferred
from storage size/alignment alone.

### Full legalization is required after route claim

The producer carries `call_target_kind="role_operator"` and an exact target
identity. Once present, malformed declaration, signature, receiver, graph, use,
CFG, or ABI facts reject before artifact publication. The dispatcher cannot
reinterpret the graph as primitive addition or retry a different multi-routine
or native path. Six metamorphic cases and 27 negative mutations enforce this
bounded legalization boundary.

### Topology-specific owner growth remains a ratchet concern

The role implementation is kept to four responsibility owners: declaration
admission, target-neutral plan, selected-target ABI projection, and final dual
emission. It reuses the existing multi-routine dispatcher, target capability,
routine/use/graph facts, and canonical `Int` ABI. No fixture, expected output,
`IntMath`, `Arithmetic`, or `Add` spelling is a router condition. The family is
hard-capped by the component contract so one successful slice cannot normalize
unbounded owner growth.

### Stable query infrastructure is still deferred

The compiler-scale no-role path now avoids a repeated cumulative expression-
graph validation and accepts the current codegen source graph. This is an owner-
boundary correction, not a cache or query engine. Incremental dependency
infrastructure remains deferred until stable identity and invalidation keys are
proven by a reached executable blocker.

## Current routed result

The direct-MIR role-operator slice is bounded `SUBSTITUTING`: the same
5,143-byte MIR artifact executes exact `123` under C and runtime-free LLVM.
The separate current-source Pergyra codegen receiver seam is now closed for the
bounded fixture. The earlier diagnostic was not evidence of a missing nominal
kind: `IntMath for Int` targets a compiler-ABI builtin value, not a nominal
declaration. Treating `Int` as `nk:struct` or another invented nominal row would
have violated the review's single-owner rule.

`CodegenCallableReceiverFacts` now binds exact callable identity to two distinct
axes: the erased role method ABI remains `mutable-identity`, while the concrete
target `Int` is admitted as a plain `value`. Source nominals, enums, and compiler
ABI builtins are mutually exclusive target authorities; a same-name collision,
missing target, unsupported builtin ownership shape, or stale parallel row
fails before emission. Function emission consumes this admitted fact and no
longer rescans role declarations. The final binding owner no longer reads
`target_type + nk` from the generic string environment.

The current Pergyra-built sibling emitted a 3,889,557-byte current codegen C
artifact. Its 2,351,659-byte host-compiled tool emitted `operator_add.pgy` to C,
which compiled and ran exact `123\n123\n3`. The focused gate also changes the
role body `123` to `321` and observes exact `321\n321\n3`, then rejects a
non-copyable builtin role target with no partial C. This is bounded self-codegen
execution evidence; it does not promote the whole compiler or the general
cross-target call ABI to complete.

## Post-review executable follow-up

The next reached production defect was not another aggregate feature. The
installed root inferred file publication from any two positional strings, so
the legitimate `source --emit-c-verified` stdout request created a file named
after the option. Commit `9a8e3dbc96c5b6e46200246a5af929c765a94a05`
removes that positional fallback and gives source C file publication the exact
mode `--emit-c-artifact-verified source output`. A focused installed-binary gate
now rejects legacy, unknown, missing, and option-shaped output requests before
publication and is a dependency of default C compile replacement.

This confirms the review's broader warning: a passing downstream compile did
not prove the argv ownership boundary was coherent. The next rung therefore
does not add features. It replaces the remaining duplicate installed/standalone
argv interpretations with one typed request admission owner before I/O.
