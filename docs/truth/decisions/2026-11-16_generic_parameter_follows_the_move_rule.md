# 2026-11-16: a generic by-value parameter follows the move rule

The user chose this from the Claude root lane's design study (research
proposal B, which drew on Rust, Swift, Hylo and Mojo).

## The case

```
func Wrap<T>(self, value: T) -> Array<T> { return [value]; }
```

Native refuses this, and also refuses the same function with
`own value: T` or with `return [Clone(value)];`. So no generic function
can return a container built from a by-value `T`. The self-host admits all
three forms.

## The rule

- The 2026-10-18 move rule applies to a default-mode generic `T` the same
  way it applies to a concrete aggregate. Returning `[value]` moves the
  argument.
- The body is still checked once, with `T` opaque. The call site handles
  the obligations, by the concrete type:
  - a copy-only value such as `Int`: nothing moves;
  - an aggregate: it moves; a later use copies (decision 2026-10-22) or
    is refused;
  - a `String`: it is handed over as an owned element; a literal or a value
    read again is copied at the call.
- `own value: T` stays refused (`type_checker_ownership_call.c`).
- `Clone` of an opaque `T` is not admitted as a deep copy: with the body
  checked once, a struct or nested-Array instance would get a shallow
  copy.

## Must stay refused

1. `[v, v]`: the same formal held twice.
2. A returned container holding a `ref v: T`.
3. Wrapping a struct that holds an Array, then reading the struct again
   with no whole copy.
4. Wrapping a deeply nested Array, then reading it again.
5. A move inside the generic body followed by a use.
6. `Future` or `Channel` instances.

## Must run clean (ASan, LLVM IR at -O0)

- `String` reuse, with one `Clone` at the call.
- A `String` literal.
- `Wrap(xs)` followed by `ArrayPush(xs)`.
- `generic_member_array_return_flow` and
  `generic_member_record_array_return_flow`.

## Not chosen

- Checking each instance: ownership analysis would grow with the number
  of instances, and both front ends would change a lot.
- Keeping the refusal: generic container functions could not be written.
