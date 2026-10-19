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

## Deep Clone landed (step 1, C and LLVM)

- **Copied deeply:**
  - a String;
  - an Array, by its runtime clone;
  - an Option's payload;
  - every field of a non-generic struct or class, recursively;
  - every field of a subject that is itself the Clone argument.

  One owner per backend does the copy: `transpiler_clone_value_emit.c` for
  C and `llvm_expr_clone_value.c` for LLVM. A parameter carried by address
  is copied from its value.
- **Independent result.** The result relation calls a Clone independent of
  its argument only when every part is copied
  (`clone_copies_deeply`). Four kinds of value still share storage:
  - a generic struct instance;
  - the elements of an Array of structs;
  - a subject field, which keeps its identity;
  - a zone.
- **Falsifier.** Before this change, a struct or subject Clone followed by
  pushes onto the copy's Array made the original read freed storage (ASan
  heap-use-after-free on main `7cd004fab`). Both main and this branch
  admitted those programs.
- **Still open:**
  - deep Clone in the self-host emitter (step 5). It cannot be run until
    the self-host driver builds on this branch.
  - Clone of an Array of structs, generic instances and zones.

## Loan copy landed for an owner change (steps 2 and 3)

- **Selection.** The move owner selects a lending call when its loan failed
  only because the owner changed (`function_param_flow_lent_argument_admitted`
  reports that). Two more conditions apply:
  - the call changes no storage it shares with its caller;
  - a Clone copies its result fully.

  The refusal stands in that analysis (`loan_copy_landing_owner.c`).
- **Application.** The driver wraps each selected call in place as
  `Clone(call)`, marked checker-built (`ast_wrap_nodes_in_builtin_call`),
  before the next analysis. A call whose result such a Clone copies lends
  nothing, so its argument is not moved.
- **A call that grows its argument in place is still a move.** Without that
  rule, seven move negatives such as `default_aggregate_move_reuse_invalid`
  were admitted. Cloning the result does not undo the growth in the
  caller's storage.
- **A loan that leaves is copied where it leaves.** The loan walk now
  records the expression whose value carried the first escape
  (`backing_effect_carried`), where a copy can stand in for it:
  - a returned value;
  - an argument the callee only receives (not `inout`);
  - a value assigned to a place outside the routine;
  - the value an Array store keeps.

  The move owner selects that expression, and the next analysis wraps it in
  Clone. A first exit no copy can stand in for (a receiver, an `inout`
  argument, a builtin's other arguments) selects nothing.

## Application landed for formal String stores

The two-run application of step 3 now exists, first for the String stores
that started most call escapes.

- **Selection.** Each analysis returns the store calls it selected, by
  stable id. Calls are selected at three points:
  - a refused owned push or ArraySet at check time;
  - a refused private push;
  - an escape-analyzer CALL on ArrayPush, ArraySet or MapSet.

  The String must be a path of a by-value or `ref` formal.
- **Application.** The driver reloads the sources and analyses them again
  with the plan, at most three times. A planned ArrayPush becomes
  ArrayPushOwnedString when its receiver admits an owned push, so an Array
  never mixes owned and borrowed elements. Any other planned store wraps its
  value in `Clone`.
- **Answered unknowns.**
  - Stable ids are the same on a fresh load of the same sources.
  - In plain mode no call is selected after the first error, so the
    canonical compile still runs once (80 s, 3.4 GB). The cost of the
    reanalysis shows only once the earlier refusals are gone.
- **Gates.**
  - `formal_store_landing_rounds` in the flow smoke counts the landing
    analyses.
  - Four former negatives are now admitted through a copy:
    - `mapnew_ref_string_value_copy_valid`;
    - `private_array_push_nonempty_alias_landed_valid`;
    - the two `named_string_same_empty_*_direct_landed_valid` fixtures.

## Unknowns to settle before step 3

- Do stable ids survive a fresh parse with the same source? The driver
  renumbers at `semantic.c:284`. A selection keyed by id needs the same
  numbering on both runs.
- The cost of a second semantic run on the canonical program. It is about
  75 s and 3.4 GB today.
- Whether other loan owners (named-call String lend, ref-result loans)
  should select through the same owner.
