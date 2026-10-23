# 2026-10-23: a callee reads of a copy's other side only the paths it names

Decided by: the Claude lane. The user can overturn it.

## Rule

After one side of a local copy changes storage in place, a later use of
the other side is refused
([rule](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)).
Handing a value that holds the changed part to a call counted as a use of
all of it. When the other side is an `inout` formal of the routine, such an
argument is now a use only when the callee reads the changed part: the
owner of formal read paths (`function_param_read_path_owner.c`) names every
access path at which the callee's formal appears, and a path the callee
hands to a declared callee of its own reads only what that callee reads,
to a depth of eight calls. A method called on a path reads its whole
receiver, and a routine with a deferred, concurrent or closure body, or one
whose claims cannot be built, reads everything.

Under a by-value or `ref` formal, or a local, the refusal stays, and lands
a copy where the copy was made when `Clone` copies the value deeply, so the
change stays the routine's own
(`local_copy_of_by_value_formal_passed_to_reader_of_other_rows_invalid`).

## Why it holds

The copy and the other side share the storage below the changed path. A
callee that reads none of it cannot see the change, whatever it does with
the rest; any appearance of the formal outside a call to a declared callee
counts as a read of its whole path there, so a local copy of the value in
the callee reads all of it
(`local_copy_other_side_aliased_whole_by_callee_invalid`). A callee that
reads the changed rows, or hands them to one that does, is still refused
(`local_copy_other_side_passed_to_reader_of_changed_rows_invalid`,
`local_copy_other_side_passed_through_to_reader_of_changed_rows_invalid`).
A callee that writes the shared storage is a move or a change of its own,
which the move walk takes from the callee's summary as before.

## Why

On the canonical driver input the semantic resolution passes
(`SemanticAstAnalysisResolveExpressionIdentities`, `...ResolveExpressionPlacesFromAdmittedBody`,
`...ResolveRoleOperatorTargetsFromAdmittedBody`, `...ResolveCallReturnTypes`,
`SemanticAstIterationSyntheticGraphRootsAttach`, `SemanticAstMatchMaterializationFactsAttach`)
copy their `inout` analysis's expression graph, set rows of the copy in
place, hand the analysis or its surfaces to readers of other rows, and
assign the copy back. Nine of their refusals were those readers.

A first version applied to any other side. It removed the copy refusal in
`SelfMirIntentExecutionCanonicalPlan`, whose landed copy had kept that
routine from changing its by-value `facts`: without it the routine's
summary moved `facts`, and its caller gained 22 refusals.

Replaces: nothing.
