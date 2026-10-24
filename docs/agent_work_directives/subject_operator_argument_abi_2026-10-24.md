# Native subject operator argument ABI

Status: see [lane state](../truth/state/operator_rule_reuse.md).
Base: `7313e9dfc6e45ffab10e7d5f665e576af4623f3d`.
User-authorized native ABI repair; observations and implementation candidates
are not ownership, self-host, installed-driver or CI closure evidence.

## Objective card

- Objective: a declared free operator consumes subject operands using the
  same argument ABI as an ordinary call, including a fresh constructor.
- Priority: identity and lifetime, one admitted ABI owner, single left-to-right
  evaluation, explicit failure, existing behavior, then patch size.
- Fact owners: MIR routine parameter type/carriage and nominal declaration
  headers; existing C subject argument policy and LLVM boundary call lowering.
- Last consumers: emitted C call arguments and LLVM call operands, matched to
  the same routine's function declaration/prologue.
- Forbidden fallback: pass a subject value to a pointer parameter; copy a
  named subject to invent addressability; evaluate an operand twice; infer a
  missing signature from runtime storage; let a role bridge overwrite a free
  operator's declaration.
- Gate: native operator exact-output/rejection runner on C and LLVM, with
  constructor, local, forwarded parameter, self, member, RHS subject and
  evaluation-order cases. Expected outputs are independent of the emitters.
- Falsifier: `Box() + 2` with `operator_add_Box(a: Box, b: Int)` must execute
  the declared function, not fail C compilation or LLVM signature validation.

## Whole affected chain and bounded changes

1. Semantic overload selection admits the free function before a role alias.
   The routine signature and nominal header already own pointer carriage.
   No ownership inference, safety rule or operator vocabulary changes.
2. C binary lowering currently captures values before invoking the operator,
   bypassing the ordinary-call address policy. Prepare arguments with that
   policy before capture: capture an existing storage address, not its copy.
   A constructor's compound literal must stay alive through the call.
3. LLVM binary lowering currently emits values before looking up the target.
   Resolve nominal targets before loading storage and reuse the boundary call
   argument owner. A primitive operand already emitted for resolution is
   consumed once, not emitted again; its actual result type matters for nested
   operators. Preserve role bridge value ABI separately.
4. LLVM role forward declarations must respect an existing admitted free
   routine even when its pointer ABI differs from a role's value receiver.
   Baseline execution also proves that LLVM's ordinary free-call boundary
   refuses a fresh constructor with indirect nominal carriage. Move the
   existing C constructor identity check into a shared codegen policy; LLVM
   may materialize that admitted fresh value, but not a copy-in/out temporary
   or arbitrary function result merely because it has the same type.
5. Verify last consumers with execution, including identity observed after
   mutation, operand evaluation order and existing ordinary-call behavior.
   Missing/unaddressable input must not become an unchecked pointer cast.

No new language syntax, manual drops, source reconstruction or synthetic
semantic identities. No edits to the main worker's ownership sources or
global truth files. No new worktree, push or installed compiler replacement.
Use a private build export of the committed base plus this lane's patches.

## Execution and integration

- One editor; no delegated implementation scope. This lane owns native
  operator/argument consumers, focused behavioral fixtures and its own state.
- Small local commits only; explicit path staging after diff inspection.
- Static/unit budget 60 seconds; focused runtime 5 minutes; integration
  shard 30 minutes. Do not weaken or skip a failing case to obtain green.
- Integration owner: main worker, including combined ownership changes,
  publication and required exact-SHA CI. Local integration gate:
  `native-compiler-boundary-regression-test-smoke`; preserve and report any
  baseline failure outside this ABI chain.

The [execution receipt](../truth/evidence/2026-10-24_native_subject_operator_abi.md)
records the two local slices, baseline counterexamples, independent expected
results, copy mutation and integration limits. It is evidence, not a new ABI
or ownership contract.
