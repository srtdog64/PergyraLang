# LLVM Option member-assignment context

Status: `RETIRED — NAMED BLOCKER REMOVED; PRODUCTION LANE REMAINS RED`

Exact base: `e19b15b2647664e4208f7d97e490318085f652d6` on
`origin/main`.

## Objective card

- Objective: make the current production compiler-purpose source compile to
  native LLVM when `self.outcome = Some(...)` assigns a typed
  `Option<DriverSourceLlvmIntentOutcome>` field, so the LLVM driver-body lane
  reaches fixture execution instead of stopping in the bootstrap compiler.
- Priority order: preserve the declared field layout; scope the context only
  across RHS emission; keep anonymous `Some` fail-closed; retain C/LLVM runtime
  parity; then minimize implementation and harness cost.
- Fact owner: the source class/subject field declaration materialized in the
  registered `LLVMClassTypeEntry` field layout. `llvm_emit_member_lvalue_ptr`
  exposes that exact target field type; the RHS must not reconstruct it from
  the constructor payload or field spelling.
- Last legitimate consumer: `llvm_emit_assignment_parts` scopes the registered
  target field type while `llvm_emit_result_option_call` materializes the
  concrete two-field `Option<T>` value immediately before the store.
- Forbidden fallback: an anonymous or payload-derived Option layout;
  `Option<Int>` defaulting; an `outcome`/`self` name special case; changing the
  Pergyra compiler source to hide `Some` behind a helper; using the C backend
  as the production result when LLVM fails; or retaining the assignment target
  context after RHS emission.
- Verification gate: a focused C/LLVM compile-and-run fixture assigns both
  branches of an `Option<nominal>` member and compares exact behavior; an
  uncontextual `Some` negative remains rejected without an artifact; the
  current filtered LLVM driver-body gate must then build the real compiler
  driver and reach its selected fixture.

## Reproduced executable gap

- Production entrypoint: native `pgy <driver_rung2_main.pgy>
  --native-pipeline --backend=llvm`, which builds the Pergyra compiler test
  driver containing the live `CompilePergyraProgram` source-to-LLVM purpose.
- Exact failure: `driver_source_llvm_intent_execution_owner.pgy:29:28` reports
  `LLVM Some(value) requires contextual Option<T>; anonymous Option layout
  fallback is disabled` before any selected body fixture executes.
- The C build of the same driver succeeds. This rung removes a bootstrap
  C/LLVM parity blocker; it does not delete a C compiler implementation and
  therefore does not claim a new hard `SUBSTITUTING` numerator or change the
  SoT census/project percentage.

## Edit scope and integration

- Allowed implementation: the existing LLVM member-assignment RHS emission
  seam and only the minimum declaration needed to scope/restore target type.
- Allowed tests: one positive member-assignment fixture, one contextless
  negative, one focused parity/ratchet gate, and its Make/CI inventory wiring.
- Allowed coordination/evidence: this directive, the top collaboration and
  handoff cards, and a bounded result audit after verification.
- Forbidden overlap: Pergyra source rewrites, general Option inference,
  semantic/MIR owner migration, local/return/call-argument contextual typing,
  class layout redesign, other LLVM gaps, and unrelated SoT rows.
- Integration owner: the primary task at this exact base.

This directive is temporary coordination state. It does not own language
semantics, registry status, progress percentage, or completion evidence.

## Bounded result

- The registered member field type now scopes only RHS emission, and the
  previous `driver_source_llvm_intent_execution_owner.pgy:29:28` contextual
  `Some` failure no longer occurs.
- `make self-host-llvm-option-member-assignment-context-test-smoke` passed with
  exact C/LLVM `41` / `true` behavior and the contextless-`Some` artifact
  negative. `build-source-inventory-test-smoke` and
  `ci-step-runner-test-smoke` also passed.
- The broader production gate did **not** reach fixture execution. It advanced
  to a distinct LLVM verifier failure: the `DriverSourceCRequest` intent value
  argument is emitted as `ptr` while the declared function parameter is the
  by-value `%DriverSourceCRequest` aggregate.
- Therefore this lease is retired only for the named member-assignment
  blocker. It does not close the production rung or satisfy the original
  end-to-end reachability objective; that exact ABI mismatch is the next
  falsifier and requires a new owner card.
