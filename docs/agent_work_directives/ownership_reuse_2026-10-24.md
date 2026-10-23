# Ownership decision reuse

Status: IMPLEMENTATION COMPLETE for these two bounded reuse slices;
results belong to the lane's
[state](../truth/state/ownership_reuse.md).
Base: `fdc661ff9bbc97033cc2a9615ab4ab3e0d7f5f99`, shared main.
The user authorizes small verified local commits, not a push. Main's worker
owns publication, canonical integration and exact-SHA CI.

## Objective card and bounded chain

- Objective: preserve ownership decisions while removing duplicate copy-edge
  traversal and LLVM String-region scope checks. No language rule changes.
- Priority: stable identity/lifetime, identical admission and refusal,
  one decision owner, removal of duplicate paths, then patch size.
- Copy owner: `default_aggregate_copy_owner.c` records `MovedPath` edges from
  checked publication facts. The move walk calls both copy-change propagation
  and indexed-assignment loan checking. They must share one traversal over the
  same initial row set, preserving field overlap, edge order and direction.
  The copy-change consumer selects copies/refusals; the assignment consumer
  returns the first conflicting live loan. These effects remain separate.
- Lifetime: the traversal borrows field strings, owns its queue/visited set,
  and ends before rebinding/freeing the move state. A consumer may append rows
  and reallocate row storage; no cached row pointer may cross that call.
  Newly appended rows are not edges of this traversal.
- Region owner: the admitted `PgyRegionPlan` owns allocation disposition.
  LLVM Clone and string addition consume its scope through one LLVM boundary;
  argument coercion and nested-field eligibility remain with their consumers.
  The last consumer is the existing function-exit region destroy.
- Forbidden: AST reanalysis, a second alias graph, different field overlap in
  each caller, cached row pointers, root region grants inherited by nested
  aggregate fields, or a heap fallback after a granted scope is invalid.
- Falsifiers: disconnected/sibling fields, chained/reversed/cyclic edges,
  state reallocation, an early loan conflict, wrong/inactive function scope,
  missing region storage/runtime, escaping copies and deleted physical cleanup.

## Slices and integration

1. Shared copy traversal and both consumers; behavioral unit plus unchanged
   parameter-flow positives/negatives on an identified baseline and candidate.
2. Shared LLVM region consumer; actual C/LLVM receipt fault injection and
   String-copy physical-release/mutation gate.
3. Extend the existing project authoring skill with bounded reuse guidance:
   reuse search, semantic versus representation duplication, complete consumer
   migration, and behavioral falsifiers. A skill is advice, not enforcement.

This lane may edit the named owners/internal API, their focused tests/Makefile
entries, this directive/state and `.agents/skills/pergyra-authoring`. It does
not edit the main worker's dirty String-landing code, existing flow-summary
script or main truth-state files. No new Git worktree or branch. Validation
uses a temporary export of the base plus only this lane's patch.

Budgets: static checks 60 s, focused gates 5 min, build/integration 30 min.
Main's unchanged canonical/bootstrap/installed-driver/CI gate is still the
shared V1 finish line. This refactor neither replaces a C compiler path nor
closes V1. The outcome here is tested reuse, not a new SoT closure claim.
