# Evidence-Projected LLVM Parameter Attributes

Status: `optimization slice, landed`. A member of the evidence-projection family
(`docs/142_evidence_driven_guard_amortization.md` — pay/prove the fact once for
safety, then reuse it as an optimization fact the backend cannot rederive from
lowered IR). This slice projects **ownership-mode evidence** onto **LLVM pointer
parameter attributes**.

The thesis in one line: Rust erases its ownership evidence before codegen;
Pergyra keeps `own` / `ref` / move first-class, so we can hand the backend facts
it otherwise has to assume.

## What changed

Owner: `src/codegen/llvm_decl.c`, in `llvm_forward_declare_func_with_signature`
(at the function `LLVMAddFunction` site, so the attribute is on the shared
function value the later definition fills in).

| Source evidence | LLVM param attribute | Why it is sound |
| --- | --- | --- |
| `own` pointer param | `noalias` | the callee uniquely owns it → it cannot alias any other accessible pointer during the call |
| default-mode **slot** param (a move) | `noalias` | default for a slot type is a move → uniquely owned, same as `own` |
| `ref` pointer param (ReadView) | `readonly` | `ref` is a read-only non-owning borrow → the callee never writes through it |
| `inout` (`MUT_REF`) | *(none)* | mutable value-result; deliberately excluded |

`param_attr[]` (0 none / 1 noalias / 2 readonly) is filled during the parameter
loop and applied after `LLVMAddFunction` (attribute index `k + 1`; index 0 is the
return value). The secure-slot token operand is a value, not a pointer, so it
never receives an attribute.

### `nocapture` → `readonly` correction

`ref` was first lowered to `nocapture` (a borrow cannot be stashed past the
call). Recent LLVM renamed that attribute, so `LLVMGetEnumAttributeKindForName`
returned 0 and it was a silent no-op. `readonly` is the semantically correct
attribute for a ReadView, is stable across LLVM versions, and is higher value —
it lets the optimizer hoist and CSE loads through the borrow because nothing
writes the pointed-to memory during the call.

## Soundness oracle

These attributes are **optimization-only**: they change what the optimizer may
assume, not program semantics — *unless the asserted fact is false*, in which
case the program miscompiles. So the **C↔LLVM backend-compare parity gates are
the soundness oracle**: a wrong `noalias`/`readonly` produces a different result
on one backend and the gate fails.

This matches the family's hard rule (`docs/136` §2): only *proven* evidence may
become a backend assumption. `own`/move uniqueness and `ref` read-only-ness are
language-guaranteed, not heuristic.

## Verification

- IR confirms emission: `define void @Consume(ptr noalias %0, { i64, i1, i1 } %1)`
  (own slot pointer `noalias`, token value untouched) and
  `define void @Touch(ptr readonly %0)` (ref ReadView `readonly`).
- A 24-case `backend_compare` sweep (10 slot/own/ref + 20 broad, overlapping)
  produces identical C/LLVM output, covering the `readonly` member across **both
  slot and non-slot** pointer params: `slot_subject_boundary_ref`,
  `subject_projection`, `object_layer_binding`, `secure_slot_view`, etc.
- `inout_caller_mutation` passes — confirming `MUT_REF` (mutable) is correctly
  **not** given `readonly`.

## Measured win

- `noalias`: **1.47x** on the pattern it unlocks (write through an owned pointer
  while reading another in a hot loop → the owned cell is register-promoted),
  measured in `benchmarks/perf_own_noalias.c`.
- `readonly`: enables load hoist/CSE through a read-only borrow; the magnitude is
  workload-dependent (loops that repeatedly read a ref view), not separately
  micro-measured here.

## Honest status

`noalias` (own/move) is clearly sound and has a measured win. `readonly` (ref) is
sound by the ReadView read-only-borrow guarantee and now verified across both
slot and non-slot ref params (24-case sweep, with `MUT_REF` correctly excluded).
A full all-cases sweep remains the exhaustive backstop; if it ever diverges,
narrow `readonly` to ref **slot** params (directly verified) or revert it.
`noalias` is independent and stays.
