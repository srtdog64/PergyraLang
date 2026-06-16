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

## LLVM backend implementation spec (mapped, ready to implement)

The LLVM backend uses the LLVM C API. The six emission points are located and
the recipe mirrors the verified C lowering one-for-one. Everything is gated on
`p->mode == PARAM_MODE_MUT_REF`, so programs without `&mut` produce identical IR
and all 16 self-host gates stay green.

1. Function signature type — `llvm_mir_emit.c`, the param-type loop (the
   non-slot `else` branch, just after the pointer-self block near line 337).
   After `param_types[i]` is set, add:
   `if (p != NULL && p->mode == PARAM_MODE_MUT_REF) param_types[i] = LLVMPointerType(param_types[i], 0);`
   so the `&mut T` parameter is lowered to `T*` in `LLVMFunctionType`.

2. Forward declaration — the same pointer-ization must be applied wherever the
   forward/prototype function type is built (`llvm_backend_forward_declare.c` /
   `llvm_domain_forward.c`) so the declared and defined function types agree.

3. ctx writeback state — `LLVMGenCtx` (`llvm_internal.h`) gains per-function
   parallel arrays: `LLVMValueRef mut_ref_ptr[N]` (the incoming pointer param),
   `LLVMValueRef mut_ref_alloca[N]` (the value-local copy), `LLVMTypeRef
   mut_ref_pt[N]` (the value type), and `int mut_ref_count`. Reset
   `mut_ref_count = 0` at function-param-emit entry
   (`llvm_mir_param_emit.c`, the params entry around line 169).

4. Parameter copy-in — `llvm_mir_param_emit.c`, the value-param block (lines
   305-308). Today: `alloca = LLVMBuildAlloca(builder, pt, name);
   LLVMBuildStore(builder, LLVMGetParam(fn, idx++), alloca); scope_declare`.
   For `&mut`, the incoming param is a pointer, so copy-in the value:

       LLVMValueRef ptr  = LLVMGetParam(fn, idx++);
       alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
       LLVMValueRef v = LLVMBuildLoad2(ctx->builder, pt, ptr, p->name);
       LLVMBuildStore(ctx->builder, v, alloca);
       llvm_scope_declare(ctx, p->name, alloca, pt);
       /* record (ptr, alloca, pt) into ctx->mut_ref_* for writeback */

   The body then references `p->name` -> the value alloca, unchanged, exactly
   like the C copy-in `T name = *name__mutref;`.

5. Copy-out write-back at returns — before every `ret`, for each recorded
   `&mut` param emit `LLVMBuildStore(builder, LLVMBuildLoad2(builder, pt,
   alloca, ""), ptr)`. The MIR return sites are `llvm_mir_block_emit.c` lines
   562 (`LLVMBuildRet`), 584 and 671 (`LLVMBuildRetVoid`); also
   `llvm_mir_emit.c:532` and `llvm_stmt.c` 93/110. This mirrors the C terminator
   hook `*name__mutref = name;`. A small helper
   `llvm_emit_mut_ref_writebacks(ctx)` called at each site keeps it one line,
   matching `transpiler_emit_mut_ref_writebacks`.

6. Call site — `llvm_expr_call_args.c` (the `LLVMBuildCall2` arg array around
   lines 80-85). For an argument position whose callee parameter is
   `PARAM_MODE_MUT_REF`, pass the argument's storage address (its scope alloca /
   lvalue pointer) instead of the loaded value, mirroring the C `&arg`. This
   needs the callee's `FuncParam` mode (already available via the routine /
   decl lookup the call path uses) and the argument's addressable slot.

Order of work: 3 (ctx state) -> 1+2 (pointer signature, inert until used) ->
4 (copy-in) -> 5 (write-back) -> 6 (call site), then build and run the existing
`/tmp/mut/t.pgy` and `t3.pgy` on `--backend=llvm` and confirm byte-identical
behavior to the C backend, then the full 16-gate parity. Once green, migrate the
linter's `ScanStructure` to `diags: &mut Array<String>` and confirm the linter
parity gate stays byte-identical on both backends -- closing the original
dogfooding loop.

## LLVM implementation status (function-definition side done and IR-verified)

Points 1-5 are implemented and verified: the `&mut` function-definition lowering
is correct on LLVM. For the test `AddOne(&mut xs: Array<Int>)`, the emitted IR is
`define void @AddOne(ptr ...)` with the copy-in load (`%v = load %PgyArray_Int,
ptr %mr_ptr`) into a value local and the write-back store before `ret`; the LLVM
verifier accepts the function body. Concretely landed:

- `llvm_mir_emit.c` -- `&mut` param lowered to pointer in the defined function
  type (signature).
- `llvm_decl.c` -- same pointer lowering in the registered/forward function type
  (this fixed the "registered function type drift" the signature change first
  exposed).
- `llvm_internal.h` -- `LLVMGenCtx` gains `mut_ref_ptr/alloca/pt[64]` +
  `mut_ref_count`.
- `llvm_mir_param_emit.c` -- copy-in (`LLVMBuildLoad2` from the pointer param
  into the value alloca) + records the triple; resets `mut_ref_count` per
  function; defines `llvm_emit_mut_ref_writebacks`.
- `llvm_mir_block_emit.c` -- calls `llvm_emit_mut_ref_writebacks(ctx)` before the
  three MIR return sites.

All of the above is gated on `PARAM_MODE_MUT_REF`, so the corpus (no `&mut`)
emits identical IR and the self-host gates stay green on both backends.

Point 6 (call site) is now done, and `&mut` works end to end on LLVM. The plain
user-function call does not go through the shared `llvm_emit_function_call_args`
helper or the `llvm_expr_call_dispatch.c` argument loops -- those serve intent
calls and the no-decl fallback. The real argument lowering for a function with a
declaration is `llvm_build_boundary_call_args`
(`llvm_expr_boundary_projection_helpers.c`), which already has the callee `decl`
and its `FuncParam` modes. There, before the value path, a `&mut` parameter
(`p->mode == PARAM_MODE_MUT_REF`) whose argument is an lvalue identifier passes
the argument's scope storage (`llvm_scope_lookup_snapshot(ctx, name, &mr_var)`
-> `mr_var.alloca`) instead of the loaded value -- mirroring the C `&arg`.

`&mut` is therefore feature-complete on both backends. Verified end to end on C
and LLVM: `AddOne(&mut xs: Array<Int>)` mutates the caller (`MUT OK len=4`), and
a multi-return `Grow(&mut xs, n)` returns the new length and updates the caller
on every branch (`MULTI-RET OK`). The full 16-gate self-host parity suite stays
green on both backends. The only redundant exploratory edits (in
`llvm_expr_call_args.c` and the `llvm_expr_call_dispatch.c` loop) were reverted
once the boundary path was found, keeping the change to the one call site that
matters.

Dogfooding loop closed: the self-hosted linter's `ScanStructure` was migrated
from the value-safe `ScanStructure(content) -> Array<String>` workaround back to
`ScanStructure(content: String, diags: &mut Array<String>) -> Void`, pushing
diagnostics directly into the caller's array. The linter compiles and runs on
both backends with byte-identical output, the linter parity gate stays green,
and the full `make self-host-preparation-test-smoke` passes -- so a real
self-host corpus tool now exercises `&mut` on both backends. The feature that
started from the linter's value-semantics workaround is now used by the linter
itself.

Step 2 borrow-check is now in place. Lvalue-argument enforcement already fell out
of the existing boundary-argument rule: a non-lvalue `&mut` argument (e.g.
`AddOne([1,2,3])`) is rejected at semantic time with "boundary arguments must use
a named variable", identically on both backends. Exclusivity was the real gap and
is now added in `type_checker_helpers_late.c`: passing the same variable to two
`&mut` parameters of one call (`Two(v, v)`) was a silent lost update under
copy-in/copy-out -- each `&mut` copies the value in independently and the last
copy-out wins, dropping the first push. The call-argument checker now collects the
identifier names bound to `&mut` parameters and rejects a repeat with "'v' is
passed as '&mut' more than once in the same call; each '&mut' argument must be a
distinct variable to avoid a lost update" (reusing the existing
`PGY_SEM_BORROW_ESCAPE` code so the diagnostic catalog stays at 66/66, no drift).
Verified: `Two(v, v)` is rejected on both backends, `AddOne(v)` and the working
`&mut` tests still pass, all 16 self-host gates stay green.

`&mut` is therefore complete: both backends, the dogfooding linter migration, and
the borrow-check (lvalue + exclusivity). The only deferred item is the
self-hosted reimplementation as a lowering layer (#134), for when the self-host
middle end reaches lowering.

## Self-hosted architecture: a lowering layer, not emission helpers

Directive: in the self-hosted compiler (written in Pergyra), `&mut` must be a
lowering layer, not the per-backend emission helpers the C and current LLVM
backends use. The helper approach was a pragmatic compromise forced by the
existing C codegen, which emits backend code directly; it is not the target
architecture.

Why the helper approach is wrong as an end state:

- The copy-out write-back is scattered across every return site, in every
  backend. The C backend needed the hook at the MIR terminator return, the AST
  return statement, the cleanup-block returns, and the function-close
  fall-through; the LLVM backend needs it at three more `ret` sites. Missing any
  one is a silent miscompile -- exactly the bug where the first C write-backs
  landed after the MIR terminator `return` and never executed.
- The same logic is duplicated per backend (`transpiler_emit_mut_ref_writebacks`
  in C, `llvm_emit_mut_ref_writebacks` in LLVM), so every backend re-derives the
  same control-flow reasoning and can drift.
- The signature pointer-ization, the copy-in, and the write-back are three
  coupled facts spread across five files per backend, kept in sync by hand.

The layer approach: lower `&mut` once as an IR-to-IR pass, before backend
emission, so the IR a backend sees already contains the copy-in and copy-out as
ordinary statements. A `func F(&mut x: T)` desugars to:

    func F(x_ref: Ptr<T>) {        // signature: pointer parameter
        var x: T = *x_ref;         // copy-in, inserted at entry
        ... body unchanged ...
        // before EVERY return (and the implicit tail return):
        *x_ref = x;                // copy-out, inserted by the pass
        return ...;
    }

The pass inserts the write-back the same way `defer` already runs at scope/return
exits -- in fact `&mut` copy-out is a defer-shaped obligation and should reuse
the same scope-exit machinery, so there is one place that knows "run these
actions before every return," and both `defer` and `&mut` register into it. The
backends then emit plain `load`/`store`; neither backend has any `&mut`-specific
code, and adding a third backend (WASM) costs nothing.

This generalizes. Effects, resilience (`retry` / `timeout`), `&mut`, and other
cross-cutting concerns are each a small layer over the IR rather than emission
hooks -- a nanopass-style middle end where each pass makes one transformation
explicit. The self-hosted compiler should be built this way from the start: the
semantic/HIR/MIR rungs already being grown in Pergyra are the place these layers
live, and `&mut` is the first concrete case study for the pattern. When the
self-hosted middle end reaches lowering, reimplement `&mut` as a desugaring layer
and delete the per-backend write-back helpers.
