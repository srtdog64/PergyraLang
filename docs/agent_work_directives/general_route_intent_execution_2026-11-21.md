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
- Forbidden: a new per-program-shape intent route; name matching to recover
  a receiver field or an intent participant; skipping a backend; weakening
  an intent gate's negative.

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

1. Receiver field identity. The self-host semantic already resolves an
   implicit receiver field for typing
   (`ast_expression_owner_field_environment_owner.pgy`). It publishes that
   resolution as an expression binding identity (a new owner-field binding
   kind with the field's syntax id and ordinal); the MIR writer and reader
   carry it; the general route reads it as a member of the receiver, and an
   `owner_field` definition as a store to that member. Native MIR without
   the identity stays refused. Gate: implicit-field subject actions equal on
   four legs; a leaf whose identity is forged is refused.
2. `Clone` of a subject: a fresh identity cell copying the source cell's
   fields, admitted wherever an inline constructor is (a zone's subject
   slot included), on C and LLVM.
3. Intent routine body: one owner per intent instruction family in the
   general emitter, C and LLVM, reusing the shared intent owners where they
   already own a fact (completion, parameter binding).
4. Observability builtins read by Main.
5. Delete the three routes, their gates' route-specific checks, owners and
   carrier rows; the intent gates then run on the general route.

Each slice lands with its gate and CI green before the next one starts; a
slice that needs an unplanned owner revisits this map first.

## Unknowns to resolve before slice 3

- Whether intent `exclusive`/`priority` scheduling and compensation need a
  runtime ABI beyond what the routes emit today, and where that ABI's rows
  live (protocol/ABI registry).
- Which observability builtins each backend must provide for runtime-equal
  output.
