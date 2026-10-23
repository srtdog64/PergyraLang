# 2026-10-24: a declared-result check walks a member chain's qualifiers as path edges

Decided by: the Claude lane, repairing the integration of main's
`abedba79e` (GPT root lane) on the merged branch.

## Rule

`backing_origin` checks each node of a declared result for destinations
with `backing_destination_value_at(..., member_qualifier)`, where an
unpublished member access is a qualifier of a member chain published as
one path: its destinations are still followed to the chain's root.

## Why

Since `abedba79e` the type checker resolves a pure member chain such as
`cfg.instructions.ids` as one value path, so the qualifier
`cfg.instructions` has no publication row. The destination walk was
changed to pass such a qualifier, but the declared-result check in
`backing_origin` asked for the qualifier's own publication and left the
frame incomplete. On the canonical input `SelfMirCfgAddBlock`'s declared
result did, through `SelfMirRoutineBuildEmpty` and
`SelfMirRoutineBuildError`, and `SelfMirLowerLetFromArtifact`'s required
return relation failed the flow summary store closed for the whole
program. The normal walk of `backing_origin` already took an unpublished
member access as a path to its object; the declared check now agrees.

## Falsifier

`declared_result_member_chain_qualifier_valid` is admitted and prints
`no names` on C and LLVM; without the qualifier flag the flow summary
fails closed at `BuildError`.
