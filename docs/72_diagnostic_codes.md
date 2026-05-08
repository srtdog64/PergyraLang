# Pergyra Diagnostic Codes

Stable identifiers for compiler diagnostics. Once a code is shipped, its **meaning** is frozen — message text may be refined, but the semantic condition it reports does not change.

## Format

All codes follow `PGY_<STAGE>_<REASON>`:

- `PGY_SEM_*` — semantic analyzer (type checker, slot analyzer, effect inference)
- `PGY_MIR_*` — MIR contract/validation (shared by both backends)
- `PGY_AIR_*` — AIR verifier invariant failures (compiler IR contract)
- `PGY_C_*` — C backend codegen (transpiler lowering MIR → C)
- `PGY_LLVM_*` — LLVM backend codegen (MIR → LLVM IR → object)
- `PGY_PARSE_*` — parser-level syntax errors
- `PGY_LEX_*` — lexer/tokenization errors
- `PGY_DRIVER_*` — driver/CLI contract gates

JSON output structure when `--error-format=json`:

```json
{
  "severity": "error" | "warning",
  "stage": "semantic",
  "code": "PGY_SEM_TYPE_MISMATCH",
  "cause_ir": "semantic:assignability_check",
  "fix_source": "annotate-or-convert",
  "location": {"line": 7, "column": 8},
  "message": "Type mismatch: cannot assign 'String' to 'Int'"
}
```

`code`, `cause_ir`, and `fix_source` are all optional. Legacy sites emit diagnostics without them. Consumers should treat a missing `code` as "not yet routable" and fall back to message-text matching.

> **Source of truth**: All `code`, `cause_ir`, and `fix_source` literals are
> `#define`d in [`src/semantic/diag_codes.h`](../src/semantic/diag_codes.h).
> New diagnostic sites MUST reference the macros from that header rather than
> using bare string literals. Adding a new literal requires updating both
> `diag_codes.h` and this document. Existing call sites that still use bare
> literals are migrating incrementally.

### `cause_ir` — IR-level origin tag

Identifies **where inside the compiler pipeline** the diagnostic was raised. Format is `<stage>:<subsystem>:<condition>`, e.g. `semantic:assignability_check`, `semantic:slot_lifecycle:write_after_release`, `llvm:result_spec:capacity_exceeded`. Stable across versions. Useful when the same `code` fires from multiple IR paths — `cause_ir` disambiguates which IR layer reported the breach.

Naming rules:

- Use lowercase ASCII tokens only.
- The first segment is a compiler stage: `lex`, `parse`, `semantic`, `mir`, `c`, `llvm`, or `io`.
- The second segment is the smallest stable subsystem that a downstream tool can route on.
- The final segment names the IR/compiler condition, not the free-text user message.
- Do not encode source-line wording, type names, or runtime values in `cause_ir`.

### `fix_source` — source-level repair action tag

Compact, stable token describing **what to change at the source level**, e.g. `annotate-or-convert`, `reuse-shared-error-enum`, `reclaim-before-use`, `reclaim-source-or-drop-view`. Distinct from the free-text `message` because message wording can be refined while `fix_source` remains stable. One `code` may map to multiple `fix_source` values when the same semantic condition admits different concrete repairs (e.g. `PGY_SEM_SLOT_RELEASED` uses `reclaim-before-use` for direct slot misuse and `reclaim-source-or-drop-view` when the release is upstream of a view).

Naming rules:

- Use lowercase ASCII action tokens joined by hyphens.
- Describe the source edit class, not the compiler stage.
- Keep the token stable even if the human `Fix:` sentence is rewritten.
- Prefer one reusable token per repair class; add a new token only when downstream tools need a distinct action.

## Registry Gate

`make diagnostic-registry-test-smoke` enforces the semantic diagnostic registry contract:

- Every `PGY_CODE_*` literal in `src/semantic/diag_codes.h` must be documented in this file.
- `semantic_error_with_hints` and `semantic_warning_with_hints` call sites must pass `PGY_CODE_*`, `PGY_CAUSE_*`, and `PGY_FIX_*` macros, or explicit `NULL` / `0` for unrouted fields.
- Function declarations/definitions and comments are ignored; the gate is about real semantic diagnostic call sites.

`make parser-lexer-diagnostic-test-smoke` enforces the parser/lexer routing gate: lexer and parser errors must include `Code:`, `Reason:`, and `Fix:` fields backed by `diag_codes.h` macros, and the driver must route those codes into JSON `stage`, `code`, `cause_ir`, and `fix_source` fields.

## Catalog

### Parse / Lex

#### `PGY_PARSE_SYNTAX`

Parser-level syntax failure: missing expected token, unsupported clause shape, duplicate clause, or malformed declaration/body surface.

- **Reason**: parser saw an unexpected token for the current grammar production.
- **Fix**: check syntax near the reported line/column.

#### `PGY_LEX_INVALID_TOKEN`

Lexer/tokenization failure: invalid character or unterminated string family.

- **Reason**: source text cannot be tokenized into the stable lexical surface.
- **Fix**: remove the invalid character, escape it, or close the literal.

### Driver / CLI Contract

#### `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`

`--runtime=none` rejects a source that uses runtime-dependent surface, or a
source that otherwise reaches the current freestanding-lowering blocker. The
mode is beta-gated so the driver cannot silently compile through the default
scheduler/arena runtime while claiming no-runtime semantics.

- **Reason**: the requested no-runtime contract cannot lower scheduler,
  channel, intent, zone/world, event, or freestanding backend contracts yet.
- **Fix**: use `--runtime=default`, remove runtime-dependent surface from the
  freestanding build, or wait for verified freestanding C/LLVM lowering.
- **cause_ir**: `driver:runtime:none_unsupported`
- **fix_source**: `use-default-runtime-or-remove-runtime-surface`

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

#### `PGY_SEM_UNINIT_LOCAL`

Function-body `let` has a type annotation but no initializer (e.g. `let x: Int;`). The two backends diverge on uninitialized reads — the C backend emits a scalar-zero default while the LLVM backend emits no store at all — so the semantic layer rejects this form at the binding site. Class/subject fields use a distinct parser path (`ClassField`) and are not affected.

- **Reason**: backend divergence on uninitialized reads.
- **Fix**: initialize at the binding (`let x: T = ...;`) or use a conditional initializer expression.
- **cause_ir**: `semantic:let:uninit_local_binding`
- **fix_source**: `initialize-at-binding`

See `docs/93_codegen_idiom_audit.md` for the full backend parity rationale.

#### `PGY_SEM_MISSING_RETURN`

Non-`Void` function body has at least one reachable normal CFG path that can fall through without returning a value.

- **Reason**: the CFG body summary contains a reachable path without a return terminator.
- **Fix**: add a return on every branch/path, or change the function return type to `Void` if falling through is intended.
- **cause_ir**: `semantic:cfg:missing_return_path`
- **fix_source**: `add-return-on-all-paths`

#### `PGY_SEM_UNREACHABLE_CODE`

Statement appears after a CFG terminator (`return`, `break`, or `continue`) in
the same block and has no reachable normal entry edge.

- **Reason**: the CFG body summary has no reachable normal edge to the statement.
- **Fix**: remove the statement or move it before the terminator if it must execute.
- **cause_ir**: `semantic:cfg:unreachable_statement`
- **fix_source**: `remove-or-move-before-terminator`

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

#### `PGY_SEM_PIN_ESCAPE`

Pin/Lease view escapes the lexical pin scope: return, outer assignment,
collection storage, callback capture, channel send, or task capture.

- **Reason**: a pinned view is a scoped capability lease and cannot outlive the
  compiler-owned unpin edge.
- **Fix**: keep the view inside the `pin` block; copy/project the required value
  before leaving the block.
- **cause_ir**: `semantic:pin:escape`
- **fix_source**: `keep-pin-view-local`

#### `PGY_SEM_PIN_PARALLEL_CONFLICT`

Two parallel tasks attempt incompatible access to the same pinned slot, a
`WriteView<T>` overlaps with another pin/read/write path, direct owner
read/write bypasses a live view, or an owning slot is released/moved while a
`ReadView<T>` / `WriteView<T>` over that source is still live. Slot assignment
and value-position slot sugar are routed through the same owner read/write
rules. Passing the owning slot to an `own`/`ref Slot<T>` helper, returning it,
or forwarding it through an array literal or stable collection-store helper
while a typed view is live is also rejected.

- **Reason**: `WriteView<T>` is the aliasing-XOR-mutability baseline for
  Pin/Lease.
- **Fix**: serialize the pinned access, split the slot per task, end the
  pin/view scope before `Release(slot)` / `Move(slot)`, or use a
  channel/snapshot boundary.
- **cause_ir**: `semantic:pin:parallel_conflict`
- **fix_source**: `serialize-pin-access`

#### `PGY_SEM_PIN_AWAIT_BOUNDARY`

`await` appears while a pinned view is live.

- **Reason**: suspension would extend the lease across an async boundary where
  cleanup and authority ownership are not local.
- **Fix**: end the pin block before `await`, or move the awaited work outside
  the pinned access region.
- **cause_ir**: `semantic:pin:await_boundary`
- **fix_source**: `end-pin-before-await`

#### `PGY_SEM_PIN_QUBIT_REJECT`

Attempt to pin a `QubitSlot`.

- **Reason**: qubit ownership and observation semantics cannot expose a stable
  typed memory view.
- **Fix**: do not pin qubit resources; use the quantum operation surface.
- **cause_ir**: `semantic:pin:qubit_reject`
- **fix_source**: `do-not-pin-qubit`

#### `PGY_SEM_PIN_TOKEN_INVALID`

Secure slot pin fails token/capability validation.

- **Reason**: `SecureSlot<T>` pinning is a capability lease and requires a valid
  token for the requested read/write mode.
- **Fix**: provide a valid token with the required capability, or use a
  non-secure slot path if security is not required.
- **cause_ir**: `semantic:pin:token_invalid`
- **fix_source**: `provide-valid-pin-token`

#### `PGY_SEM_DEFER_DYNAMIC_CONTROL`

`defer` appears inside runtime-dependent `if`, `match`, `while`, or repeated
`for` control. Static control forms remain accepted, but dynamic control needs
a runtime defer stack before the compiler can guarantee cleanup executes only
for paths that actually entered the deferred region.

- **Reason**: lexical C/LLVM lowering can otherwise produce a false parity state
  where both backends run the same wrong cleanup.
- **Fix**: move the `defer` outside the dynamic control, make the control
  compile-time static, or wait for the runtime defer stack contract.
- **cause_ir**: `semantic:defer:dynamic_control`
- **fix_source**: `move-defer-outside-dynamic-control`

#### `PGY_SEM_RAW_ESCAPE_UNSTABLE`

Attempt to use a system-tier raw escape surface such as `SlotRawPointer(...)`.
The `unsafe { ... }` keyword exists as a lexical marker, but it is not a
beta-stable raw pointer / inline-assembly / MMIO contract.

- **Reason**: raw escape needs explicit syntax, semantic gates, ABI lowering,
  diagnostics, and determinism tests before it can be accepted.
- **Fix**: use typed Pin/Lease views for repeated slot access, or wait for the
  raw escape contract.
- **cause_ir**: `semantic:raw_escape:unstable`
- **fix_source**: `use-pin-or-wait-for-raw-escape-contract`

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

Inside a `parallel` block, two or more tasks mutate or release the **same** slot. This is a hard error for the covered same-slot conflict subset.

- **Cause IR**: `semantic:parallel:resource_conflict`
- **Reason**: owning writes across tasks must be disjoint for the stable subset; identity and alias analysis both trace to the same slot.
- **Fix**: split the slot into per-task slots; use a `Channel<T>` to serialize writes; or move the write outside the parallel block.

#### `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (warning)

Inside a `parallel` block, one task reads while another mutates or releases the same slot. Not a hard error (reader may be semantically fine in your domain), but flagged for review.

- **Reason**: read can observe partial/invalid state.
- **Fix**: synchronize via `Channel<T>`, take a snapshot before the parallel block, or demote the read to a `ReadView` acquired before the writer runs.

#### `PGY_SEM_EFFECT_CONFLICT` (warning)

Function body joins effect classes that the current partial-order treats as incompatible (typically `SECURE` + `REMOTE`/`COLLAPSE`/`NONDETERMINISTIC`).

- **Reason**: effect mask closure on the body produces a combination that the lattice flags as authority-sensitive work mixed with boundary/resource work.
- **Fix**: split the routine so each helper owns one effect family; isolate the conflicting branch behind an explicit boundary helper.

#### `PGY_SEM_PARALLEL_SECURE_FORBIDDEN`

A capability-bearing operation (SecureSlot read/write/release, DeviceSlot access, or a secure-effect-tagged function/method call) appears inside a `parallel` block. Capability scheduling requires a serialized call-site so the capability identity is not aliased across tasks.

- **Reason**: secure/device operations carry implicit capability state that is not safe to interleave at task-level granularity.
- **Fix**: hoist the operation to before/after the parallel block, or narrow the parallel block so it excludes the capability-bearing step.

### Declaration Uniqueness

#### `PGY_SEM_REDECLARATION`

A symbol (variable, function, class, ability, role, party, roster, world, world state, zone state, domain slot, intent, etc.) is declared twice in the same scope.

- **Reason**: scope permits only one definition per name; a second declaration is ambiguous.
- **Fix**: rename one of the declarations, or remove the earlier/later one if it was unintended.

### Borrow / Escape

#### `PGY_SEM_BORROW_ESCAPE`

A value bound as a `ref` parameter (borrowed boundary value / subject / movable resource / slot) attempts to escape the current function through a path that would outlive or alias the borrow: new `let` binding, `return`, assignment rebind, array literal, channel send, constructor field store, or downstream helper call.

- **Reason**: borrowed references do not transfer ownership; allowing them to leave through a storing/forwarding path would create a second observable binding for the same identity.
- **Fix**: (a) change the parameter to `own` if transfer is actually intended, (b) project/clone into a fresh value before the escaping store, or (c) keep the mutation local and move the store before/after the borrow.

### Domain Contracts

#### `PGY_SEM_INTENT_STEP_INVALID`

Intent step declaration violates one or more contract requirements: missing `where: <Zone>` when no zone is inferable; transfer clauses referencing unknown aliases; `who` participants that do not bind to a subject type or a matching zone slot; required abilities unsatisfied by the participant's role impl; effects not declared in the zone contract; authority requirements violated (missing `authorized by`, unknown authorized participant, wrong type, non-authority slot, or ambiguous same-type slot mapping); forbidden control-flow in contract clauses.

- **Reason**: intent step is over-constrained by the combined zone / action / participant / effect contracts and the declaration is inconsistent with that combination.
- **Fix**: cross-check the zone's subject slots, ability impls, effect slots, and authority rules; align the step's `where`/`using`/`who`/`authorized by`/`causes` clauses accordingly, or adjust the matching action contract.

#### `PGY_SEM_INTENT_BOUNDARY_DRIFT`

AIR verification detects that an intent step's declared abstraction contract and the implementation boundary it crosses disagree on sync/async behavior. The baseline Phase 1 case is a sync intent step mapped to an async boundary, or an async intent step mapped to a sync-only boundary.

- **Reason**: intent orchestration is a user-facing abstraction boundary; if the step contract and body boundary disagree, observability, compensation, and authority handoff can no longer be explained consistently.
- **Fix**: align the intent step contract with the actual boundary (`align-intent-boundary-sync`), or move the implementation through a boundary with the declared sync class.

#### `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`

AIR strict-evidence mode detected that an intent boundary has no matching RIR boundary evidence, or an authority-required boundary has no matching RIR authority evidence.

- **Reason**: AIR drift checks are only trustworthy when the declared boundary can be reconciled with the runtime/resource IR evidence that will explain authority and boundary behavior.
- **Fix**: align the intent boundary with a zone/world boundary that lowers into RIR evidence (`align-intent-boundary-evidence`), or extend AIR/RIR synthesis if the boundary is valid but not yet represented.
- **cause_ir**: `PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE`
- **fix_source**: `PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE`

#### `PGY_AIR_INVARIANT_INVALID`

AIR verifier detected an invalid compiler IR inventory before MIR lowering:
missing backing arrays for non-zero AIR counts, inconsistent boundary-to-intent
step indexing, malformed authority participant lists, or evidence flags without
the required layered provenance.

- **Reason**: AIR is a compiler verification graph. If its own inventory is
  malformed, the compiler must stop before user-facing drift diagnostics or
  backend lowering can trust it.
- **Fix**: report a compiler bug with the source that produced the invalid AIR
  graph (`report-compiler-bug`).
- **cause_ir**: `PGY_CAUSE_AIR_INVARIANT_INVALID`
- **fix_source**: `PGY_FIX_REPORT_COMPILER_BUG`

#### `PGY_SEM_ACTION_CONTRACT_INVALID`

`action` declaration references undeclared or non-exported zone/effect/subject; `authorized by` refers to a non-`self`/non-parameter name; authorized subject type has no matching slot in the target zone; causes an effect with no matching effect slot; or declares a zone-bound effect without `authorized by`.

- **Reason**: action contract surface must be consistent with every zone/effect it is visible from; any cross-reference must be exportable.
- **Fix**: import/export the referenced declarations; align subject/effect types with the zone's slots; add `authorized by` when the action causes authority-sensitive effects.

#### `PGY_SEM_ABILITY_CONTRACT_INVALID`

`ability` declaration or `requires`/`impl` site uses wrong generic arity, invalid generic argument, duplicate or non-exported `fields` entries.

- **Reason**: ability is part of a role's public contract; its surface must be well-formed so implementers and consumers can type-check without ambiguity.
- **Fix**: adjust the generic arity/bounds, remove duplicate `fields`, export referenced types, or drop the clause if not needed.

#### `PGY_SEM_ROLE_CONTRACT_INVALID`

`role` declaration cannot implement a required ability because the subject is missing a required field or has an incompatible field type; includes an unknown role; references a non-exported ability from another module; fails generic bound on an included role.

- **Reason**: role-based dispatch relies on the subject + role combination exposing the exact signature the ability prescribes.
- **Fix**: add/rename the missing field, align the field type, import the referenced role/ability, or relax the generic bound.

#### `PGY_SEM_CLASS_CONTRACT_INVALID`

`class` declaration or instantiation site cannot validate a `where`-clause or resolve a generic argument (either at instantiation or specialization time).

- **Reason**: class generic bounds are part of the type; an unsatisfied bound makes the specialization unsound.
- **Fix**: provide a type argument that satisfies the bound, or widen the bound if the original constraint was overly tight.

#### `PGY_SEM_ZONE_CONTRACT_INVALID`

`zone` / `world` declaration or site violates one of the zone surface contracts: authority references unknown/non-subject slot; authority declared twice for one slot; layer/state/apply/link/detach/unlink/maintain clause refers to missing slot or wrong kind; zone projection misses required target field; zone-state transition is malformed; etc.

- **Reason**: zone is a capability boundary; every clause inside it must point to a slot/state the zone declares, otherwise routing and authority checks cannot be validated.
- **Fix**: check slot/state naming and kinds; import/export referenced zones; align the authority ability set with the subject's role; remove or repair the offending clause.

#### `PGY_SEM_WORLD_CONTRACT_INVALID`

`world` declaration or composed-world state contract references unknown or forward-defined zone/state; world state pulls from derived state where only zone sources are allowed; world activate/deactivate/maintain references unknown zone slot; world constructor implicit-copies a zone binding that should be moved/cloned; `HasZone(...)` predicate on unknown zone.

- **Reason**: world is a composition of zones and their states; every name in a clause must resolve to a slot/state the world already declares at that point.
- **Fix**: declare or reorder the referenced zones/states; switch to the underlying zone slot instead of a derived alias; make the copy explicit via `Clone(...)` where unavoidable.

### Loop Control

#### `PGY_SEM_LOOP_CONTROL_INVALID`

`break` or `continue` used outside a loop, or targets a label that does not name an enclosing loop.

- **Reason**: loop control needs an enclosing loop for scope; unknown labels cannot be resolved.
- **Fix**: move the statement inside a loop, or fix the label to match an enclosing labeled loop.

### Builtins

#### `PGY_SEM_BUILTIN_ARGS_INVALID`

A built-in intrinsic (`Rc*`, `Weak*`, `Box*`, `Move`, `Clone`, `BoxArray`, ...) was called with the wrong number or kind of arguments: wrong arity, argument is not the expected generic handle type, non-owning binding where an owning slot is required.

- **Reason**: built-in intrinsics have fixed signatures and cannot accept overloads; their arguments must match exactly.
- **Fix**: match the documented intrinsic signature; construct the expected
  wrapper type (`Rc<T>`, `Weak<T>`, `Box<T>`, owning `Slot<T>`) before the
  call. For beta-stable `Box<T>`, do not use a resource handle payload; box a
  copied/passive value instead.

#### `PGY_SEM_PREDICATE_ARGS_INVALID`

`HasState`, `HasProjection`, `HasZone`, `HasLayer` (or zone-layered variants) called with wrong argument shape, in the wrong host scope (e.g. `HasState` outside a zone body), or with an identifier/literal that does not resolve to a declared name.

- **Reason**: these predicates are compiled against the enclosing host's declared slots; they must be used inside a host that has those slots, with a name that matches one of them.
- **Fix**: move the call into the right host scope (zone/world/relation/effect); check the slot/state name; use a string literal or identifier that the host declares.

### Event / Remote / Anchored

#### `PGY_SEM_EVENT_CONTRACT_INVALID`

`event` declaration parameter is missing a type annotation, or an `event` invoke site passes the wrong argument count.

- **Reason**: event signatures bridge publisher and subscriber; both sides must agree on types and arity.
- **Fix**: add explicit parameter types on the declaration; align invoke-site argument count with the signature.

#### `PGY_SEM_REMOTE_FUTURE_MISUSE`

`Write(...)`, `Read(...)`, or `Release(...)` called on a `RemoteFuture<T>`. Remote resources are not accessible in place — they are consumed by `await`.

- **Reason**: `RemoteFuture<T>` is a one-shot result, not a slot; direct access does not have a well-defined semantic.
- **Fix**: `await` the future to obtain the `Result<T, E>` and operate on that.

#### `PGY_SEM_ANCHORED_HANDLE_COPY`

An anchored resource handle (`Slot<T>`/`SecureSlot<T>`/`DeviceSlot<T>`) was bound into a new `let` without moving; the handle identity would now be duplicated.

- **Reason**: anchored handles are unique per resource; copying them creates two observable bindings for the same resource.
- **Fix**: use `Move(slot)` to transfer, or keep the original binding and work through it.

#### `PGY_SEM_CHANNEL_TRANSPORT_INVALID`

A channel send/receive builtin (e.g. `TrySend`/`TryRecv`/`ch <- value`) violates the channel transport contract: the channel element type does not agree with the value being sent, an anchored/capability-bearing handle is being shipped across the channel, a movable resource is being sent non-blockingly, or the send expression is not a named binding.

- **Reason**: the channel boundary is where ownership and capability provenance are reconciled; sending an anchored or capability-bearing handle would break slot uniqueness or leak authority, and unnamed sends make the moved-here source ambiguous. Type mismatches at the boundary leave the receiver with a wrong-shape value.
- **Fix family** (by `fix_source`):
  - `align-channel-element-type` — channel element type vs. sent/received value disagree; change one to match the other.
  - `keep-handle-local-or-send-inner-value` — an anchored (`Slot`/`SecureSlot`/`DeviceSlot`) or capability-bearing value is being transported; send the inner scalar value or keep the handle local to the authorized flow.
  - `bind-to-named-variable-before-send` — the send expression is unnamed; bind it in a local first so ownership transfer has one concrete source.

### Type Resolution

#### `PGY_SEM_TYPE_DEPENDENCY_CYCLE`

A type, alias, or domain-decl forms a resolution cycle — the graph has an edge A → B → ... → A where each step is a type-usage dependency.

- **Reason**: resolving the cycle would require knowing one of its members before it is itself resolved; nominal types cannot be computed that way.
- **Fix**: break the cycle by introducing an indirection (boxing via `Rc<T>` / `Box<T>`, or a named alias that does not participate in the cycle), or restructure the types so that one is expressed in terms of another without the back-reference.

### Match / Select

#### `PGY_SEM_MATCH_PATTERN_INVALID`

A `match` pattern is malformed or the subject is outside the beta-stable match
surface. Stable subjects are `Int`, `Long`, `Bool`, enum, `Option<T>`, and
`Result<T, E>`.

- **Reason**: pattern bindings must line up exactly with the value's shape, and
  backend lowering must not rely on local equality assumptions for unsupported
  subject types.
- **Fix**: remove extra payload bindings on `None`, align enum payload arity
  with the declaration, import/declare the enum before matching on it, or
  rewrite unsupported subject matches as explicit conditionals until the subject
  type has a stable equality contract.

#### `PGY_SEM_SELECT_CASE_INVALID`

A `select { case ... : ... }` arm does not start with a channel-receive pattern.

- **Reason**: `select` multiplexes channel readiness; each case must anchor on a `<-channel` receive or a named bind of one.
- **Fix**: rewrite the case to begin with `let v = <- ch` (or the bare receive for discard), or move the non-channel work out of the `select`.

### Operators

#### `PGY_SEM_UNOP_TYPE_MISMATCH`

Unary operator applied to an operand whose type does not satisfy the operator's domain: `!` on a non-`Bool`; unary `-` on a non-numeric type.

- **Reason**: unary operators have fixed domains; applying them to the wrong type would change the observable behavior silently.
- **Fix**: coerce or restructure to a value of the expected operand type, or use a different operator / builtin for the intent.

### Visibility / Immutability

#### `PGY_SEM_VISIBILITY_BOUNDARY`

A member access (`obj.field` or `obj.method`) crosses a `public`/`module-private`/`pub(zone)` visibility boundary the call site is not allowed to cross.

- **Reason**: visibility modifiers are enforced at semantic check time; the current module/zone/world is not in the set the member is exported to.
- **Fix**: widen the member's visibility, move the caller into the permitted scope, or expose a public accessor the outside caller is allowed to use.

#### `PGY_SEM_IMMUTABLE_FIELD_WRITE`

Assignment target is an immutable field: `object` field after construction; `tobject` field (always immutable); a bare slot-like resource handle that requires `Write(slot, ...)` instead of a reassignment.

- **Reason**: `object` and `tobject` host kinds freeze field values at construction; mutation would invalidate the identity/snapshot semantics those kinds carry.
- **Fix**: construct a fresh `object`/`tobject` with the updated field, or switch to a `subject`/`class`/`vessel` host if ongoing mutation is intended; for resource handles, use the `Write(...)` builtin.

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

## C Backend (`stage: "c_codegen"`)

These codes fire inside the C transpiler (MIR → C source) before the native C compiler is invoked. JSON stage is always `"c_codegen"`.

#### `PGY_C_TYPE_UNSUPPORTED`

The C transpiler does not yet lower the type / builtin / nominal shape encountered at this site. Covers "C backend: unsupported X", "cannot render ability/party/role", "cannot determine slot/lambda/element/parameter type", "cannot resolve included role / party / ability", "cannot derive Result<T, E> specialization", "too many tuple specializations".

- **Reason**: the C backend's lowering library has not been extended to cover this MIR/domain shape. LLVM backend may support it (the two backends are not perfectly symmetric).
- **Fix**: prefer `--backend=llvm` for the affected program, or add the missing lowering path to the C transpiler (if extending pergyra itself).

## LLVM Backend (`stage: "llvm_codegen"`)

These codes fire inside the LLVM backend before the native compiler (clang/gcc) is invoked. JSON stage is always `"llvm_codegen"`.

#### `PGY_LLVM_SPEC_LIMIT`

Too many distinct `Result<T, E>` specializations in one translation unit (internal limit `MAX_LLVM_RESULT_SPECS`). Each unique `(T, E)` pair costs one specialization slot.

- **Reason**: either real fan-out across many error types, or accidental re-specialization due to type name normalization drift.
- **Fix**: collapse error types (reuse a shared enum), raise the limit, or audit `llvm_ensure_result_spec` for duplicate-detection regressions.

#### `PGY_LLVM_TYPE_UNSUPPORTED`

The LLVM backend cannot emit the requested type/operation: `Result<...>` inner type resolution failed; standalone `let` for a slot-like type lacks a concrete annotation; `DeviceWrite`/`DeviceRead`/`ReleaseDeviceSlot`/`SubmitDeviceRead` on a slot whose inner type is not concrete; destructure source is not Array-like/tuple; `ClaimDeviceSlot` let without `DeviceSlot<T>` annotation.

- **Reason**: LLVM monomorphization requires a known concrete type at emission time; type inference could not pin it down from annotations.
- **Fix**: add an explicit `let x: <Type> = ...` annotation, or restructure the source so the type is derivable from context.
- **Additional fix token**: `align-result-error-type` is used when postfix `?` cannot coerce the callee's `Result<T, E>` error payload into the current function's declared error payload type. Align the function return type with the propagated `E`, or map the error explicitly before using `?`.

#### `PGY_LLVM_MIR_ROUTINE_MISSING`

MIR-only path expected a routine or top-level wrapper that was not emitted: e.g. the `__pgy_top_level_exec` wrapper needed to execute the program entry.

- **Reason**: MIR lowering dropped a required function-level artifact; the LLVM walker cannot proceed without it.
- **Fix**: inspect the MIR inventory for the missing routine; verify the HIR → MIR pipeline emitted it.

#### `PGY_LLVM_SCOPE_LIMIT`

LLVM's per-module scope/variable registry exceeded an internal capacity: scope depth > max, or too many variables in one scope.

- **Reason**: the current function/module uses more nested scopes or local variables than the static buffer handles.
- **Fix**: refactor the function into helpers, or raise the limit in `llvm_registry.c`.

#### `PGY_LLVM_OOM`

Heap allocation failed while growing an internal LLVM-backend data structure (e.g. `generic_templates`).

- **Reason**: runtime memory exhaustion.
- **Fix**: reduce the translation unit size, or run on a machine with more memory.

## Routing Guidance for AI Consumers

- Check `code` first; fall back to `stage` + message pattern if `code` is absent.
- Codes are stable across versions — safe to hard-code in auto-fix heuristics.
- Message text may change — do not regex the message unless you have to.
- Same `code` at different locations shares the same fix family; different locations may have distinct concrete fixes (e.g. `PGY_SEM_SLOT_RELEASED` on direct Write vs via ReadView/WriteView).

## Future Extensions

- `cause_ir` and `fix_source` fields are wired (see field reference at the top). Coverage is partial: a handful of representative sites carry them today (type mismatch, slot lifecycle, view-through, LLVM spec limit). Remaining sites can be upgraded incrementally via `semantic_error_with_hints` / `llvm_set_error_with_hints`.
- Parser/lexer stage errors now carry stage codes in their message surface, and driver JSON routing preserves parse/lex `stage`, `code`, `cause_ir`, and `fix_source`. Remaining parser diagnostic work is richer multi-error accumulation and more precise parser-specific code splitting beyond the baseline `PGY_PARSE_SYNTAX`.
- `related_rules` field will link each code to the language reference spec once that document exists.
