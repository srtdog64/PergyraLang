# Mutable Borrow Parameters (Design Decision)

Status: accepted direction. Records why collections stay value-by-default and
why in-place mutation through a function should be an explicit, exclusive
`&mut T` borrow rather than blanket reference semantics.

## Problem (found by dogfooding)

The self-hosted linter (src/self_hosted/tools/linter) split its structural scan
into a helper, `ScanStructure(content, diags)`, which called ArrayPush on
`diags`. The pushes were lost: arrays pass by value, so the callee mutated a
copy and the caller's array was unchanged. The scan had to be inlined into Main
so every push shared one array.

This surfaced a real ergonomic gap: there is no way to hand a collection to a
function and have it mutate the caller's collection in place.

## Decision

Keep value semantics as the default for collections, and add an explicit,
opt-in mutable borrow parameter, `&mut Array<T>` (and `&mut T` generally). Do
not switch collections to blanket reference semantics.

## Why not blanket reference

Blanket reference reintroduces uncontrolled aliasing: two names bound to the
same array, where a mutation through one is visible through the other. That is
the exact class of bug the slot and ownership model exists to prevent, and it
contradicts the project's immutability leaning (ImmutableArray, parallel
map-to-immutable). Blanket reference would make the language look like every
other aliasing language and undercut the ownership story that is its
differentiator.

Value-by-default keeps three properties: predictable evaluation, no
spooky-action-at-a-distance, and safe sharing. Mutation through a function
becomes a deliberate, visible act, not a default.

## The design

A function that mutates a caller's value takes it as `&mut`:

    func ScanStructure(content: String, diags: &mut Array<String>) -> Void {
        ArrayPush(diags, ...);   // mutates the caller's array
    }

    // call site makes the borrow explicit
    ScanStructure(content, &mut diags);

Rules.

- Exclusivity: at most one active `&mut` borrow of a given value at a time, and
  no shared (`&`) borrow may coexist with it. This is what makes in-place
  mutation safe without aliasing: the borrow is exclusive, so there is no second
  name to observe the mutation unexpectedly.
- Default stays value: a parameter without `&mut` (or `&`) is a value, copied as
  today. Nothing existing changes.
- `&` shared borrow (read-only, possibly aliased) may be added later for large
  read-only inputs; it is not required for this decision.

## Consistency with existing receivers

Receivers already carry this distinction: methods take `&self` or `&mut self`
(added earlier in the project). `&mut` parameters are the same mechanism
generalized from the receiver to an ordinary parameter, so the model and the
borrow rules are already partly in the language; this extends them rather than
inventing a new concept.

## Alternatives considered

- Return and rebind: the helper returns the collection and the caller appends or
  reassigns. Pure and value-safe, works today, but threads the collection
  through return values and is awkward for accumulation across many call sites.
  This is the correct pattern to use until `&mut` parameters exist.
- Accumulator as a slot or handle: pass an owned DiagnosticSink handle and
  append through it. Very much in the language's grain, since the mutable thing
  becomes a first-class owned handle rather than a raw array, and the append is
  capability-bound. Worth offering as a library pattern even after `&mut`
  parameters land.

## Staging

1. Parser: accept `&mut T` (and later `&T`) on parameters, mirroring the
   receiver syntax already parsed for `&mut self`.
2. Semantic: enforce exclusivity (one active `&mut`, no coexisting `&`), and
   require `&mut` at the call site so the borrow is visible.
3. Backends: pass `&mut` parameters by address; value parameters keep copy
   semantics. C and LLVM must stay byte-identical.
4. Migrate the linter's ScanStructure back to a helper that takes
   `&mut Array<String>`, removing the inline workaround, and keep linter_parity
   green.

Until step 1 lands, use return-and-rebind (alternative one) for value-safe
accumulation; do not reach for blanket reference as a shortcut.
