# Pergyra Diagnostic Codes

Stable identifiers for compiler diagnostics. Designed for AI/tooling consumers: once a code is shipped, its **meaning** is frozen — message text may be refined, but the semantic condition it reports does not change.

## Format

All codes follow `PGY_<STAGE>_<REASON>`:

- `PGY_SEM_*` — semantic analyzer (type checker, slot analyzer, effect inference)
- `PGY_MIR_*` — MIR contract/validation (reserved, not yet assigned)
- `PGY_LLVM_*` — LLVM backend codegen (reserved, not yet assigned)
- `PGY_PARSE_*` — parser-level syntax errors (reserved — currently parse errors surface via `"stage":"parse"` without a code)

JSON output structure when `--error-format=json`:

```json
{
  "severity": "error" | "warning",
  "stage": "semantic",
  "code": "PGY_SEM_TYPE_MISMATCH",
  "location": {"line": 7, "column": 8},
  "message": "Type mismatch: cannot assign 'String' to 'Int'"
}
```

`code` is optional — legacy sites emit diagnostics without it. Downstream tooling should treat a missing `code` as "not yet routable" and fall back to message-text matching.

## Catalog

### Type System

#### `PGY_SEM_TYPE_MISMATCH`

Assignment or pass-site where the source type is not assignable to the target type. Covers `let`, argument passing, field assignment.

- **Reason**: static type rule — `from` is not a subtype of `to` and no coercion applies.
- **Fix**: change one side so types align, or introduce an explicit conversion.

#### `PGY_SEM_BINOP_TYPE_MISMATCH`

Binary operator applied to operands of incompatible types where the operator does not define a mixed-type rule (e.g. `Int + Bool` without overload).

- **Reason**: neither operand type drives a valid operator dispatch for the requested operation.
- **Fix**: unify the operand types (cast, refactor) or define a role-based operator overload.

#### `PGY_SEM_UNKNOWN_TYPE`

Type reference resolves to nothing: not a primitive, not a declared class/enum/trait/alias, not a generic parameter in scope.

- **Reason**: identifier used in type position is not declared or is out of visibility.
- **Fix**: import/declare the type; check module/namespace spelling; confirm export visibility.

#### `PGY_SEM_UNDEFINED_SYMBOL`

Identifier or member access where the symbol cannot be resolved. Distinct from `PGY_SEM_UNKNOWN_TYPE` (type position vs value position).

- **Reason**: value-level identifier not found in any accessible scope.
- **Fix**: check spelling, imports, and visibility modifiers.

### Type Inference

#### `PGY_SEM_INFER_COLLECTION`

`let x = ListNew()` / `SetNew()` / `MapNew()` / `QueueNew()` without an explicit type annotation — collection constructors are fully generic at this call site and the language requires annotation to pin the element type.

- **Reason**: constructor return type is generic; no inference source available.
- **Fix**: write `let x: List<Int> = ListNew()` (etc.).

#### `PGY_SEM_INFER_GENERIC`

Inferred type still has an unbound generic parameter after all constraints applied.

- **Reason**: initializer type contains a generic the checker cannot solve with the available context.
- **Fix**: provide a type annotation on the binding.

#### `PGY_SEM_INFER_REQUIRED`

`let` with neither a type annotation nor an initializer.

- **Reason**: no type source.
- **Fix**: add an annotation or initializer.

### Slot Ownership / Views

#### `PGY_SEM_SLOT_RELEASED`

Attempt to read, write, or view-through a slot that is statically known to be released in this scope. Covers both direct slot use and read/write through `ReadView`/`WriteView` whose source slot was released.

- **Reason**: ownership lifecycle requires a live source; release marks the slot as invalid for subsequent ops.
- **Fix**: re-Claim the slot before use, or remove the earlier Release.

#### `PGY_SEM_RELEASE_REQUIRES_OWNER`

`.Release()` called on a non-slot receiver or a view/handle that does not own the underlying resource.

- **Reason**: Release is a lifecycle operation on the owning slot identifier only.
- **Fix**: call Release on the owning `Slot<T>` / `SecureSlot<T>` identifier, not on a view.

#### `PGY_SEM_SLOT_DOUBLE_RELEASE`

Release called on a slot that is already in released state.

- **Reason**: double-release is never safe; the lifecycle invariant is "one Release per Claim".
- **Fix**: remove the redundant Release.

#### `PGY_SEM_VIEW_KIND_MISMATCH`

Read through `WriteView<T>` or Write through `ReadView<T>` — the view's capability does not match the requested operation.

- **Reason**: view kinds are capability tags; `ReadView` permits Read only, `WriteView` permits Write only.
- **Fix**: acquire the right view (`ReadView(slot)` / `WriteView(slot)`) or use the owning `Slot<T>` directly.

#### `PGY_SEM_MOVE_TOKEN_MISUSE`

Read or Write through a `MoveToken<T>`. Move tokens are one-shot ownership transfer handles with no in-place access.

- **Reason**: tokens are consumed by `Receive`/materialization, not accessed like slots.
- **Fix**: materialize the token into a `Slot<T>` via the receiving side (`let s: Slot<T> = token;`).

#### `PGY_SEM_MOVE_FROM_RELEASED`

`Move(slot)` or implicit move from a source slot that was already released/consumed.

- **Reason**: move transfers ownership; a released source has no ownership to transfer.
- **Fix**: re-Claim the source, or trace back to the earlier move/release that consumed it.

### Parallel / Effect

#### `PGY_SEM_PARALLEL_SLOT_CONFLICT`

Inside a `parallel` block, two or more tasks mutate or release the **same** slot. This is a hard error — data race by construction.

- **Reason**: owning writes across tasks must be disjoint; identity and alias analysis both trace to the same slot.
- **Fix**: split the slot into per-task slots; use a `Channel<T>` to serialize writes; or move the write outside the parallel block.

#### `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (warning)

Inside a `parallel` block, one task reads while another mutates or releases the same slot. Not a hard error (reader may be semantically fine in your domain), but flagged for review.

- **Reason**: read can observe partial/invalid state.
- **Fix**: synchronize via `Channel<T>`, take a snapshot before the parallel block, or demote the read to a `ReadView` acquired before the writer runs.

#### `PGY_SEM_EFFECT_CONFLICT` (warning)

Function body joins effect classes that the current partial-order treats as incompatible (typically `SECURE` + `REMOTE`/`COLLAPSE`/`NONDETERMINISTIC`).

- **Reason**: effect mask closure on the body produces a combination that the lattice flags as authority-sensitive work mixed with boundary/resource work.
- **Fix**: split the routine so each helper owns one effect family; isolate the conflicting branch behind an explicit boundary helper.

## MIR Contract (`stage: "mir_validation"`)

These codes fire before C or LLVM codegen reaches the native compiler; they indicate the MIR-level emission contract was violated or a required MIR artifact is missing. JSON stage is always `"mir_validation"`.

#### `PGY_MIR_UNRESOLVED_LOCAL`

A branch terminator or statement references an identifier that has no SSA-mapped local at the consumer block. Typically raised by `transpiler_expr_identifiers_mapped` for a destructure binding that was defined in a predecessor block but is not propagated through `ssa_entry_values` to the consumer.

- **Reason**: MIR SSA rename pass did not connect the def block to the use block for this name — often a shape the validator needs to learn about (e.g. a new destructure form).
- **Fix**: if a user-visible pattern (e.g. array destructure + conditional branch on a binding), register the bindings in `transpiler_register_with_alias_bindings_in_block`. If the validator is genuinely catching invalid MIR, trace back to the CFG pass that produced it.

#### `PGY_MIR_TOPOLOGY_INVALID`

MIR routine for a function is either missing, has wrong kind, or has no associated declaration AST. Raised by `transpiler_validate_mir_emission_contract` when `mir_routine == NULL`, `routine->kind != MIR_SCOPE_FUNCTION`, or `routine->ast == NULL`.

- **Reason**: HIR → MIR lowering dropped or misclassified the routine.
- **Fix**: inspect HIR for the function in question; verify the lowering pass recognizes the declaration form.

#### `PGY_MIR_SIGNATURE_UNSUPPORTED`

Function signature (parameters / return type) is in a shape the MIR emitter does not yet support (e.g. certain generic-bound combinations, exotic slot access modes, event handler types in positions the emitter has not learned).

- **Reason**: `transpiler_mir_function_signature_supported` returned false.
- **Fix**: either simplify the signature or extend the emitter.

#### `PGY_MIR_SSA_LIMIT`

Too many SSA locals in one emitted function (internal capacity 4096 per routine).

- **Reason**: The function has more distinct versioned locals than the emitter's static buffer handles.
- **Fix**: refactor the function into helpers; or raise the limit in `transpiler_emitters_base_b.inc`.

#### `PGY_MIR_INTENT_CARRIER_MISSING`

An `intent` step emission reached the codegen layer missing a required metadata carrier (pre/guard/post/expect/invariant/subintent/on-eval/zone/who/dispatch/compensate). Same code shared by C and LLVM backends (18 call sites in total) because the fix family is identical.

- **Reason**: HIR → MIR intent lowering did not materialize the expected instruction for this step kind.
- **Fix**: verify the intent step parses correctly; ensure the DIR → MIR intent lowering produces the matching carrier instruction.

## LLVM Backend (`stage: "llvm_codegen"`)

These codes fire inside the LLVM backend before the native compiler (clang/gcc) is invoked. JSON stage is always `"llvm_codegen"`.

#### `PGY_LLVM_SPEC_LIMIT`

Too many distinct `Result<T, E>` specializations in one translation unit (internal limit `MAX_LLVM_RESULT_SPECS`). Each unique `(T, E)` pair costs one specialization slot.

- **Reason**: either real fan-out across many error types, or accidental re-specialization due to type name normalization drift.
- **Fix**: collapse error types (reuse a shared enum), raise the limit, or audit `llvm_ensure_result_spec` for duplicate-detection regressions.

## Routing Guidance for AI Consumers

- Check `code` first; fall back to `stage` + message pattern if `code` is absent.
- Codes are stable across versions — safe to hard-code in auto-fix heuristics.
- Message text may change — do not regex the message unless you have to.
- Same `code` at different locations shares the same fix family; different locations may have distinct concrete fixes (e.g. `PGY_SEM_SLOT_RELEASED` on direct Write vs via ReadView/WriteView).

## Future Extensions

- `cause_ir` field (MIR/IR-level origin) and `fix_source` field (source-level patch hint) are planned but not yet wired. Catalog entries above describe the semantic fix; when those fields are added they will give AI a separate machine-parseable "what to change" channel.
- `PGY_MIR_*` and `PGY_LLVM_*` prefixes are reserved. MIR contract breaches (e.g. "unresolved SSA local at branch terminator") are the next priority once they are routed through the JSON sink.
- `related_rules` field will link each code to the language reference spec once that document exists.
