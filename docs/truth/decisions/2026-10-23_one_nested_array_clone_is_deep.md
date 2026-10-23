# 2026-10-23: one concrete nested Array layer is a deep Clone

## Decision

The C and LLVM backends copy an `Array<Array<T>>` independently when `T` is
a physical scalar or `String`. The ownership publication relation therefore
classifies that exact layout as a whole-value Clone and may land a compiler-
inserted Clone when a moving call's argument is used again.

Arrays nested more deeply, Arrays of structs, and generic instances remain
unsupported by this rule and fail closed. Supporting one layout is not a
recursive claim about arbitrary aggregate graphs.

## Evidence and boundary

Both backends already used the nested clone-into runtime for these concrete
leaves. The stale semantic classifier refused the later use even though the
emitted copy was physically independent. C and LLVM now pass the strengthened
nested-Array mutation falsifier, while the existing struct-Array negatives
remain refused.

This decision supersedes only the statement in
[the 2026-10-22 moved-argument decision](2026-10-22_moved_argument_used_again_is_copied.md)
that every Array of Arrays is shallow. Its move, last-use and visible-MIR rules
remain unchanged.

Evidence: [nominal-field storage identity and bounded nested Clone](../evidence/2026-10-23_nominal_field_and_nested_clone_identity.md).
