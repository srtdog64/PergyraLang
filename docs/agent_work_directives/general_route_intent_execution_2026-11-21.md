# General route intent execution (2026-11-21)

Status: implementation plan, one edit lane (Claude). Base revision
`e001e5223`. Integration owner: Claude; integration gate: the three intent
gates in `ci.yml` (`self-host-direct-mir-{legacy,composite,nested}-intent-program-llvm-test-smoke`)
and `direct_mir_nested_intent_program_c_owner.sh`, run with the intent routes
deleted. Outputs are implementation candidates; current source, registries
and executable gates stay authoritative.

## Objective card

- Objective: the general direct-MIR emitter compiles the programs the three
  intent routes (legacy, composite, nested) serve, on C and LLVM with
  runtime-equal output; then the three routes, their owners and their
  `mir.execution_graph` carrier rows are deleted (V1 self-host generality).
- Priority: one emitter organized by MIR instruction kind; facts from their
  owners (no source-spelling recovery); fail closed; behavioral gates.
- Forbidden: a new per-program-shape intent route; recovering a receiver
  field or an intent participant from text the declarations do not own
  (field lookup by name within the receiver's declared type, as member
  access already does, is the declared-name lookup, not recovery);
  skipping a backend; weakening an intent gate's negative.

## Whole chain (observed on the base revision)

- Production entry: `CompileAdmittedDirectMirMultiRoutineForTargetObserved`
  calls the legacy (LLVM only), composite (LLVM only) and nested (C, LLVM)
  intent routes before the general route. 5,649 lines in the three route
  families, 441 in shared `direct_mir_intent_*` owners.
- MIR lowers an intent routine to declarative instructions. The three gate
  programs use: `CommitIntent`, `Read` (resource-op); `IntentMode`,
  `IntentEval` (priority, `on`), `IntentCheck` (success, expect),
  `IntentBinding`, `IntentParticipant`, `IntentStep`, `IntentWho`,
  `IntentZoneWhere`, `IntentZoneAlias`, `IntentInvalidationTarget`,
  `IntentValue` (stmt); `cleanup-edge*`, `RollbackPolicy`, `AbortIntent`,
  `DetachInvalidation` (cleanup). Main reads observability builtins
  (`IntentLast*`, `IntentHistory*`, `IntentActive*`).
- With the intent routes not computed, the general route refuses all three
  programs at the same first point, then the same second one:
  1. `Zone(Clone(subject))`: `Clone` admits only String, `Array<Int>` and
     `Array<String>`, and the identity-cell lifetime owner admits only an
     inline constructor as a zone's subject slot.
  2. A subject action that reads and writes its receiver's field without
     `self.` (`value = value + 1`): MIR marks the write `owner_field`, but
     the read leaf carries `binding_kind: none`, so the routine's local
     inventory finds no local or parameter `value`. The explicit form
     `self.value = self.value + 1` already runs on the general route.
  3. Not yet reached: the intent routine body and the observability
     builtins.

## Slices (dependency order)

1. Receiver field access without `self.`. Observed MIR (self-host producer):
   `self.value = self.value + 1` is an `assign` instruction (target
   `self.value`, `arg1` `default_param`), which the general route already
   stores through the receiver cell. `value = value + 1` is a `def`
   (`arg0` `value`, `arg1` `owner_field`, result `value.1`), and the leaf
   `value` carries `binding_kind: none` and no `local_ref`. Locals in this
   MIR carry no `local_ref` either: the general route already resolves a
   leaf against the routine's local inventory by name and a member against
   its declared type's fields by name. So, in a routine whose receiver is an
   identity cell or logical record: the local ref plan does not count an
   `owner_field` definition as a local; the CFG plan stores it to the
   receiver's field, the operation the `assign` form gets; a leaf that is
   neither a local nor a parameter reads the receiver field of that name,
   and a name that is neither fails closed. An MIR owner-field binding kind
   would need the native producer, the self-host producer, the readers and
   the native/self MIR parity gates; it is not needed for this slice.
   Gate: implicit-field subject actions equal on four legs; an `owner_field`
   definition naming no receiver field is refused.
   Reached owners: `DirectMirIdentityCellStoreFromOwners`
   (`direct_mir_identity_cell_store_admission_owner.pgy`) admits only a
   three-node `self.field` target graph (an `assign`, or a `def` that
   rebinds `self`); the `owner_field` definition has a one-leaf target and
   defines the SSA value `value.1`. `DirectMirScalarCfgLocalRefPlanFromOwners`
   counts every `def` as a local definition named by `arg0`.
   MIR treats the field as an SSA variable: in
   `value = value + 1; value = value + 2; Log(value)` the defs use
   `value.0` (the field at entry) then `value.1`, and `Log` uses `value.2`.
   Design: a receiver-field local ref kind. The local ref plan gives an
   `owner_field` definition's `arg0` a local of that kind (its value rows
   `value.N`, entry `value.0` included); the local inventory matches it
   against the receiver declaration's fields instead of the source locals;
   the value type is the field's type; the C and LLVM emitters declare no
   storage for it, read it as the receiver cell's member and write each
   definition through to that member, so a call that changes the field
   between two reads is seen. An `assign`/`def` on `self` keeps its
   existing store path.
   Landed design (2026-11-25): not a local kind. A read of the field before
   any definition in the routine carries no use at all (`return value + 1`
   in a getter has `uses: []`), so the reads cannot all arrive as uses of
   `value.N`. The local ref plan keeps no local for an owner-field variable
   (its definitions and phis); an `owner_field` definition is the
   identity-cell store the `assign` form gets; an unbound leaf that is no
   local or parameter and names a receiver field is admitted as the
   receiver parameter and its identity-cell member, the nodes `self.value`
   gets, consuming its `value.N` uses in the value lane; its phis join
   nothing. Reads hit the cell, so a call that changes the field between
   two reads is seen. Owner: `direct_mir_receiver_field_binding_owner.pgy`.
2. `Clone` of a subject: a fresh identity cell copying the source cell's
   fields, admitted wherever an inline constructor is (a zone's subject
   slot included), on C and LLVM.
   Landed (2026-11-25): the record Clone lowering copies a subject or vessel
   cell into a fresh cell constructor; a zone or world Clone stays refused.
   With slices 1 and 2 the three intent programs first stop at their intent
   routine (`instruction_kind`, source `AST_INTENT_DECL`).
3. Intent routine body: one owner per intent instruction family in the
   general emitter, C and LLVM, reusing the shared intent owners where they
   already own a fact (completion, parameter binding).
4. Observability builtins read by Main.
5. Delete the three routes, their gates' route-specific checks, owners and
   carrier rows; the intent gates then run on the general route.

Each slice lands with its gate and CI green before the next one starts; a
slice that needs an unplanned owner revisits this map first.

## Unknowns to resolve before slice 3

Observed 2026-11-25 on the legacy program (native `--emit-c`):

- The intent routine's MIR is declarative (`CommitIntent`, `IntentMode`,
  `IntentEval priority`/`on`, `IntentCheck expect`/`success`,
  `IntentBinding`/`IntentParticipant`, `IntentStep`, `IntentZoneWhere`,
  `IntentZoneAlias`, `IntentInvalidationTarget`, `IntentWho`, then cleanup
  blocks). The self-host MIR document has no separate execution section.
- Native executes it as: `pgy_intent_enter_export(name, subjects, count,
  concurrent, priority)` over the `who` participants, failing to the
  cleanup block on a zero handle; per step, copy the participant into the
  zone slot (`zone->counter = *counter`), sync the zone, rebind the
  participant to the slot, run `on:`, sync, copy the slot back, restore the
  binding, then check `expect`; then `success`. The cleanup blocks call
  `pgy_mir_cleanup_op_export(handle, kind, arg0, arg1)` for each cleanup
  instruction (the rollback block only on failure) and
  `pgy_intent_exit_export(handle)`. The symbols already have self-host
  owners (`codegen/runtime_abi/intent_runtime_symbol_owner.pgy`).

Slice 3 design sketch (to confirm against the composite and nested
programs before editing):

- One intent routine owner reads an `intent` routine's declarative rows
  into one fact: name, mode, priority, the `who` participants (parameter
  ordinals), each step (zone parameter and slot ordinal from
  `IntentZoneWhere`/`IntentZoneAlias` and the zone cell's slot fields, the
  participant, the `on`, `expect`, `compensate` and `post` expression rows),
  `success` and `failure`, and the cleanup rows in block order. A row it
  does not know refuses the routine.
- Expressions go through the general expression admission. Inside a
  step's `on:` the participant's leaf is admitted as the zone slot member
  (as native rebinds the participant to the slot); everywhere else it is
  the parameter.
- C and LLVM emit enter, per-step copy-in / sync / `on` / sync / copy-out /
  `expect`, `success`, the cleanup calls and exit from that fact, with the
  existing zone sync and cell-copy code where it exists.
- Falsifiers: the three gate programs equal to native with the routes
  deleted; a forged row (an unknown intent stmt, a participant that is no
  parameter, a step zone that is not a zone parameter) refused.

- Whether intent `exclusive`/`priority` scheduling and compensation need a
  runtime ABI beyond what the routes emit today, and where that ABI's rows
  live (protocol/ABI registry).
- Which observability builtins each backend must provide for runtime-equal
  output.
