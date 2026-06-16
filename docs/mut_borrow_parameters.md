# Inout Parameters

Status: accepted current contract.

Pergyra collections stay value-by-default. Caller-visible mutation through a
function is explicit and value-result, spelled `inout`. The older `&mut`
spelling has been removed because it borrowed Rust's sigil while providing
Swift-style copy-in/copy-out semantics.

## Problem

The self-hosted linter originally split structural scanning into
`ScanStructure(content, diags)`. Inside that helper, `ArrayPush(diags, ...)`
mutated only the callee's copy because arrays pass by value. The caller saw no
new diagnostics.

That behavior is correct for a plain value parameter, but it exposed an
ergonomic gap: a function sometimes needs to update the caller's collection
without turning all collections into aliased references.

## Decision

Use `inout` for value-result parameters:

```pergyra
func ScanStructure(content: String, inout diags: Array<String>) -> Void {
    ArrayPush(diags, "empty block");
}

ScanStructure(content, diags);
```

Rules:

- `inout` copies the caller value in on entry and copies it back on normal
  function exit.
- The argument must be an addressable named variable.
- The same named variable may not be passed to more than one `inout` parameter
  in the same call, because the last copy-out would otherwise lose earlier
  updates.
- A plain parameter stays a value. `ArrayPush(arr, x)` on a plain parameter is a
  local mutation of the callee's copy.
- `&mut` is rejected by the parser; `inout` is the only spelling for
  value-result mutable parameters.

The implementation mode is still named `PARAM_MODE_MUT_REF` because that name
exists in compiler internals. It is an implementation detail, not the language
surface.

## Why Not Blanket References

Blanket reference semantics would reintroduce uncontrolled aliasing: two names
could refer to the same array, and mutation through one name would become
visible through another. That is exactly the class of bug the ownership and slot
model is designed to keep explicit.

Value-by-default keeps sharing predictable. `inout` marks the rare
caller-visible mutation path and lets the checker enforce the copy-in/copy-out
hazards directly.

## Current Implementation

The parser accepts `inout name: T` for ordinary parameters and mutable receivers.
It rejects `&mut` binding modes and `&mut T` types with diagnostics explaining
that Pergyra uses value-result mutation, not live borrow types.

C and LLVM lower `inout` through the same shape:

```c
void F(T *x_ref) {
    T x = *x_ref;     /* copy in */
    ...
    *x_ref = x;       /* copy out before every normal return */
}
```

The self-hosted linter now uses:

```pergyra
func ScanStructure(content: String, inout diags: Array<String>) -> Void
```

so the dogfooding case that exposed the bug exercises the final surface.

## Self-Hosted Direction

The C and LLVM backends currently carry copy-in/copy-out hooks because the
existing compiler emits backend code directly. The self-hosted compiler should
not replicate that shape per backend. It should lower `inout` once as an
IR-to-IR pass before backend emission, using the same exit-obligation machinery
that will support `defer`-shaped cleanup.

That keeps `inout`, `defer`, resilience modifiers, and other cross-cutting
surface features as explicit middle-end transformations instead of duplicated
backend helpers.
