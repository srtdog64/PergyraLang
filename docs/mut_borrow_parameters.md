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

Until the backend step lands, use return-and-rebind (alternative one) for
value-safe accumulation; do not reach for blanket reference as a shortcut.

## Progress

Step 1 (parser captures mutability) is done and verified. The parser already
accepted `&`/`&mut` on parameters and types but erased the mutability:
FuncParam.mode (ParamMode DEFAULT/OWN/REF) had no mutable-borrow variant, and
the `&` handler consumed `mut` without recording it, so `&` and `&mut` both
became PARAM_MODE_REF. Added PARAM_MODE_MUT_REF (ast_types.h) and set it when
`&mut` is parsed, in both the regular (parser_decl.c) and async (parser_async.c)
parameter loops. Verified: pgy builds clean; `&mut xs: Array<Int>` parses on C
and LLVM; no regression across hello.pgy, dyn_test.pgy, the self-hosted lexer
and parser, and the `&mut self` receiver.

Steps 2 and 3 remain and are the larger pieces.

Step 3 concrete sites. The backend already branches on
`(mode == PARAM_MODE_OWN || mode == PARAM_MODE_REF)` to give own/ref parameters
their non-default ABI, in: transpiler_func_class_flow_emit.c,
transpiler_expr_call_user_emit.c, transpiler_mir_func_emit.c,
transpiler_func_forward_emit.c, llvm_boundary_slot_param.c, and
llvm_mir_type_helpers.c. PARAM_MODE_MUT_REF currently falls through to the
default value path, which is why a `&mut` parameter parses but does not yet
mutate the caller.

ABI (confirmed from emitted C). Array<T> lowers to a value struct, for example
PgyArray_Int, that holds its own data pointer, length, and capacity. ArrayPush
lowers to pgy_array_push_T(&arr, x): it already takes the address of the struct
and mutates it in place, and because the data pointer lives inside the struct, a
reallocation is written back through that same pointer. So single indirection is
enough; double indirection is not needed. The by-value bug is exactly that a
value-struct parameter is copied, so the callee grows the copy.

Important: those existing `(OWN || REF)` checks are Slot-specific. They gate the
`boundary_slot` path, which only triggers for `Slot<...>` / `SecureSlot<...>`
parameter types (e.g. transpiler_mir_func_emit.c sets
`boundary_slot = type is Slot<...> && (mode == OWN || REF)`). Array and other
value types never enter that path, so MUT_REF cannot simply be added to those
conditions. It needs a new, general parameter-as-pointer branch.

Backend work for `&mut T` (general, not slot-specific):

- in the param-signature emission, when `mode == PARAM_MODE_MUT_REF`, emit the
  parameter type as a mutable pointer to the value (`<CType>*`) instead of the
  value, for any type, not only slots; do the same in the forward declaration;
- in the function body, treat the parameter as already a pointer: a push or
  field write that would emit `&local` for a value local must emit the pointer
  parameter directly (pgy_array_push_T(param, x), not &param);
- at the call site, pass the address of the caller's value (&arg) for a MUT_REF
  argument, and require the `&mut` marker there for visibility;
- keep C and LLVM byte-identical and verify with the linter migrated to
  `func ScanStructure(content: String, diags: &mut Array<String>)`, which must
  reproduce the committed diagnostics with the in-place push.

### Implementation approach: copy-in / copy-out (value-result)

Making every body use of a `&mut` parameter pointer-aware (field access via `->`,
no address-of for push) would be pervasive and error-prone, because the body is
MIR-emitted and references the parameter as a value in many shapes
(`xs.length`, `&xs` for `pgy_array_push`, etc.). A cleaner, localized approach
is copy-in / copy-out:

    void P(PgyArray_Int *_xs_ref) {     /* param is a pointer */
        PgyArray_Int xs = *_xs_ref;     /* prologue: copy in */
        ... body unchanged: xs.length, pgy_array_push_Int(&xs, 1) ...
        *_xs_ref = xs;                  /* before each return: copy out */
    }
    /* call site */ P(&ys);

The body emission does not change at all; only the prologue, the write-back
before each return, the parameter signature, and the call site change. Under the
exclusive-borrow rule (step 2: one active `&mut`, no aliasing) copy-in/copy-out
is semantically identical to a true reference, so this is correct, not an
approximation.

The change is localized to transpiler_mir_func_emit.c plus the call site:

- parameter signature (~lines 230-300): for `PARAM_MODE_MUT_REF`, emit
  `<CType> *_<name>_ref` and the matching forward declaration;
- prologue (right after the function `{` / first MIR block label): emit
  `<CType> <name> = *_<name>_ref;` for each MUT_REF parameter;
- before each emitted return (transpiler_mir_func_emit.c ~644 `return %s;` and
  ~648 `return;`): emit `*_<name>_ref = <name>;` for each MUT_REF parameter, so
  multi-return functions still write back;
- call site (transpiler_expr_call_user_emit.c): pass `&<arg>` for a MUT_REF
  argument and require the `&mut` marker for visibility;
- mirror the same in the LLVM backend and lock C/LLVM byte-identical.

End-to-end proof: migrate the linter to
`func ScanStructure(content: String, diags: &mut Array<String>)` and confirm
linter_parity stays green with the in-place push.

There is already a working template for the prologue in the same function. The
role receiver path emits exactly copy-in today: around transpiler_mir_func_emit.c
lines 400-433, right after the function `{` and `ctx->indent++`, it writes
`<CType> self = *(<CType> *)_raw_self;`. The MUT_REF prologue is the same shape,
`<CType> <name> = *_<name>_ref;`, emitted at the same point for each MUT_REF
parameter. The one piece of plumbing is to stash each MUT_REF parameter's name
and rendered C type during the parameter loop (where they are already computed,
around lines 300-372) so the prologue and the return write-backs can reuse them.

So the exact recipe is: param signature at line 368 emits `<CType> *_<name>_ref`;
the prologue mirrors the role-self copy-in at lines 400-433; the write-back goes
before the returns at lines 644 and 648; the call site adds `&`. The role-self
path proves the pattern compiles and round-trips, which de-risks the prologue and
the value-result shape.

This is still a build-loop change (multi-return write-back and LLVM parity must
be verified byte-identical, and transpiler_mir_func_emit.c is on the active SoT
migration path), but copy-in/copy-out keeps it localized rather than pervasive,
and the role-self prologue is a working precedent to copy.

Step 2 (semantic exclusivity / borrow check) is independent of step 3 and can
follow it: one active `&mut` per value, no coexisting `&`, `&mut` required at the
call site.

## Implementation status (C backend done and verified)

Step 3 is implemented and verified on the C backend. A `&mut T` value parameter
lowers to a pointer parameter `T *<name>__mutref`; the function prologue copies
in `T <name> = *<name>__mutref;` so the body is emitted unchanged, and a
write-back `*<name>__mutref = <name>;` is emitted on every exit path. The
write-back point is the MIR return terminator (the real exit for MIR-emitted
functions), with the AST return path, MIR cleanup-block returns, and the
function-close fall-through covered as well; copy-in/copy-out helpers live beside
the defer machinery (`transpiler_emit_mut_ref_copyins` /
`transpiler_emit_mut_ref_writebacks` in `transpiler_defer_emit.c`). Both the
function definition (`transpiler_mir_func_emit.c`) and the forward declaration
(`transpiler_func_forward_emit.c`) emit the pointer parameter so the prototype
and body agree. The call site (`transpiler_expr_call_user_emit.c`) passes
`&<arg>` when the callee parameter is `&mut`.

Every new behavior is gated on `PARAM_MODE_MUT_REF`, so output for all programs
without `&mut` is byte-identical to before; the full 16-gate self-host parity
suite stays green on both backends. Verified end to end: a function that
`ArrayPush`es into a `&mut Array<Int>` parameter is observed by the caller
(length 1 then 4), and a multi-return function updates the caller array and
returns the new length on every branch.

Remaining: the LLVM backend lowering (the C backend is the verified reference;
the same copy-in/copy-out shape applies), step 2 borrow-check (exclusivity and
lvalue-argument enforcement — until then a non-lvalue `&mut` argument is a C
compile error rather than a Pergyra diagnostic), and migrating the linter's
`ScanStructure` to a `diags: &mut Array<String>` parameter once LLVM lands so the
linter parity gate stays green on both backends.
