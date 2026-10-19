# Loan copy landing: whole-chain plan

Status: PLAN, Claude lane, single implementation lane. Base revision
`27af204a1` on `claude/p0-native-checker`. Decision:
[copy an unprovable getter loan where its proof fails](../truth/decisions/2026-10-20_escaping_loan_copies_where_it_fails.md).

## Objective card

- **Objective.** A getter loan whose proof fails is admitted through a
  compiler-selected `Clone`, not by moving its argument. The canonical
  census had 1313 such refusals at `2d30ec766`.
- **Priority.**
  1. The copy really is independent on C and LLVM.
  2. One owner selects each copy, and the selection is visible in MIR.
  3. A missing copy fails closed.
  4. The number of copies.
- **Fact owner.** The default-aggregate move owner already decides lend or
  move per lending call (`default_aggregate_move_owner.c`,
  `lent_use_admitted`). It becomes the owner that selects a copy, and it
  records the reason (escape or owner change).
- **Last consumer.** The type checker's `Clone` call on the selected node,
  then HIR, MIR, and the C and LLVM emitters of `Clone`. Later, the
  self-host general emitter.
- **Forbidden fallbacks:**
  - admitting a failed loan without a copy;
  - a copy that one backend emits shallowly;
  - a post-semantic AST rewrite that the collection effect snapshot does
    not cover;
  - a source change in the canonical program.

## What the survey found

- `Clone` of a String, Option or struct is shallow today. C returns the
  argument (`transpiler_expr_stdlib_builtin.c`); LLVM returns the value
  (`llvm_expr_call_dispatch.c`). Only `Array<T>` and C `Slot<T>` copy.
  - The semantic result relation agrees: a non-Array `Clone` copies
    shallowly (`function_param_publication_result_relation.c`).
  - The [deep struct Clone decision](../truth/decisions/2026-10-19_struct_clone_is_deep.md)
    is therefore not implemented.
- A node created after semantic analysis breaks the collection effect
  snapshot and the flow claims (`function_param_flow_claims_current`,
  `hir_collection_effect_snapshot_current`, `mir_validate_collection_effect_evidence`).
  The selection therefore cannot be applied after the snapshot.
- The ArrayPushOwnedString landing works because it changes a call during
  type checking, before its facts are recorded.

## Change set, in dependency order

1. **Deep Clone (C, LLVM).**
   - String: a fresh heap copy.
   - Option: Clone the payload.
   - struct, zone, subject: every field, recursively.
   - Array: its elements cloned as well, when they are Strings or structs.
   - The result relation then makes the result independent of its argument
     for these types.
   - Gates: C and LLVM runtime-equal fixtures that change the copy and read
     the original. The Array case uses an ASan falsifier, where a shallow
     copy plus pushes reads freed storage.
2. **Selection.**
   - The move owner records, for each failed lending call, either the
     escaping expression or the call, with a stable id. It records this
     instead of the move refusals.
   - A value with no deep Clone keeps the move.
3. **Application.**
   - The driver runs semantic analysis again on a fresh parse with the
     selection as input. The type checker wraps each selected node in
     `Clone` before it checks the node, as the push landing does.
   - The second run must select nothing new. If it does, the selection is
     incomplete and the compile fails closed with the remaining refusals.
   - Gate: the canonical run time and peak memory recorded, both runs.
4. **Fixtures.**
   - Each failure reason gets a positive (admitted, Clone in MIR,
     runtime-equal C/LLVM, ASan clean) and a negative (no Clone for its
     type: the move refusal stays).
   - Mutations: no selection; selection not applied; a shallow Clone.
5. **Self-host emitter.** Deep Clone in the general MIR emitter, under
   self-host generality.

## Unknowns to settle before step 3

- Do stable ids survive a fresh parse with the same source? The driver
  renumbers at `semantic.c:284`. A selection keyed by id needs the same
  numbering on both runs.
- The cost of a second semantic run on the canonical program. It is about
  75 s and 3.4 GB today.
- Whether other loan owners (named-call String lend, ref-result loans)
  should select through the same owner.
