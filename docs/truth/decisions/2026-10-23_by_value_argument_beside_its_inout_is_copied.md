# 2026-10-23: a by-value argument beside an `inout` one naming the same variable is copied

Decided by: the Claude lane. It extends the type checker's overlap rule
for `inout` and `ref` arguments of one call, and copies as
[a moved argument used again is copied](2026-10-22_moved_argument_used_again_is_copied.md)
does. The user can overturn it.

## Rule

When one call takes the same variable as an `inout` argument and as a
by-value argument of a type that shares storage (a struct, a tuple, a
collection, or an enum whose payloads hold storage), the type checker
refuses the call and selects the by-value argument for a copy
(`type_checker_helpers_late.c`). The next analysis passes `Clone` of it
there, when `Clone` copies a value of that type whole; otherwise the
refusal stays (`by_value_maps_beside_their_inout_invalid`). An Int or a
String argument shares nothing and is left as it was
(`scalar_and_string_beside_their_inout_valid`). Two by-value arguments
naming one variable are not this rule's
(`by_value_array_to_two_reading_formals_valid`).

## Why it holds

A by-value aggregate shares its storage with the variable it names, as a
`ref` does, and an `inout` argument beside a `ref` one naming the same
variable was already refused. The copy is the value the variable held when
the call began, which is what a by-value parameter means; the callee's
change goes to the variable alone.

## Why

On `b66b90a30` such a call was admitted with no copy. A push through the
`inout` formal freed the storage the by-value formal still read (ASan
heap-use-after-free, `by_value_argument_beside_its_inout_landed_valid`),
and a set through it changed what the by-value formal read (printed 55,
now 15: `by_value_argument_before_its_inout_landed_valid`).

Replaces: nothing.
